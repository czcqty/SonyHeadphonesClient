#pragma once

#include <QObject>
#include <QStringList>
#include <memory>

#include "DeviceProfileRepository.hpp"
#include "ViewModels.hpp"
#include "PlatformAppearance.hpp"

class MdrSession;

class AppController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString currentRoute READ currentRoute WRITE setCurrentRoute NOTIFY currentRouteChanged)
    Q_PROPERTY(QString locale READ locale WRITE setLocale NOTIFY localeChanged)
    Q_PROPERTY(QString previewState READ previewState WRITE setPreviewState NOTIFY previewStateChanged)
    Q_PROPERTY(bool previewMode READ previewMode CONSTANT)
    Q_PROPERTY(QString desktopStyle READ desktopStyle NOTIFY appearanceChanged)
    Q_PROPERTY(PlatformAppearance* appearance READ appearance CONSTANT)
    Q_PROPERTY(bool darkMode READ darkMode NOTIFY darkModeChanged)
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
    explicit AppController(bool previewMode = false, QObject* parent = nullptr);
    ~AppController() override;

    QString currentRoute() const { return m_currentRoute; }
    QString locale() const { return m_locale; }
    QString previewState() const { return m_previewState; }
    bool previewMode() const { return m_previewMode; }
    QString desktopStyle() const;
    bool darkMode() const { return m_appearance.dark(); }
    PlatformAppearance* appearance() { return &m_appearance; }
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

    Q_INVOKABLE QStringList availableProfiles() const;
    Q_INVOKABLE bool applyProfile(const QString& profileId);
    Q_INVOKABLE void refreshDevices();
    Q_INVOKABLE void connectDevice(const QString& name, const QString& mac);
    Q_INVOKABLE void disconnectDevice();
    Q_INVOKABLE void goBack();
    Q_INVOKABLE void toggleDarkMode();
    Q_INVOKABLE void toggleLocale();
    Q_INVOKABLE void completeConnectionTransition();
    Q_INVOKABLE void notifyRejectedWriteCountChanged();

    void setCurrentRoute(const QString& route);
    void setLocale(const QString& locale);
    void setPreviewState(const QString& state);
    void setPreviewTheme(const QString& theme);

signals:
    void appearanceChanged();
    void currentRouteChanged();
    void localeChanged();
    void previewStateChanged();
    void darkModeChanged();
    void profileChanged();
    void rejectedWriteCountChanged();

private:
    void refreshViewModels();
    const DeviceProfile* profileForDeviceName(const QString& modelName) const;
    void replaceRoute(const QString& route, bool clearHistory = false);

private slots:
    void selectLiveProfile(const QString& modelName);
    void handleConnectionAbandoned(const QString& errorCode, const QString& errorDetail);

private:
    DeviceProfileRepository m_profiles;
    const DeviceProfile* m_profile = nullptr;
    DeviceState m_state;
    PlaybackViewModel m_playback;
    NoiseControlViewModel m_noiseControl;
    EqualizerViewModel m_equalizer;
    SettingsViewModel m_settings;
    DeviceManagerViewModel m_deviceManager;
    ConnectionViewModel m_connection;
    QString m_currentRoute = QStringLiteral("home");
    QString m_locale = QStringLiteral("en");
    QString m_previewState = QStringLiteral("ready");
    bool m_previewMode = false;
    PlatformAppearance m_appearance;
    QStringList m_routeHistory;
    std::unique_ptr<MdrSession> m_session;
};
