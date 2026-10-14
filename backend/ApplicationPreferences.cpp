#include "ApplicationPreferences.hpp"
#include <QLocale>

ApplicationPreferences::ApplicationPreferences(bool preview, QSettings* store)
    : m_preview(preview), m_store(store),
      m_locale(preview ? "en" : QLocale::system().language() == QLocale::Chinese ? "zh_CN" : "en")
{
    if (preview) return;
    QSettings defaults;
    auto& settings = store ? *store : defaults;
    if (settings.contains("appearance/locale"))
        m_locale = settings.value("appearance/locale").toString().startsWith("zh", Qt::CaseInsensitive) ? "zh_CN" : "en";
    if (settings.contains("appearance/themeMode"))
        set("themeMode", settings.value("appearance/themeMode").toString(), false);
    else if (settings.contains("appearance/darkMode")) {
        m_theme = settings.value("appearance/darkMode").toBool() ? "dark" : "light";
        settings.setValue("appearance/themeMode", m_theme);
        settings.sync();
        // Keep the old key until a successful sync; older Qt releases can still
        // read it. Explicit subsequent changes remove it after saving the new key.
    }
}

QJsonObject ApplicationPreferences::snapshot() const
{
    return {{"locale", m_locale}, {"themeMode", m_theme}, {"persistent", !m_preview}};
}

bool ApplicationPreferences::set(const QString& key, const QString& value, bool persist)
{
    if (key == "locale") {
        if (value != "en" && value != "zh_CN") return false;
    } else if (key == "themeMode") {
        if (value != "system" && value != "light" && value != "dark") return false;
    } else return false;
    if (persist && !m_preview) {
        QSettings defaults;
        auto& settings = m_store ? *m_store : defaults;
        settings.setValue("appearance/" + key, value);
        settings.sync();
        if (settings.status() != QSettings::NoError) return false;
        if (key == "themeMode") settings.remove("appearance/darkMode");
    }
    (key == "locale" ? m_locale : m_theme) = value;
    return true;
}
