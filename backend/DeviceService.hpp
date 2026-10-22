#pragma once

#include <QObject>
#include <QJsonObject>
#include <memory>
#include "DeviceProfileRepository.hpp"
#include "ViewModels.hpp"

class MdrSession;

// Device lifetime and business operations, independent of any presentation.
// All calls and notifications belong to this object's Qt Core event loop.
class DeviceService : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool previewMode READ previewMode CONSTANT)
    Q_PROPERTY(QString currentProfile READ currentProfile NOTIFY profileChanged)
    Q_PROPERTY(QString modelName READ modelName NOTIFY profileChanged)
    Q_PROPERTY(QString deviceFamily READ deviceFamily NOTIFY profileChanged)
    Q_PROPERTY(QString deviceImage READ deviceImage NOTIFY profileChanged)
    Q_PROPERTY(QString protocol READ protocol NOTIFY profileChanged)
    Q_PROPERTY(bool profileReferenceBlocked READ profileReferenceBlocked NOTIFY profileChanged)
    Q_PROPERTY(int rejectedWriteCount READ rejectedWriteCount NOTIFY rejectedWriteCountChanged)
    Q_PROPERTY(PlaybackViewModel* playback READ playback CONSTANT)
    Q_PROPERTY(NoiseControlViewModel* noiseControl READ noiseControl CONSTANT)
    Q_PROPERTY(EqualizerViewModel* equalizer READ equalizer CONSTANT)
    Q_PROPERTY(SettingsViewModel* settings READ settings CONSTANT)
    Q_PROPERTY(DeviceManagerViewModel* deviceManager READ deviceManager CONSTANT)
    Q_PROPERTY(ConnectionViewModel* connection READ connection CONSTANT)

public:
    explicit DeviceService(bool previewMode = false, QObject* parent = nullptr);
    ~DeviceService() override;
    bool previewMode() const { return m_previewMode; }
    QString currentProfile() const;
    QString modelName() const;
    QString deviceFamily() const;
    QString deviceImage() const;
    QString protocol() const;
    bool profileReferenceBlocked() const;
    int rejectedWriteCount() const { return m_state.rejectedWrites; }
    PlaybackViewModel* playback() { return &m_playback; }
    NoiseControlViewModel* noiseControl() { return &m_noiseControl; }
    EqualizerViewModel* equalizer() { return &m_equalizer; }
    SettingsViewModel* settings() { return &m_settings; }
    DeviceManagerViewModel* deviceManager() { return &m_deviceManager; }
    ConnectionViewModel* connection() { return &m_connection; }
    QJsonObject deviceDiagnostics() const;

    Q_INVOKABLE QStringList availableProfiles() const;
    Q_INVOKABLE bool applyProfile(const QString& profileId);
    Q_INVOKABLE void refreshDevices();
    Q_INVOKABLE void connectDevice(const QString& name, const QString& mac);
    Q_INVOKABLE void disconnectDevice();
    Q_INVOKABLE void notifyRejectedWriteCountChanged();

signals:
    void profileChanged();
    void rejectedWriteCountChanged();
    void connectionAbandoned(const QString& errorCode, const QString& errorDetail);
    void disconnected();

private slots:
    void selectLiveProfile(const QString& modelName);
    void handleConnectionAbandoned(const QString& errorCode, const QString& errorDetail);

private:
    void refreshViewModels();
    const DeviceProfile* profileForDeviceName(const QString& modelName) const;
    DeviceProfileRepository m_profiles;
    const DeviceProfile* m_profile = nullptr;
    DeviceState m_state;
    PlaybackViewModel m_playback;
    NoiseControlViewModel m_noiseControl;
    EqualizerViewModel m_equalizer;
    SettingsViewModel m_settings;
    DeviceManagerViewModel m_deviceManager;
    ConnectionViewModel m_connection;
    bool m_previewMode;
    quint64 m_previewGeneration = 0;
    std::unique_ptr<MdrSession> m_session;
};
