#include <QtTest>
#include <QApplication>
#include <QWindow>
#include "../quick/PanelController.hpp"

class PanelTests : public QObject
{
    Q_OBJECT
private slots:
    void anchorStaysInsideWorkArea()
    {
        const QRect work(-1920, 30, 1920, 1050);
        const QSize size(412, 600);
        for (const QRect anchor : {QRect(-30, 0, 20, 20), QRect(-1900, 1080, 20, 20),
                                   QRect(-1930, 500, 20, 20), QRect()}) {
            const QPoint pos = PanelController::anchoredPosition(work, anchor, size);
            QVERIFY(work.contains(QRect(pos, size)));
        }
        QCOMPARE(PanelController::anchoredPosition(work, QRect(-600, 0, 20, 20), size).y(), 30);
    }

    void noTrayKeepsFallbackReachable()
    {
        QWindow window;
        PanelController panel(&window, false, [] { return QRect(); });
        QVERIFY(window.isVisible());
        panel.checkFocus();
        QVERIFY(window.isVisible());
        panel.toggle();
        QVERIFY(window.isVisible());
    }

    void residentPanelAndTransientFocus()
    {
        QWindow window;
        PanelController panel(&window, true, [] { return QRect(); });
        QVERIFY(!window.isVisible());
        panel.toggle();
        QTRY_VERIFY(window.isVisible());
        window.requestActivate();
        QTRY_COMPARE(QGuiApplication::focusWindow(), &window);
        panel.checkFocus();
        QVERIFY(window.isVisible());

        QWindow popup;
        popup.setTransientParent(&window);
        popup.show();
        popup.requestActivate();
        QTRY_COMPARE(QGuiApplication::focusWindow(), &popup);
        panel.checkFocus();
        QVERIFY(window.isVisible());

        QWindow other;
        other.show();
        other.requestActivate();
        QTRY_COMPARE(QGuiApplication::focusWindow(), &other);
        panel.checkFocus();
        QVERIFY(!window.isVisible());
    }
};

QTEST_MAIN(PanelTests)
#include "PanelTests.moc"
