#include "AppController.hpp"
#include "MdrSession.hpp"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QLocale>
#include <QSettings>
#include <QSet>
#include <QStyleHints>

AppController::AppController(bool previewMode, QObject* parent)
    : QObject(parent),
      m_playback(&m_state, this),
      m_noiseControl(&m_state, this),
      m_equalizer(&m_state, this),
      m_settings(&m_state, this),
      m_deviceManager(&m_state, this),
      m_connection(this),
      m_previewMode(previewMode),
      m_appearance(previewMode, this)
{
    QSettings settings;
    if (m_previewMode) {
        m_locale = QStringLiteral("en");
    } else if (settings.contains(QStringLiteral("appearance/locale"))) {
        m_locale = settings.value(QStringLiteral("appearance/locale")).toString()
                .startsWith(QStringLiteral("zh"), Qt::CaseInsensitive)
            ? QStringLiteral("zh_CN") : QStringLiteral("en");
    } else {
        m_locale = QLocale::system().language() == QLocale::Chinese
            ? QStringLiteral("zh_CN") : QStringLiteral("en");
    }

    connect(&m_appearance, &PlatformAppearance::changed, this, [this] {
        emit darkModeChanged();
        emit appearanceChanged();
    });

    applyProfile(QStringLiteral("WH-1000XM6"));
    m_connection.setSelectedDevice(modelName(), deviceImage());
    if (m_previewMode) {
        m_connection.setState(QStringLiteral("connected"));
    } else {
        m_currentRoute = QStringLiteral("connection");
        m_connection.setState(QStringLiteral("disconnected"));
        m_connection.setDevices({});
        m_session = std::make_unique<MdrSession>(&m_state, this);
        connect(m_session.get(), &MdrSession::stateChanged,
                &m_connection, &ConnectionViewModel::setState);
        connect(m_session.get(), &MdrSession::errorOccurred,
                &m_connection, &ConnectionViewModel::setError);
        connect(m_session.get(), &MdrSession::devicesChanged,
                &m_connection, &ConnectionViewModel::setDevices);
        connect(m_session.get(), &MdrSession::deviceUpdated,
                this, &AppController::refreshViewModels);
        connect(m_session.get(), &MdrSession::initializationCompleted,
                this, &AppController::selectLiveProfile);
        connect(m_session.get(), &MdrSession::connectionAbandoned,
                this, &AppController::handleConnectionAbandoned);
    }
}

AppController::~AppController() = default;

QString AppController::desktopStyle() const { return m_appearance.desktop(); }

QString AppController::currentProfile() const { return m_profile ? m_profile->id : QString(); }
QString AppController::modelName() const { return m_profile ? m_profile->modelName : QString(); }
QString AppController::deviceFamily() const { return m_profile ? m_profile->family : QString(); }
QString AppController::deviceImage() const { return m_profile ? m_profile->assetPath : QString(); }
QString AppController::protocol() const { return m_profile ? m_profile->protocol : QString(); }
bool AppController::profileReferenceBlocked() const { return m_profile && m_profile->referenceBlocked; }

QStringList AppController::availableProfiles() const
{
    return m_profiles.profileIds();
}

bool AppController::applyProfile(const QString& profileId)
{
    const DeviceProfile* profile = m_profiles.find(profileId);
    if (!profile)
        return false;

    m_profile = profile;
    m_profiles.apply(*profile, m_state.device, m_state.capabilities);
    if (m_previewMode)
        m_connection.setSelectedDevice(profile->modelName, profile->assetPath);
    m_state.rejectedWrites = 0;
    refreshViewModels();
    emit profileChanged();
    emit rejectedWriteCountChanged();
    return true;
}

void AppController::refreshDevices()
{
    m_connection.clearError();
    if (m_previewMode) {
        m_connection.refresh();
        m_connection.setState(QStringLiteral("disconnected"));
        return;
    }
    if (m_session)
        m_session->refreshDevices();
}

void AppController::connectDevice(const QString& name, const QString& mac)
{
    const DeviceProfile* pendingProfile = profileForDeviceName(name);
    m_connection.setSelectedDevice(name, pendingProfile ? pendingProfile->assetPath : deviceImage());
    m_connection.clearError();
    if (m_previewMode) {
        m_connection.setState(QStringLiteral("connecting"));
        QMetaObject::invokeMethod(this, [this, name]() {
            selectLiveProfile(name);
            m_connection.setState(QStringLiteral("connected"));
        }, Qt::QueuedConnection);
        return;
    }
    if (m_session)
        m_session->connectDevice(name, mac);
}

