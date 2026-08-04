#include "MdrSession.hpp"

#include <QMetaObject>
#include <QVariantMap>

#include <exception>
#include <string>

#include <mdr-c/Base.h>
#include <mdr-c/Connection.h>

#if defined(_WIN32)
#include <mdr-c/Platform/PlatformWindows.h>
#elif defined(__linux__)
#include <mdr-c/Platform/PlatformLinux.h>
#elif defined(__APPLE__)
#include <mdr-c/Platform/PlatformMacOS.h>
#endif

struct PlatformConnection
{
    void* platform = nullptr;
    MDRConnection* connection = nullptr;

    bool create()
    {
#if defined(_WIN32)
        platform = mdrConnectionWindowsCreate();
        if (platform)
            connection = mdrConnectionWindowsGet(static_cast<MDRConnectionWindows*>(platform));
#elif defined(__linux__)
        platform = mdrConnectionLinuxCreate();
        if (platform)
            connection = mdrConnectionLinuxGet(static_cast<MDRConnectionLinux*>(platform));
#elif defined(__APPLE__)
        platform = mdrConnectionMacOSCreate();
        if (platform)
            connection = mdrConnectionMacOSGet(static_cast<MDRConnectionMacOS*>(platform));
#endif
        return connection != nullptr;
    }

    ~PlatformConnection()
    {
        if (!platform)
            return;
#if defined(_WIN32)
        mdrConnectionWindowsDestroy(static_cast<MDRConnectionWindows*>(platform));
#elif defined(__linux__)
        mdrConnectionLinuxDestroy(static_cast<MDRConnectionLinux*>(platform));
#elif defined(__APPLE__)
        mdrConnectionMacOSDestroy(static_cast<MDRConnectionMacOS*>(platform));
#endif
    }
};

MdrSession::MdrSession(DeviceState* state, QObject* parent)
    : QObject(parent),
      m_stateData(state),
      m_platform(std::make_unique<PlatformConnection>())
{
    m_connectResult = MDR_RESULT_INPROGRESS;
    m_platform->create();
    m_pollTimer.setInterval(16);
    connect(&m_pollTimer, &QTimer::timeout, this, &MdrSession::poll);
}

MdrSession::~MdrSession()
{
    m_pollTimer.stop();
    ++m_operationGeneration;
    joinWorker();
    if (backendAvailable())
        mdrConnectionDisconnect(m_platform->connection);
}

bool MdrSession::backendAvailable() const
{
    return m_platform && m_platform->connection;
}

void MdrSession::refreshDevices()
{
    if (!backendAvailable()) {
        emit errorOccurred(QStringLiteral("bluetooth-backend-unavailable"), {});
        return;
    }
    if (m_scanInProgress || m_connectInProgress || m_state != State::Disconnected)
        return;

    joinWorker();
    const quint64 generation = ++m_operationGeneration;
    m_scanInProgress = true;
    emit stateChanged(QStringLiteral("scanning"));
    MDRConnection* connection = m_platform->connection;
    m_workerThread = std::thread([this, connection, generation]() {
        QVariantList devices;
        QString error;
        MDRDeviceInfo* list = nullptr;
        int count = 0;
        const int result = mdrConnectionGetDevicesList(connection, &list, &count);
        if (result == MDR_RESULT_OK) {
            for (int i = 0; i < count; ++i) {
                QString name = QString::fromUtf8(list[i].szDeviceName);
                const QString mac = QString::fromUtf8(list[i].szDeviceMacAddress);
                if (name.isEmpty())
                    name = mac;
                devices.push_back(QVariantMap{
                    {QStringLiteral("name"), name},
                    {QStringLiteral("mac"), mac},
                });
            }
        } else {
            const char* message = mdrConnectionGetLastError(connection);
            error = QString::fromUtf8(message && message[0] ? message : "Bluetooth scan failed");
        }
        if (list)
            mdrConnectionFreeDevicesList(connection, &list);

        m_scanInProgress = false;
        QMetaObject::invokeMethod(this, [this, devices, error, generation]() {
            if (generation != m_operationGeneration)
                return;
            if (!error.isEmpty())
                emit errorOccurred(QStringLiteral("bluetooth-scan-failed"), error);
            emit devicesChanged(devices);
            emit stateChanged(QStringLiteral("disconnected"));
        }, Qt::QueuedConnection);
    });
}

