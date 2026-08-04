// SPDX-License-Identifier: MIT
// Adapted from SonyBridge commit bafa7d57609435cd823c528734d989f5b1f24e6d.
// See ../THIRD_PARTY_NOTICES.md for the upstream copyright and license.

#include <algorithm>
#include <array>

#include <mdr/Headphones.hpp>

namespace mdr
{
    namespace
    {
        // MDR v1 NC/ASM command layout imported from SonyBridge / Plutoberth.
        // Both upstream projects are MIT licensed.
        constexpr UInt8 kNcAsmGetParam = 0x66;
        constexpr UInt8 kNcAsmRetParam = 0x67;
        constexpr UInt8 kNcAsmSetParam = 0x68;
        constexpr UInt8 kNcAsmInquiredType = 0x02;

        constexpr UInt8 kNcAsmEffectOff = 0x00;
        constexpr UInt8 kNcAsmEffectAdjustmentComplete = 0x11;
        constexpr UInt8 kLevelAdjustment = 0x01;
        constexpr UInt8 kDualSingleOff = 0x00;
        constexpr UInt8 kDualSingleSingle = 0x01;
        constexpr UInt8 kDualSingleDual = 0x02;
        constexpr UInt8 kAsmNormal = 0x00;
        constexpr UInt8 kAsmVoice = 0x01;
        constexpr UInt8 kAsmLevelDisabled = 0xff;

        UInt8 dualSingleForLevel(UInt8 level)
        {
            if (level == 1)
                return kDualSingleSingle;
            if (level == 0)
                return kDualSingleDual;
            return kDualSingleOff;
        }
    }

    MDRTask MDRHeadphones::RequestInitV1()
    {
        mProtocol = {1, 0, 0};

        using Function = v2::MessageMdrV2FunctionType_Table1;
        mSupport.table1Functions[static_cast<UInt8>(
            Function::NOISE_CANCELLING_DUAL_SINGLE_OFF_AMBIENT_SOUND_MODE_LEVEL_ADJUSTMENT)] = true;

        co_await RequestSyncV1();
        co_return MDR_HEADPHONES_TASK_INIT_OK;
    }

    MDRTask MDRHeadphones::RequestSyncV1()
    {
        constexpr std::array<UInt8, 2> command{kNcAsmGetParam, kNcAsmInquiredType};

        int retries = 0;
        for (; retries < kAwaitAckRetries; ++retries)
        {
            SendCommandImpl(command, MDRDataType::DATA_MDR, mSeqNumber);
            const int result = co_await Await(AWAIT_ACK);
            if (result == MDR_RESULT_OK)
                break;
        }
        MDR_CHECK_MSG(retries != kAwaitAckRetries,
                      "Timeout exceeded waiting for legacy device ACK");

        const int response = co_await Await(AWAIT_NCASM_PARAM);
        MDR_CHECK_MSG(response == MDR_RESULT_OK,
                      "Timeout exceeded waiting for legacy NC/ASM state");
        co_return MDR_HEADPHONES_TASK_SYNC_OK;
    }

    MDRTask MDRHeadphones::RequestCommitV1()
    {
        if (!IsDirtyV1())
            co_return MDR_HEADPHONES_TASK_COMMIT_OK;

        const bool enabled = mNcAsmEnabled.desired;
        const bool ambient = enabled && mNcAsmMode.desired == v2::t1::NcAsmMode::ASM;
        const UInt8 level = !enabled
            ? kAsmLevelDisabled
            : ambient
                ? static_cast<UInt8>(std::clamp(mNcAsmAmbientLevel.desired, 1, 20))
                : 0;

        const std::array<UInt8, 8> command{
            kNcAsmSetParam,
            kNcAsmInquiredType,
            enabled ? kNcAsmEffectAdjustmentComplete : kNcAsmEffectOff,
            kLevelAdjustment,
            dualSingleForLevel(level),
            kLevelAdjustment,
            mNcAsmFocusOnVoice.desired ? kAsmVoice : kAsmNormal,
            level
        };

        int retries = 0;
        for (; retries < kAwaitAckRetries; ++retries)
        {
            SendCommandImpl(command, MDRDataType::DATA_MDR, mSeqNumber);
            const int result = co_await Await(AWAIT_ACK);
            if (result == MDR_RESULT_OK)
                break;
        }
        MDR_CHECK_MSG(retries != kAwaitAckRetries,
                      "Timeout exceeded waiting for legacy device ACK");

        mNcAsmEnabled.commit();
        mNcAsmFocusOnVoice.commit();
        mNcAsmAmbientLevel.commit();
        mNcAsmMode.commit();
        co_return MDR_HEADPHONES_TASK_COMMIT_OK;
    }

    int MDRHeadphones::HandleCommandV1(Span<const UInt8> cmd, MDRCommandSeqNumber)
    {
        if (cmd.size() < 2
            || cmd[0] != kNcAsmRetParam
            || cmd[1] != kNcAsmInquiredType)
        {
            return MDR_HEADPHONES_EVT_UNHANDLED;
        }

        if (cmd.size() < 8)
            return MDR_HEADPHONES_EVT_UNHANDLED;

        const bool enabled = cmd[2] != kNcAsmEffectOff;
        const UInt8 rawLevel = cmd[7];
        const bool ambient = enabled
            && rawLevel != kAsmLevelDisabled
            && rawLevel > 0;

        mNcAsmEnabled.overwrite(enabled);
        mNcAsmMode.overwrite(ambient ? v2::t1::NcAsmMode::ASM : v2::t1::NcAsmMode::NC);
        mNcAsmFocusOnVoice.overwrite(cmd[6] == kAsmVoice);
        if (ambient)
            mNcAsmAmbientLevel.overwrite(std::clamp<int>(rawLevel, 1, 20));

        Awake(AWAIT_NCASM_PARAM);
        return MDR_HEADPHONES_EVT_NCASM_PARAM;
    }
}
