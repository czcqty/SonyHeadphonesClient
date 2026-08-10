#include <QCommandLineParser>
#include <QFileInfo>
#include <QApplication>
#include <QAction>
#include <QAbstractNativeEventFilter>
#include <QIcon>
#include <QMenu>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QShortcut>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QTranslator>

#include "AppController.hpp"

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>

class GlobalHotkeyFilter final : public QAbstractNativeEventFilter
{
public:
    GlobalHotkeyFilter(AppController* controller, QQuickWindow* window)
        : m_controller(controller), m_window(window)
    {
        RegisterHotKey(nullptr, 1, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 'P');
        RegisterHotKey(nullptr, 2, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 'N');
        RegisterHotKey(nullptr, 3, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 'M');
    }

    ~GlobalHotkeyFilter() override
    {
        UnregisterHotKey(nullptr, 1); UnregisterHotKey(nullptr, 2); UnregisterHotKey(nullptr, 3);
    }

    bool nativeEventFilter(const QByteArray&, void* message, qintptr*) override
    {
        const auto* msg = static_cast<MSG*>(message);
        if (!msg || msg->message != WM_HOTKEY)
            return false;
        if (msg->wParam == 1) m_controller->playback()->togglePlayback();
        if (msg->wParam == 2) m_controller->playback()->nextTrack();
        if (msg->wParam == 3) {
            if (m_window->isVisible()) m_window->hide();
            else { m_window->show(); m_window->raise(); m_window->requestActivate(); }
        }
        return false;
    }

private:
    AppController* m_controller;
    QQuickWindow* m_window;
};
#endif

int main(int argc, char* argv[])
{
    QQuickStyle::setStyle(QStringLiteral("Basic"));
    QApplication app(argc, argv);
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
    parser.addOption({QStringLiteral("preview-size"), QStringLiteral("Viewport size in WIDTHxHEIGHT form"), QStringLiteral("size"), QStringLiteral("412x780")});
    parser.addOption({QStringLiteral("locale"), QStringLiteral("Interface locale (en or zh_CN)"), QStringLiteral("locale")});
    parser.addOption({QStringLiteral("capture"), QStringLiteral("Save a deterministic window capture and exit"), QStringLiteral("path")});
    parser.process(app);

    const bool previewMode = parser.isSet(QStringLiteral("preview-profile"))
        || parser.isSet(QStringLiteral("preview-route"))
        || parser.isSet(QStringLiteral("preview-state"))
        || parser.isSet(QStringLiteral("preview-theme"))
        || parser.isSet(QStringLiteral("preview-size"));
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

    auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().constFirst());
    if (window && previewMode) {
        const QString normalizedSize = QString(parser.value(QStringLiteral("preview-size"))).replace('X', 'x');
        const QStringList dimensions = normalizedSize.split('x');
        bool widthOk = false;
        bool heightOk = false;
        const int width = dimensions.value(0).toInt(&widthOk);
        const int height = dimensions.value(1).toInt(&heightOk);
        if (dimensions.size() != 2 || !widthOk || !heightOk || width < 360 || height < 600) {
            qCritical("Invalid --preview-size; expected WIDTHxHEIGHT with a minimum of 360x600");
            return EXIT_FAILURE;
        }
        window->resize(width, height);
    }
    std::unique_ptr<QSystemTrayIcon> tray;
    std::unique_ptr<QMenu> trayMenu;
    std::unique_ptr<QShortcut> playShortcut;
    std::unique_ptr<QShortcut> nextShortcut;
    std::unique_ptr<QShortcut> windowShortcut;
#if defined(_WIN32)
    std::unique_ptr<GlobalHotkeyFilter> globalHotkeys;
#endif

    const auto toggleWindow = [window]() {
        if (!window)
            return;
        if (window->isVisible()) window->hide();
        else { window->show(); window->raise(); window->requestActivate(); }
    };

    if (window && !previewMode) {
#if defined(_WIN32)
        globalHotkeys = std::make_unique<GlobalHotkeyFilter>(&controller, window);
        app.installNativeEventFilter(globalHotkeys.get());
#else
        playShortcut = std::make_unique<QShortcut>(QKeySequence(QStringLiteral("Ctrl+Alt+P")), window);
        nextShortcut = std::make_unique<QShortcut>(QKeySequence(QStringLiteral("Ctrl+Alt+N")), window);
        windowShortcut = std::make_unique<QShortcut>(QKeySequence(QStringLiteral("Ctrl+Alt+M")), window);
        QObject::connect(playShortcut.get(), &QShortcut::activated, window,
                         [&controller]() { controller.playback()->togglePlayback(); });
        QObject::connect(nextShortcut.get(), &QShortcut::activated, window,
                         [&controller]() { controller.playback()->nextTrack(); });
        QObject::connect(windowShortcut.get(), &QShortcut::activated, window, toggleWindow);
#endif
    }

    if (window && !previewMode && QSystemTrayIcon::isSystemTrayAvailable()) {
        app.setQuitOnLastWindowClosed(false);
        trayMenu = std::make_unique<QMenu>();
        auto* showAction = trayMenu->addAction(QObject::tr("Show / Hide"));
        auto* playAction = trayMenu->addAction(QObject::tr("Play / Pause"));
        auto* nextAction = trayMenu->addAction(QObject::tr("Next track"));
        auto* disconnectAction = trayMenu->addAction(QObject::tr("Disconnect"));
        trayMenu->addSeparator();
        auto* quitAction = trayMenu->addAction(QObject::tr("Quit"));
        QObject::connect(showAction, &QAction::triggered, window, toggleWindow);
        QObject::connect(playAction, &QAction::triggered, window,
                         [&controller]() { controller.playback()->togglePlayback(); });
        QObject::connect(nextAction, &QAction::triggered, window,
                         [&controller]() { controller.playback()->nextTrack(); });
        QObject::connect(disconnectAction, &QAction::triggered, &controller,
                         &AppController::disconnectDevice);
        QObject::connect(quitAction, &QAction::triggered, &app, &QCoreApplication::quit);
        tray = std::make_unique<QSystemTrayIcon>(app.windowIcon());
        tray->setToolTip(QStringLiteral("SonyHeadphonesClient"));
        tray->setContextMenu(trayMenu.get());
        QObject::connect(tray.get(), &QSystemTrayIcon::activated, window,
                         [toggleWindow](QSystemTrayIcon::ActivationReason reason) {
            if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick)
                toggleWindow();
        });
        tray->show();

    }

    if (parser.isSet(QStringLiteral("capture"))) {
        const QString capturePath = QFileInfo(parser.value(QStringLiteral("capture"))).absoluteFilePath();
        QTimer::singleShot(350, &app, [&app, &engine, capturePath]() {
            auto* captureWindow = qobject_cast<QQuickWindow*>(engine.rootObjects().constFirst());
            if (!captureWindow || !captureWindow->grabWindow().save(capturePath))
                app.exit(EXIT_FAILURE);
            else
                app.exit(EXIT_SUCCESS);
        });
    }

    return app.exec();
}
