#include "PanelController.hpp"

#include <QApplication>
#include <QCursor>
#include <QScreen>
#include <QWindow>
#include <algorithm>

PanelController::PanelController(QWindow* window, bool trayAvailable,
                                 std::function<QRect()> trayGeometry, QObject* parent)
    : QObject(parent), m_window(window), m_trayAvailable(trayAvailable),
      m_trayGeometry(std::move(trayGeometry))
{
    m_focusTimer.setSingleShot(true);
    m_focusTimer.setInterval(150);
    connect(qApp, &QGuiApplication::focusWindowChanged, this,
            [this] { m_focusTimer.start(); });
    connect(qApp, &QGuiApplication::applicationStateChanged, this,
            [this] { m_focusTimer.start(); });
    connect(&m_focusTimer, &QTimer::timeout, this, &PanelController::checkFocus);
    if (trayAvailable) {
        m_window->setFlags(Qt::Tool | Qt::FramelessWindowHint);
        qApp->setQuitOnLastWindowClosed(false);
    } else {
        show();
    }
}

QPoint PanelController::anchoredPosition(const QRect& available, const QRect& anchor,
                                        const QSize& size)
{
    const int gap = 8;
    // No reliable icon geometry (some Linux hosts): use the work area's corner.
    int x = available.right() - size.width() + 1 - gap;
    int y = available.bottom() - size.height() + 1 - gap;
    if (!anchor.isEmpty()) {
        x = anchor.center().x() - size.width() / 2;
        y = anchor.center().y() < available.center().y()
            ? anchor.bottom() + 1 + gap : anchor.top() - size.height() - gap;
    }
    return {std::clamp(x, available.left(), std::max(available.left(), available.right() - size.width() + 1)),
            std::clamp(y, available.top(), std::max(available.top(), available.bottom() - size.height() + 1))};
}

void PanelController::show()
{
    const QRect anchor = m_trayGeometry();
    auto* screen = QGuiApplication::screenAt(anchor.isEmpty() ? QCursor::pos() : anchor.center());
    if (!screen) screen = QGuiApplication::primaryScreen();
    if (screen) {
        const QRect available = screen->availableGeometry();
        m_window->setScreen(screen);
        m_window->resize(std::min(412, available.width()), std::min(600, available.height()));
        m_window->setPosition(anchoredPosition(available, anchor, m_window->size()));
    }
    m_window->show();
    m_window->raise();
    m_window->requestActivate();
}

void PanelController::toggle()
{
    if (!m_trayAvailable) {
        show(); // Without a tray, hiding would remove the only usable entry point.
        return;
    }
    if (m_window->isVisible()) {
        m_window->hide();
    } else if (!m_lastDismiss.isValid() || m_lastDismiss.elapsed() > 250) {
        // A tray press can cause focus loss before its activation arrives.
        show();
    }
}

void PanelController::checkFocus()
{
    if (!m_trayAvailable || !m_window->isVisible() || QApplication::activePopupWidget())
        return;
    for (auto* focus = QGuiApplication::focusWindow(); focus; focus = focus->transientParent()) {
        if (focus == m_window) return;
    }
    m_window->hide();
    m_lastDismiss.start();
}
