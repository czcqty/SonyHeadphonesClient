#include <QtTest>

#include <mdr/Command.hpp>
#include <mdr/Headphones.hpp>

#include <algorithm>
#include <array>
#include <cstring>
#include <deque>
#include <utility>

namespace
{
struct LifetimeProbe
{
    int* liveCount;

    explicit LifetimeProbe(int& count) : liveCount(&count) { ++*liveCount; }
    LifetimeProbe(const LifetimeProbe& other) : liveCount(other.liveCount)
    {
        ++*liveCount;
    }
    LifetimeProbe(LifetimeProbe&& other) noexcept
        : liveCount(std::exchange(other.liveCount, nullptr))
    {
    }
    ~LifetimeProbe()
    {
        if (liveCount)
            --*liveCount;
    }
};

mdr::MDRTask makePendingTask(LifetimeProbe probe)
{
    co_return MDR_HEADPHONES_IDLE;
}

struct LegacyFakeTransport
{
    MDRConnection connection{};
    std::deque<mdr::MDRBuffer> receiveQueue;
    mdr::MDRBuffer lastDataCommand;

    LegacyFakeTransport()
    {
        connection.user = this;
        connection.connect = &connect;
        connection.disconnect = &disconnect;
        connection.recv = &recv;
        connection.send = &send;
        connection.poll = &poll;
        connection.getDevicesList = &getDevicesList;
        connection.freeDevicesList = &freeDevicesList;
        connection.getLastError = &getLastError;
    }

    static LegacyFakeTransport& self(void* user)
    {
        return *static_cast<LegacyFakeTransport*>(user);
    }

    static int connect(void*, const char*, const char*) { return MDR_RESULT_OK; }
    static void disconnect(void*) {}

    static int recv(void* user, char* dst, int size, int* received)
    {
        auto& transport = self(user);
        if (transport.receiveQueue.empty())
        {
            *received = 0;
            return MDR_RESULT_INPROGRESS;
        }

        auto packet = std::move(transport.receiveQueue.front());
        transport.receiveQueue.pop_front();
        const int amount =
            std::min<int>(size, static_cast<int>(packet.size()));
        std::memcpy(dst, packet.data(), amount);
        *received = amount;
        return MDR_RESULT_OK;
    }

    static int send(void* user, const char* src, int size, int* sent)
    {
        auto& transport = self(user);
        const auto* begin = reinterpret_cast<const mdr::UInt8*>(src);
        // The sender may batch a notification ACK with the next command.
        // Consume one complete frame and leave the remainder for the next send.
        const auto* end = std::find(begin, begin + size, mdr::kEndMarker);
        if (end == begin + size)
            return MDR_RESULT_ERROR_GENERAL;
        const auto frameSize = static_cast<size_t>(end - begin + 1);
        mdr::MDRBuffer payload;
        mdr::MDRDataType type{};
        mdr::MDRCommandSeqNumber seq{};
        const auto result = mdr::MDRUnpackCommand(
            {begin, frameSize}, payload, type, seq);
        if (result != mdr::MDRUnpackResult::OK)
            return MDR_RESULT_ERROR_GENERAL;

        *sent = static_cast<int>(frameSize);
        if (type != mdr::MDRDataType::DATA_MDR)
            return MDR_RESULT_OK;

        transport.lastDataCommand = payload;
        transport.receiveQueue.push_back(
            mdr::MDRPackCommand(mdr::MDRDataType::ACK, 1 - seq, {}));

        if (payload.size() == 2 && payload[0] == 0x66 && payload[1] == 0x02)
        {
            constexpr std::array<mdr::UInt8, 8> state{
                0x67, 0x02, 0x11, 0x01, 0x00, 0x01, 0x01, 0x08
            };
            transport.receiveQueue.push_back(
                mdr::MDRPackCommand(mdr::MDRDataType::DATA_MDR, seq, state));
        }
        return MDR_RESULT_OK;
    }

    static int poll(void*, int) { return MDR_RESULT_OK; }
    static int getDevicesList(void*, MDRDeviceInfo**, int*)
    {
        return MDR_RESULT_ERROR_NOT_SUPPORTED;
    }
    static int freeDevicesList(void*, MDRDeviceInfo**)
    {
        return MDR_RESULT_ERROR_NOT_SUPPORTED;
    }
    static const char* getLastError(void*)
    {
        return "legacy fake transport";
    }
};

int pollUntil(mdr::MDRHeadphones& device, int expected)
{
    int result = MDR_HEADPHONES_IDLE;
    for (int i = 0; i < 20; ++i)
    {
        result = device.PollEvents();
        if (result == expected || result == MDR_HEADPHONES_ERROR)
            break;
    }
    return result;
}
}

class BackendTests final : public QObject
{
    Q_OBJECT

private slots:
    void taskMoveAssignmentReleasesPreviousFrame();
    void legacyProtocolInitializesFromCommunityPacket();
    void legacyProtocolSerializesAmbientSetting();
    void multipointConfirmationRequiresMatchingRecentWrite();
};

void BackendTests::taskMoveAssignmentReleasesPreviousFrame()
{
    int liveCount = 0;
    {
        auto first = makePendingTask(LifetimeProbe(liveCount));
        auto second = makePendingTask(LifetimeProbe(liveCount));
        QCOMPARE(liveCount, 2);

        first = std::move(second);
        QCOMPARE(liveCount, 1);
        QVERIFY(!second);
    }
    QCOMPARE(liveCount, 0);
}

