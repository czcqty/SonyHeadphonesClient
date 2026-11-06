#pragma once

#include <KWayland/Client/connection_thread.h>
#include <KWayland/Client/plasmashell.h>
#include <KWayland/Client/registry.h>
#include <KWayland/Client/surface.h>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusReply>
#include <QGuiApplication>
#include <QPointer>
#include <QScreen>
#include <QWindow>

// KWin can place an ordinary, focusable window using the Plasma shell
// extension. Other Wayland compositors own the positioning of their toplevels.
class PlasmaPanel : public QObject {
public:
    PlasmaPanel() {
        if (!QGuiApplication::platformName().startsWith("wayland")) return;
        using namespace KWayland::Client;
        auto connection = ConnectionThread::fromApplication(this);
        if (!connection) return;
        auto registry = new Registry(this);
        connect(registry, &Registry::plasmaShellAnnounced, this, [this, registry](quint32 name, quint32 version) {
            shell = registry->createPlasmaShell(name, version, this);
            connect(shell, &PlasmaShell::removed, shell, &QObject::deleteLater);
            applyPosition();
        });
        registry->create(connection);
        registry->setup();
    }
    static QRect workArea(QScreen* screen) {
        // In particular Qt Wayland may return the whole output as its available
        // geometry. Plasma's strut manager reports panel reservations per output.
        auto query = QDBusMessage::createMethodCall("org.kde.plasmashell", "/StrutManager",
            "org.kde.PlasmaShell.StrutManager", "availableScreenRect");
        query.setAutoStartService(false);
        query << screen->name();
        const QDBusReply<QRect> reply = QDBusConnection::sessionBus().call(query, QDBus::Block, 150);
        const auto work = screen->geometry().intersected(reply.value());
        return reply.isValid() && work.isValid() ? work : screen->availableGeometry();
    }
    void place(QWindow* panel, QPoint origin) {
        window = panel;
        position = origin;
        applyPosition();
    }
private:
    void applyPosition() {
        if (!shell || !shell->isValid() || !window || !window->isVisible()) return;
        using namespace KWayland::Client;
        if (auto surface = Surface::fromWindow(window)) {
            // Qt Wayland may recreate wl_surface after hide/show.
            auto plasmaSurface = PlasmaShellSurface::get(surface);
            if (!plasmaSurface) plasmaSurface = shell->createSurface(surface, surface);
            plasmaSurface->setPosition(position);
            // KWin applies a changed position with the next wl_surface commit.
            // Re-activating a visible panel can leave Qt with nothing to paint.
            if (window->isExposed()) surface->commit(Surface::CommitFlag::None);
        }
    }
    QPointer<KWayland::Client::PlasmaShell> shell;
    QPointer<QWindow> window;
    QPoint position;
};