void MdrSession::connectDevice(const QString& name, const QString& mac)
{
    if (!backendAvailable()) {
        emit errorOccurred(QStringLiteral("bluetooth-backend-unavailable"), {});
        return;
    }
    if (m_connectInProgress || m_state != State::Disconnected)
        return;

    if (m_scanInProgress) {
        joinWorker();
        m_scanInProgress = false;
    }
    ++m_operationGeneration;
    m_name = name;
    m_mac = mac;
    m_usingLegacyProtocol = false;
    m_initDone = false;
    m_state = State::Connecting;
    emit stateChanged(QStringLiteral("connecting"));
    startConnectionAttempt(MDR_SERVICE_UUID_XM5);
    m_pollTimer.start();
}

void MdrSession::disconnectDevice()
{
    m_pollTimer.stop();
    ++m_operationGeneration;
    joinWorker();
    m_connectInProgress = false;
    m_scanInProgress = false;
    if (backendAvailable())
        mdrConnectionDisconnect(m_platform->connection);
    m_stateData->device = mdr::MDRHeadphones();
    m_state = State::Disconnected;
    m_initDone = false;
    emit stateChanged(QStringLiteral("disconnected"));
    emit deviceUpdated();
}

void MdrSession::poll()
{
    if (m_state == State::Connecting) {
        if (m_attemptElapsed.isValid() && m_attemptElapsed.elapsed() >= 10000) {
            connectionAttemptFailed(QStringLiteral("connection-timeout"));
            return;
        }
        if (m_connectInProgress)
            return;

        const int connectResult = m_connectResult.load();
        if (connectResult != MDR_RESULT_OK && connectResult != MDR_RESULT_INPROGRESS) {
            const char* error = mdrConnectionGetLastError(m_platform->connection);
            connectionAttemptFailed(QStringLiteral("connection-failed"),
                QString::fromUtf8(error && error[0] ? error : "Connection failed"));
            return;
        }

        const int result = mdrConnectionPoll(m_platform->connection, 0);
        if (result == MDR_RESULT_OK) {
            m_stateData->device = mdr::MDRHeadphones(m_platform->connection);
            m_stateData->device.mModelName = m_name.toStdString();
            m_stateData->device.mUniqueId = m_mac.toStdString();
            m_state = State::Connected;
            emit stateChanged(QStringLiteral("initializing"));
            const int initResult = m_usingLegacyProtocol
                ? m_stateData->device.Invoke(m_stateData->device.RequestInitV1())
                : m_stateData->device.Invoke(m_stateData->device.RequestInitV2());
            if (initResult != MDR_RESULT_OK)
                connectionAttemptFailed(QStringLiteral("initialization-failed"));
        } else if (result != MDR_RESULT_INPROGRESS && result != MDR_RESULT_ERROR_TIMEOUT) {
            const char* error = mdrConnectionGetLastError(m_platform->connection);
            connectionAttemptFailed(QStringLiteral("connection-failed"),
                QString::fromUtf8(error && error[0] ? error : "Connection failed"));
        }
        return;
    }

    if (m_state != State::Connected || !m_stateData->device)
        return;

    if (!m_initDone && m_attemptElapsed.isValid() && m_attemptElapsed.elapsed() >= 10000) {
        connectionAttemptFailed(QStringLiteral("initialization-timeout"));
        return;
    }

    try {
        const int event = m_stateData->device.PollEvents();
        switch (event) {
        case MDR_HEADPHONES_TASK_INIT_OK:
            if (m_usingLegacyProtocol) {
                finishInitialization();
            } else {
                m_stateData->device.Invoke(m_stateData->device.RequestSyncV2());
            }
            break;
        case MDR_HEADPHONES_TASK_SYNC_OK:
            if (!m_initDone)
                finishInitialization();
            else
                emit deviceUpdated();
            break;
        case MDR_HEADPHONES_IDLE:
            if (m_stateData->device.IsReady()) {
                if (m_usingLegacyProtocol && m_stateData->device.IsDirtyV1())
                    m_stateData->device.Invoke(m_stateData->device.RequestCommitV1());
                else if (!m_usingLegacyProtocol && m_stateData->device.IsDirty())
                    m_stateData->device.Invoke(m_stateData->device.RequestCommitV2());
            }
            break;
        case MDR_HEADPHONES_ERROR:
            connectionAttemptFailed(QStringLiteral("protocol-error"),
                QString::fromUtf8(m_stateData->device.GetLastError()));
            return;
        default:
            if (event != MDR_HEADPHONES_TASK_COMMIT_OK)
                emit deviceUpdated();
            break;
        }

        if (m_initDone && m_syncElapsed.isValid() && m_syncElapsed.elapsed() >= 60000
            && m_stateData->device.IsReady()) {
            m_syncElapsed.restart();
            if (m_usingLegacyProtocol)
                m_stateData->device.Invoke(m_stateData->device.RequestSyncV1());
            else
                m_stateData->device.Invoke(m_stateData->device.RequestSyncV2());
        }
    } catch (const std::exception& exception) {
        connectionAttemptFailed(QStringLiteral("unexpected-error"),
            QString::fromUtf8(exception.what()));
    }
}

