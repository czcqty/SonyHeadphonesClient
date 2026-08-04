#include <QCommandLineParser>
#include <QFileInfo>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTimer>
#include <QTranslator>

#include "AppController.hpp"

int main(int argc, char* argv[])
{
    QQuickStyle::setStyle(QStringLiteral("Basic"));
    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("SonyHeadphonesClient"));
    app.setApplicationDisplayName(QStringLiteral("SonyHeadphonesClient"));
    app.setApplicationVersion(QStringLiteral(MDR_VERSION));
    app.setOrganizationName(QStringLiteral("czcqty"));
    app.setOrganizationDomain(QStringLiteral("czcqty.github.io"));
    app.setWindowIcon(QIcon(QStringLiteral(":/icons/app-icon-512.png")));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Clean-room Sound Connect local controls frontend"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption({QStringLiteral("preview-profile"), QStringLiteral("Device profile to load"), QStringLiteral("profile")});
    parser.addOption({QStringLiteral("preview-route"), QStringLiteral("Route to open"), QStringLiteral("route"), QStringLiteral("home")});
    parser.addOption({QStringLiteral("preview-state"), QStringLiteral("State to reproduce"), QStringLiteral("state"), QStringLiteral("ready")});
    parser.addOption({QStringLiteral("preview-theme"), QStringLiteral("Theme to reproduce (system, light, or dark)"), QStringLiteral("theme"), QStringLiteral("system")});
    parser.addOption({QStringLiteral("locale"), QStringLiteral("Interface locale (en or zh_CN)"), QStringLiteral("locale")});
    parser.addOption({QStringLiteral("capture"), QStringLiteral("Save a deterministic window capture and exit"), QStringLiteral("path")});
    parser.process(app);

    const bool previewMode = parser.isSet(QStringLiteral("preview-profile"))
        || parser.isSet(QStringLiteral("preview-route"))
        || parser.isSet(QStringLiteral("preview-state"))
        || parser.isSet(QStringLiteral("preview-theme"));
    AppController controller(previewMode);
    if (parser.isSet(QStringLiteral("preview-profile")))
        controller.applyProfile(parser.value(QStringLiteral("preview-profile")));
    if (previewMode || parser.isSet(QStringLiteral("preview-route")))
        controller.setCurrentRoute(parser.value(QStringLiteral("preview-route")));
    controller.setPreviewState(parser.value(QStringLiteral("preview-state")));
    if (parser.isSet(QStringLiteral("preview-theme")))
        controller.setPreviewTheme(parser.value(QStringLiteral("preview-theme")));
    if (parser.isSet(QStringLiteral("locale")))
        controller.setLocale(parser.value(QStringLiteral("locale")));

    QTranslator translator;
    const auto applyTranslation = [&app, &controller, &translator]() {
        app.removeTranslator(&translator);
        if (controller.locale() == QStringLiteral("zh_CN")) {
            if (!translator.load(QStringLiteral(":/i18n/SonyHeadphonesClient_zh_CN.qm")))
                qWarning("Unable to load the Simplified Chinese translation catalog");
            else
                app.installTranslator(&translator);
        }
    };
    applyTranslation();

    QQmlApplicationEngine engine;
    QObject::connect(&controller, &AppController::localeChanged, &engine,
        [&applyTranslation, &engine]() {
            applyTranslation();
            engine.retranslate();
        });
    engine.rootContext()->setContextProperty(QStringLiteral("appController"), &controller);
    engine.loadFromModule(QStringLiteral("HeadphonesClient"), QStringLiteral("Main"));
    if (engine.rootObjects().isEmpty())
        return EXIT_FAILURE;

    if (parser.isSet(QStringLiteral("capture"))) {
        const QString capturePath = QFileInfo(parser.value(QStringLiteral("capture"))).absoluteFilePath();
        QTimer::singleShot(350, &app, [&app, &engine, capturePath]() {
            auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().constFirst());
            if (!window || !window->grabWindow().save(capturePath))
                app.exit(EXIT_FAILURE);
            else
                app.exit(EXIT_SUCCESS);
        });
    }

    return app.exec();
}