void AppController::disconnectDevice()
{
    if (m_session)
        m_session->disconnectDevice();
    replaceRoute(QStringLiteral("connection"), true);
}

void AppController::goBack()
{
    if (m_routeHistory.isEmpty())
        return;
    const QString previous = m_routeHistory.takeLast();
    if (m_currentRoute == previous)
        return;
    m_currentRoute = previous;
    emit currentRouteChanged();
}

void AppController::notifyRejectedWriteCountChanged()
{
    emit rejectedWriteCountChanged();
}

void AppController::setCurrentRoute(const QString& route)
{
    static const QSet<QString> allowed = {
        QStringLiteral("connection"),
        QStringLiteral("home"),
        QStringLiteral("settings"),
        QStringLiteral("sound"),
        QStringLiteral("behavior"),
        QStringLiteral("touch"),
        QStringLiteral("voice"),
        QStringLiteral("noise"),
        QStringLiteral("equalizer"),
        QStringLiteral("devices"),
    };
    const QString normalized = allowed.contains(route) ? route : QStringLiteral("home");
    if (m_currentRoute == normalized)
        return;
    m_routeHistory.push_back(m_currentRoute);
    m_currentRoute = normalized;
    emit currentRouteChanged();
}

void AppController::toggleDarkMode()
{
    m_appearance.setThemeMode(darkMode() ? QStringLiteral("light") : QStringLiteral("dark"));
}

void AppController::toggleLocale()
{
    setLocale(m_locale == QStringLiteral("zh_CN")
        ? QStringLiteral("en") : QStringLiteral("zh_CN"));
}

void AppController::completeConnectionTransition()
{
    replaceRoute(QStringLiteral("home"), true);
}

void AppController::setLocale(const QString& locale)
{
    const QString normalized = locale.startsWith(QStringLiteral("zh"), Qt::CaseInsensitive)
        ? QStringLiteral("zh_CN") : QStringLiteral("en");
    if (m_locale == normalized)
        return;
    m_locale = normalized;
    if (!m_previewMode) {
        QSettings settings;
        settings.setValue(QStringLiteral("appearance/locale"), m_locale);
    }
    emit localeChanged();
}

void AppController::setPreviewState(const QString& state)
{
    static const QSet<QString> allowed = {
        QStringLiteral("ready"),
        QStringLiteral("loading"),
        QStringLiteral("empty"),
        QStringLiteral("error"),
        QStringLiteral("disabled"),
    };
    const QString normalized = allowed.contains(state) ? state : QStringLiteral("ready");
    if (m_previewState == normalized)
        return;
    m_previewState = normalized;
    emit previewStateChanged();
}

void AppController::setPreviewTheme(const QString& theme)
{
    m_appearance.setThemeMode(theme.toLower());
}

void AppController::refreshViewModels()
{
    m_playback.refresh();
    m_noiseControl.refresh();
    m_equalizer.refresh();
    m_settings.refresh();
    m_deviceManager.refresh();
}

void AppController::selectLiveProfile(const QString& modelName)
{
    if (const DeviceProfile* profile = profileForDeviceName(modelName))
        m_profile = profile;
    emit profileChanged();
    refreshViewModels();
}

const DeviceProfile* AppController::profileForDeviceName(const QString& modelName) const
{
    if (const DeviceProfile* exact = m_profiles.find(modelName))
        return exact;

    for (const QString& id : m_profiles.profileIds()) {
        if (!id.startsWith(QStringLiteral("generic"))
            && modelName.contains(id, Qt::CaseInsensitive))
            return m_profiles.find(id);
    }

    if (modelName.contains(QStringLiteral("WF-"), Qt::CaseInsensitive)
        || modelName.contains(QStringLiteral("LinkBuds"), Qt::CaseInsensitive))
        return m_profiles.find(QStringLiteral("generic-tws"));
    return m_profiles.find(QStringLiteral("generic-headband"));
}

void AppController::replaceRoute(const QString& route, bool clearHistory)
{
    if (clearHistory)
        m_routeHistory.clear();
    if (m_currentRoute == route)
        return;
    m_currentRoute = route;
    emit currentRouteChanged();
}

void AppController::handleConnectionAbandoned(const QString& errorCode, const QString& errorDetail)
{
    m_connection.setError(errorCode, errorDetail);
    replaceRoute(QStringLiteral("connection"), true);
}
