#pragma once

#include <QObject>
#include <QElapsedTimer>
#include <QTimer>
#include <QVariantList>

#include <atomic>
#include <memory>
#include <thread>

#include "ViewModels.hpp"

struct MDRConnection;
struct PlatformConnection;

class MdrSession final : public QObject
{
    Q_OBJECT

public:
    explicit MdrSession(DeviceState* state, QObject* parent = nullptr);
    ~MdrSession() override;

    bool backendAvailable() const;

public slots:
    void refreshDevices();
    void connectDevice(const QString& name, const QString& mac);
    void disconnectDevice();

signals:
    void stateChanged(const QString& state);
    void errorOccurred(const QString& errorCode, const QString& errorDetail);
    void devicesChanged(const QVariantList& devices);
    void deviceUpdated();
    void initializationCompleted(const QString& modelName);
    void connectionAbandoned(const QString& errorCode, const QString& errorDetail);

private slots:
    void poll();

private:
    enum class State { Disconnected, Connecting, Connected };

    void startConnectionAttempt(const char* serviceUuid);
    bool retryWithLegacyProtocol();
    void connectionAttemptFailed(const QString& errorCode, const QString& errorDetail = {});
    void finishInitialization();
    void joinWorker();
    void failPermanently(const QString& errorCode, const QString& errorDetail);

    DeviceState* m_stateData;
    std::unique_ptr<PlatformConnection> m_platform;
    QTimer m_pollTimer;
    QElapsedTimer m_attemptElapsed;
    QElapsedTimer m_syncElapsed;
    State m_state = State::Disconnected;
    QString m_name;
    QString m_mac;
    bool m_usingLegacyProtocol = false;
    bool m_initDone = false;
    quint64 m_operationGeneration = 0;
    std::thread m_workerThread;
    std::atomic<bool> m_scanInProgress{false};
    std::atomic<bool> m_connectInProgress{false};
    std::atomic<int> m_connectResult{0};
};
