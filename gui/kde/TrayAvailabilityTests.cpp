#include "TrayAvailability.hpp"
#include <QCoreApplication>

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    qputenv("XDG_CURRENT_DESKTOP", "KDE");
    auto bus = QDBusConnection::sessionBus();
    if (!bus.registerService("org.kde.StatusNotifierWatcher") || trayHostAvailable()) return 1;
    const auto host = QStringLiteral("org.kde.StatusNotifierHost-123");
    if (!bus.registerService(host) || !trayHostAvailable()) return 1;
    if (!bus.unregisterService(host) || trayHostAvailable()) return 1;
    return bus.registerService(host) && trayHostAvailable() ? 0 : 1;
}
