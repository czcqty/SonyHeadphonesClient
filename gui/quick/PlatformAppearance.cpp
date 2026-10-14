#include "PlatformAppearance.hpp"

#include <QEvent>
#include <QGuiApplication>
#include <QPalette>
#include <QSettings>
#include <QStyleHints>
#include <QWindow>

QString PlatformAppearance::detectDesktop(const QString& environment)
{
    const auto value = environment.toLower();
    if (value.contains("kde") || value.contains("plasma")) return QStringLiteral("kde");
    if (value.contains("gnome") || value.contains("unity")) return QStringLiteral("gnome");
    if (value.contains("xfce")) return QStringLiteral("xfce");
    return QStringLiteral("linux");
}

PlatformAppearance::PlatformAppearance(bool preview, QObject* parent)
    : QObject(parent), m_preview(preview)
{
#ifdef Q_OS_WIN
    m_desktop = QStringLiteral("windows");
#elif defined(Q_OS_MACOS)
    m_desktop = QStringLiteral("macos");
#else
    m_desktop = detectDesktop(qEnvironmentVariable("XDG_CURRENT_DESKTOP",
                             qEnvironmentVariable("DESKTOP_SESSION")));
#endif
    if (!preview) {
        QSettings settings;
        if (settings.contains("appearance/themeMode"))
            m_mode = settings.value("appearance/themeMode").toString();
        else if (settings.contains("appearance/darkMode")) {
            m_mode = settings.value("appearance/darkMode").toBool() ? "dark" : "light";
            settings.setValue("appearance/themeMode", m_mode);
        }
        if (m_mode != "system" && m_mode != "light" && m_mode != "dark")
            m_mode = QStringLiteral("system");
    }
    if (!qobject_cast<QGuiApplication*>(QCoreApplication::instance())) return;
    m_systemDark = QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark;
    if (QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Unknown)
        m_systemDark = QGuiApplication::palette().color(QPalette::Window).lightnessF() < 0.5;
    qGuiApp->installEventFilter(this);
    connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this,
        [this](Qt::ColorScheme scheme) {
            m_systemDark = scheme == Qt::ColorScheme::Dark;
            emit changed();
        });
}

QFont PlatformAppearance::systemFont() const { return QGuiApplication::font(); }

void PlatformAppearance::setThemeMode(const QString& mode)
{
    if (mode != "system" && mode != "light" && mode != "dark") return;
    if (m_mode == mode) return;
    m_mode = mode;
    if (!m_preview) {
        QSettings settings;
        settings.setValue("appearance/themeMode", mode);
        settings.remove("appearance/darkMode");
    }
    applyTheme();
    emit changed();
}

bool PlatformAppearance::setPreviewDesktop(const QString& desktop)
{
    if (!m_preview || !QStringList{"windows", "macos", "kde", "gnome", "xfce", "linux"}.contains(desktop))
        return false;
    // Metadata only: native styles must run on their host operating system.
    m_desktop = desktop;
    emit changed();
    return true;
}

bool PlatformAppearance::eventFilter(QObject*, QEvent* event)
{
    if (event->type() == QEvent::ApplicationPaletteChange || event->type() == QEvent::ApplicationFontChange)
        emit changed();
    return false;
}

void PlatformAppearance::attachWindow(QWindow* window)
{
    m_window = window;
    applyTheme();
}

void PlatformAppearance::applyTheme()
{
    if (!m_window) return;
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    QGuiApplication::styleHints()->setColorScheme(m_mode == "dark" ? Qt::ColorScheme::Dark
        : m_mode == "light" ? Qt::ColorScheme::Light : Qt::ColorScheme::Unknown);
#else
    // Keep the preference for newer runtimes; do not replace native palettes.
    if (m_mode != "system")
        qWarning("Manual color scheme requires Qt 6.8 or newer; using the system theme");
#endif
}
