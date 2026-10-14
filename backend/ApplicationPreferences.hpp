#pragma once

#include <QJsonObject>
#include <QSettings>

// Storage and migration only. Rendering and system-theme observation stay in
// the native adapters. Preview never reads or writes the user's settings.
class ApplicationPreferences final
{
public:
    explicit ApplicationPreferences(bool preview, QSettings* store = nullptr);
    QJsonObject snapshot() const;
    bool set(const QString& key, const QString& value, bool persist = true);
private:
    bool m_preview;
    QSettings* m_store;
    QString m_locale, m_theme = "system";
};
