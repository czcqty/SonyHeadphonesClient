#pragma once

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QSystemTrayIcon>
#include <algorithm>

inline bool trayHostAvailable()
{
    if (qEnvironmentVariable("XDG_CURRENT_DESKTOP").split(':').contains("KDE", Qt::CaseInsensitive)) {
        // Plasma's watcher lives in kded and reports a host even after the
        // shell exits. The actual panel owns this name on both Plasma 5 and 6.
        auto bus = QDBusConnection::sessionBus().interface();
        if (bus) {
            const auto names = bus->registeredServiceNames().value();
            if (names.contains("org.kde.StatusNotifierWatcher"))
                return std::any_of(names.begin(), names.end(), [](const QString& name) {
                    return name.startsWith("org.kde.StatusNotifierHost-");
                });
        }
    }
    return QSystemTrayIcon::isSystemTrayAvailable();
}
