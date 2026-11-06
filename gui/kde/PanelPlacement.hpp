#pragma once
#include <QRect>

// All inputs are in Qt's device-independent coordinates, including on screens
// whose origin is negative. A missing tray point uses the edge reserved by the
// desktop panel; with no reservation the default is bottom right.
inline QPoint defaultTrayCorner(const QRect& screen, const QRect& work)
{
    const bool left = work.left() - screen.left() > screen.right() - work.right();
    const bool top = work.top() - screen.top() > screen.bottom() - work.bottom();
    return {left ? work.left() : work.right(), top ? work.top() : work.bottom()};
}

inline QPoint quickPanelOrigin(QRect work, QSize size, QPoint tray)
{
    const int freeX = qMax(0, work.width() - size.width());
    const int freeY = qMax(0, work.height() - size.height());
    const int marginX = qMin(8, freeX / 2), marginY = qMin(8, freeY / 2);
    return work.topLeft() + QPoint(tray.x() < work.center().x() ? marginX : freeX - marginX,
                                   tray.y() < work.center().y() ? marginY : freeY - marginY);
}
