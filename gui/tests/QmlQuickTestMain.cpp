#include <QtQuickTest>

#include <QCoreApplication>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickStyle>
#include <QTranslator>
#include <memory>

#include "../quick/AppController.hpp"

class QuickTestSetup final : public QObject
{
    Q_OBJECT

public slots:
    void applicationAvailable()
    {
        QQuickStyle::setStyle(QStringLiteral("Basic"));
        QCoreApplication::setApplicationName(QStringLiteral("SonyHeadphonesClient"));
        QCoreApplication::setOrganizationName(QStringLiteral("czcqty"));
    }

    void qmlEngineAvailable(QQmlEngine* engine)
    {
        m_controller = std::make_unique<AppController>(true);
        engine->rootContext()->setContextProperty(
            QStringLiteral("appController"), m_controller.get());
        connect(m_controller.get(), &AppController::localeChanged, engine,
            [this, engine]() {
                QCoreApplication::removeTranslator(&m_translator);
                if (m_controller->locale() == QStringLiteral("zh_CN")) {
                    QVERIFY(m_translator.load(
                        QStringLiteral(":/i18n/SonyHeadphonesClient_zh_CN.qm")));
                    QCoreApplication::installTranslator(&m_translator);
                }
                engine->retranslate();
            });
    }

private:
    std::unique_ptr<AppController> m_controller;
    QTranslator m_translator;
};

QUICK_TEST_MAIN_WITH_SETUP(headphonesclient, QuickTestSetup)

#include "QmlQuickTestMain.moc"
