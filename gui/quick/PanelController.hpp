#pragma once

#include <QObject>
#include <QElapsedTimer>
#include <QRect>
#include <QTimer>
#include <functional>

class QWindow;

// Owns presentation only. Device state and settings remain in AppController.
class PanelController final : public QObject
{
    Q_OBJECT
public:
    PanelController(QWindow* window, bool trayAvailable,
                    std::function<QRect()> trayGeometry, QObject* parent = nullptr);
    void toggle();
    void show();
    void checkFocus();
    static QPoint anchoredPosition(const QRect& available, const QRect& anchor,
                                   const QSize& panelSize);
public slots:
    void dismiss();
private:
    QWindow* m_window;
    bool m_trayAvailable;
    std::function<QRect()> m_trayGeometry;
    QTimer m_focusTimer;
    QElapsedTimer m_lastDismiss;
};
