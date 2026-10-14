#include <QCommandLineParser>
#include <QFileInfo>
#include <QApplication>
#include <QAction>
#include <QAbstractNativeEventFilter>
#include <QIcon>
#include <QCursor>
#include <QMenu>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QShortcut>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QSurfaceFormat>
#include <QTranslator>

#include "AppController.hpp"
#include "PanelController.hpp"

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>

class GlobalHotkeyFilter final : public QAbstractNativeEventFilter
{
public:
    GlobalHotkeyFilter(AppController* controller, PanelController* panel)
        : m_controller(controller), m_panel(panel)
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
        if (msg->wParam == 3) m_panel->toggle();
        return false;
    }

private:
    AppController* m_controller;
    PanelController* m_panel;
};
#endif

int main(int argc, char* argv[])
{
    QSurfaceFormat format = QSurfaceFormat::defaultFormat();
    format.setAlphaBufferSize(8);
    QSurfaceFormat::setDefaultFormat(format);
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
    parser.addOption({QStringLiteral("preview-size"), QStringLiteral("Viewport size in WIDTHxHEIGHT form"), QStringLiteral("size"), QStringLiteral("412x600")});
    parser.addOption({QStringLiteral("locale"), QStringLiteral("Interface locale (en or zh_CN)"), QStringLiteral("locale")});
    parser.addOption({QStringLiteral("capture"), QStringLiteral("Save a deterministic window capture and exit"), QStringLiteral("path")});
    parser.addOption({QStringLiteral("preview-desktop"), QStringLiteral("Legacy preview metadata; controls use the host Qt style (windows, macos, kde, gnome, xfce, linux)"), QStringLiteral("desktop")});
    parser.process(app);

    const bool previewMode = parser.isSet(QStringLiteral("preview-desktop"))
        || parser.isSet(QStringLiteral("preview-profile"))
        || parser.isSet(QStringLiteral("preview-route"))
        || parser.isSet(QStringLiteral("preview-state"))
        || parser.isSet(QStringLiteral("preview-theme"))
        || parser.isSet(QStringLiteral("preview-size"));
    AppController controller(previewMode);
    if (parser.isSet(QStringLiteral("preview-desktop"))
        && !controller.appearance()->setPreviewDesktop(parser.value(QStringLiteral("preview-desktop")))) {
        qCritical("Invalid --preview-desktop; expected windows, macos, kde, gnome, xfce, or linux");
        return EXIT_FAILURE;
    }
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
        if (dimensions.size() != 2 || !widthOk || !heightOk || width < 360 || height < 480) {
            qCritical("Invalid --preview-size; expected WIDTHxHEIGHT with a minimum of 360x480");
            return EXIT_FAILURE;
        }
        window->resize(width, height);
    }
    std::unique_ptr<QSystemTrayIcon> tray;
    std::unique_ptr<QMenu> trayMenu;
    std::unique_ptr<PanelController> panel;
    std::unique_ptr<QShortcut> playShortcut;
    std::unique_ptr<QShortcut> nextShortcut;
    std::unique_ptr<QShortcut> windowShortcut;
#if defined(_WIN32)
    std::unique_ptr<GlobalHotkeyFilter> globalHotkeys;
#endif

    const bool hasTray = !previewMode && QSystemTrayIcon::isSystemTrayAvailable();
    if (window && !previewMode) {
        panel = std::make_unique<PanelController>(window, hasTray,
            [&tray] { return tray ? tray->geometry() : QRect(); });
        QObject::connect(window, SIGNAL(dismissRequested()), panel.get(), SLOT(dismiss()));
    }
    controller.appearance()->attachWindow(window);
    const auto toggleWindow = [&panel]() { if (panel) panel->toggle(); };

    if (window && !previewMode) {
#if defined(_WIN32)
        globalHotkeys = std::make_unique<GlobalHotkeyFilter>(&controller, panel.get());
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

    if (window && hasTray) {
        app.setQuitOnLastWindowClosed(false);
        trayMenu = std::make_unique<QMenu>();
        auto* showAction = trayMenu->addAction(QObject::tr("Show / Hide"));
        auto* playAction = trayMenu->addAction(QObject::tr("Play / Pause"));
        auto* nextAction = trayMenu->addAction(QObject::tr("Next track"));
        auto* disconnectAction = trayMenu->addAction(QObject::tr("Disconnect"));
        trayMenu->addSeparator();
        auto* quitAction = trayMenu->addAction(QObject::tr("Quit"));
        QObject::connect(&controller, &AppController::localeChanged, trayMenu.get(),
            [showAction, playAction, nextAction, disconnectAction, quitAction] {
                showAction->setText(QObject::tr("Show / Hide"));
                playAction->setText(QObject::tr("Play / Pause"));
                nextAction->setText(QObject::tr("Next track"));
                disconnectAction->setText(QObject::tr("Disconnect"));
                quitAction->setText(QObject::tr("Quit"));
            });
        QObject::connect(showAction, &QAction::triggered, window, toggleWindow);
        QObject::connect(playAction, &QAction::triggered, window,
                         [&controller]() { controller.playback()->togglePlayback(); });
        QObject::connect(nextAction, &QAction::triggered, window,
                         [&controller]() { controller.playback()->nextTrack(); });
        QObject::connect(disconnectAction, &QAction::triggered, &controller,
                         &AppController::disconnectDevice);
        QObject::connect(quitAction, &QAction::triggered, &app, &QCoreApplication::quit);
        QObject::connect(trayMenu.get(), &QMenu::aboutToHide, window, [&panel] {
            QTimer::singleShot(200, panel.get(), &PanelController::checkFocus);
        });
        tray = std::make_unique<QSystemTrayIcon>(app.windowIcon());
        tray->setToolTip(QStringLiteral("SonyHeadphonesClient"));
#if !defined(__APPLE__)
        tray->setContextMenu(trayMenu.get());
#endif
        QObject::connect(tray.get(), &QSystemTrayIcon::activated, window,
                         [toggleWindow, &trayMenu](QSystemTrayIcon::ActivationReason reason) {
            if (reason == QSystemTrayIcon::Trigger)
                toggleWindow();
#if defined(__APPLE__)
            // Attaching a Cocoa tray menu consumes the primary click. Keep it
            // for the panel and expose the existing actions on a context click.
            if (reason == QSystemTrayIcon::Context)
                trayMenu->popup(QCursor::pos());
#endif
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
