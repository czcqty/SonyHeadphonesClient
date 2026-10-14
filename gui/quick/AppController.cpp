#include "AppController.hpp"

#include <QCoreApplication>
#include <QLocale>
#include <QSettings>
#include <QSet>

AppController::AppController(bool previewMode, QObject* parent)
    : DeviceService(previewMode, parent), m_appearance(previewMode, this)
{
    QSettings settings;
    if (this->previewMode()) {
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

    if (!previewMode) m_currentRoute = QStringLiteral("connection");
    connect(this, &DeviceService::connectionAbandoned, this, [this] {
        replaceRoute(QStringLiteral("connection"), true);
    });
    connect(this, &DeviceService::disconnected, this, [this] {
        replaceRoute(QStringLiteral("connection"), true);
    });
}

AppController::~AppController() = default;

QString AppController::desktopStyle() const { return m_appearance.desktop(); }

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
    if (!previewMode()) {
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

void AppController::replaceRoute(const QString& route, bool clearHistory)
{
    if (clearHistory)
        m_routeHistory.clear();
    if (m_currentRoute == route)
        return;
    m_currentRoute = route;
    emit currentRouteChanged();
}