void BackendTests::legacyProtocolInitializesFromCommunityPacket()
{
    LegacyFakeTransport transport;
    mdr::MDRHeadphones device(&transport.connection);

    QCOMPARE(device.Invoke(device.RequestInitV1()), MDR_RESULT_OK);
    QCOMPARE(pollUntil(device, MDR_HEADPHONES_TASK_INIT_OK),
             MDR_HEADPHONES_TASK_INIT_OK);
    QCOMPARE(device.mProtocol.version, 1);
    QVERIFY(device.mNcAsmEnabled.current);
    QCOMPARE(device.mNcAsmMode.current, mdr::v2::t1::NcAsmMode::ASM);
    QCOMPARE(device.mNcAsmAmbientLevel.current, 8);
    QVERIFY(device.mNcAsmFocusOnVoice.current);

    using Function = mdr::v2::MessageMdrV2FunctionType_Table1;
    QVERIFY(device.mSupport.contains(
        Function::NOISE_CANCELLING_DUAL_SINGLE_OFF_AMBIENT_SOUND_MODE_LEVEL_ADJUSTMENT));
}

void BackendTests::legacyProtocolSerializesAmbientSetting()
{
    LegacyFakeTransport transport;
    mdr::MDRHeadphones device(&transport.connection);
    device.mProtocol.version = 1;
    device.mNcAsmEnabled.overwrite(false);
    device.mNcAsmMode.overwrite(mdr::v2::t1::NcAsmMode::NC);
    device.mNcAsmAmbientLevel.overwrite(8);
    device.mNcAsmFocusOnVoice.overwrite(false);

    device.mNcAsmEnabled.desired = true;
    device.mNcAsmMode.desired = mdr::v2::t1::NcAsmMode::ASM;
    device.mNcAsmAmbientLevel.desired = 7;
    device.mNcAsmFocusOnVoice.desired = true;

    QCOMPARE(device.Invoke(device.RequestCommitV1()), MDR_RESULT_OK);
    QCOMPARE(pollUntil(device, MDR_HEADPHONES_TASK_COMMIT_OK),
             MDR_HEADPHONES_TASK_COMMIT_OK);

    const mdr::MDRBuffer expected{
        0x68, 0x02, 0x11, 0x01, 0x00, 0x01, 0x01, 0x07
    };
    QCOMPARE(transport.lastDataCommand, expected);
    QVERIFY(!device.IsDirtyV1());
}

void BackendTests::multipointConfirmationRequiresMatchingRecentWrite()
{
    using namespace mdr::v2;
    using namespace mdr::v2::t1;
    LegacyFakeTransport transport;
    mdr::MDRHeadphones device(&transport.connection);
    device.mProtocol.version = 2;
    device.mSupport.table1Functions[int(MessageMdrV2FunctionType_Table1::FIXED_MESSAGE)] = true;
    device.mSupport.table1Functions[int(MessageMdrV2FunctionType_Table1::GENERAL_SETTING_2)] = true;
    device.mGsCapability2.value.subject.value = "MULTIPOINT_SETTING";
    auto alert = [&](AlertMessageType message) {
        const mdr::MDRBuffer payload{mdr::UInt8(Command::ALERT_NTFY_PARAM),
            mdr::UInt8(AlertInquiredType::FIXED_MESSAGE), mdr::UInt8(message),
            mdr::UInt8(AlertActionType::POSITIVE_NEGATIVE)};
        transport.receiveQueue.push_back(mdr::MDRPackCommand(mdr::MDRDataType::DATA_MDR, 0, payload));
        QCOMPARE(pollUntil(device, MDR_HEADPHONES_EVT_ALERT), MDR_HEADPHONES_EVT_ALERT);
    };
    const auto multipoint = AlertMessageType::DISCONNECT_CAUSED_BY_CHANGING_MULTIPOINT;
    alert(multipoint);
    QVERIFY(!device.mMultipointConfirmationPending); // Unsolicited alert.
    device.mGsParamBool2.desired = true;
    QCOMPARE(device.Invoke(device.RequestCommitV2()), MDR_RESULT_OK);
    QCOMPARE(pollUntil(device, MDR_HEADPHONES_TASK_COMMIT_OK), MDR_HEADPHONES_TASK_COMMIT_OK);
    const mdr::MDRBuffer setting{mdr::UInt8(Command::GENERAL_SETTING_SET_PARAM), 0xD2, 0, 0};
    QCOMPARE(transport.lastDataCommand, setting);
    alert(AlertMessageType::NEED_DISCONNECTION_FOR_UPDATING_FIRMWARE);
    QVERIFY(!device.mMultipointConfirmationPending); // Never confirm unrelated actions.
    alert(multipoint);
    QVERIFY(device.mMultipointConfirmationPending);
    QVERIFY(device.IsDirty());
    QCOMPARE(device.Invoke(device.RequestCommitV2()), MDR_RESULT_OK);
    QCOMPARE(pollUntil(device, MDR_HEADPHONES_TASK_COMMIT_OK), MDR_HEADPHONES_TASK_COMMIT_OK);
    const mdr::MDRBuffer reply{mdr::UInt8(Command::ALERT_SET_PARAM), 0, 7, 1};
    QCOMPARE(transport.lastDataCommand, reply);
    QVERIFY(!device.mMultipointConfirmationPending);
    alert(multipoint);
    QVERIFY(!device.mMultipointConfirmationPending); // One response per request.
    device.mMultipointChangeUntil = std::chrono::steady_clock::now() - std::chrono::seconds(1);
    alert(multipoint);
    QVERIFY(!device.mMultipointConfirmationPending);
}

QTEST_APPLESS_MAIN(BackendTests)

#include "BackendTests.moc"