void MdrSession::startConnectionAttempt(const char* serviceUuid)
{
    joinWorker();
    m_connectInProgress = true;
    m_connectResult = MDR_RESULT_INPROGRESS;
    m_attemptElapsed.start();
    MDRConnection* connection = m_platform->connection;
    const std::string mac = m_mac.toUtf8().toStdString();
    const std::string uuid(serviceUuid);
    m_workerThread = std::thread([this, connection, mac, uuid]() {
        m_connectResult = mdrConnectionConnect(connection, mac.c_str(), uuid.c_str());
        m_connectInProgress = false;
    });
}

bool MdrSession::retryWithLegacyProtocol()
{
    if (m_usingLegacyProtocol || !backendAvailable())
        return false;
    joinWorker();
    mdrConnectionDisconnect(m_platform->connection);
    m_usingLegacyProtocol = true;
    m_state = State::Connecting;
    emit stateChanged(QStringLiteral("connecting"));
    startConnectionAttempt(MDR_SERVICE_UUID_LEGACY);
    return true;
}

void MdrSession::connectionAttemptFailed(const QString& errorCode, const QString& errorDetail)
{
    if (!m_initDone && retryWithLegacyProtocol())
        return;
    failPermanently(errorCode, errorDetail);
}

void MdrSession::finishInitialization()
{
    if (m_initDone)
        return;

    m_initDone = true;
    m_stateData->capabilities.probeFromDevice(m_stateData->device);
    m_syncElapsed.start();
    emit initializationCompleted(QString::fromStdString(m_stateData->device.mModelName));
    emit stateChanged(QStringLiteral("connected"));
    emit deviceUpdated();
}

void MdrSession::joinWorker()
{
    if (m_workerThread.joinable())
        m_workerThread.join();
}

void MdrSession::failPermanently(const QString& errorCode, const QString& errorDetail)
{
    m_pollTimer.stop();
    ++m_operationGeneration;
    joinWorker();
    m_connectInProgress = false;
    if (backendAvailable())
        mdrConnectionDisconnect(m_platform->connection);
    m_stateData->device = mdr::MDRHeadphones();
    m_state = State::Disconnected;
    m_initDone = false;
    emit errorOccurred(errorCode, errorDetail);
    emit stateChanged(QStringLiteral("error"));
    emit connectionAbandoned(errorCode, errorDetail);
    emit deviceUpdated();
}
