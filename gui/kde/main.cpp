#include "BackendClient.hpp"
#include <QFileInfo>
#include <QApplication>
#include <QAbstractItemView>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QCommandLineParser>
#include <QCursor>
#include <QFile>
#include <QFormLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMainWindow>
#include <QMenu>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSlider>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QToolBar>
#include <QVBoxLayout>
#include <KPageWidget>
#include <KColorSchemeManager>
#include <QAbstractItemModel>
#include <KStatusNotifierItem>
#include <algorithm>
#include <functional>
#include <iostream>
#include <memory>
#include <vector>

class SettingsWindow : public QMainWindow {
public:
    void closeEvent(QCloseEvent* event) override {
        if (QSystemTrayIcon::isSystemTrayAvailable()) { hide(); event->ignore(); }
        else { qApp->quit(); event->accept(); }
    }
};

struct Binding { QJsonObject field; QWidget* widget; QLabel* label; };

class App : public QObject {
public:
    SettingsWindow window;
    QWidget quick{nullptr, Qt::Popup};
    KPageWidget* pages = new KPageWidget;
    QLabel* message = new QLabel;
    QLabel* quickMessage = new QLabel;
    BackendClient backend;
    QPushButton* retry = new QPushButton;
    QJsonObject state, catalog;
    QByteArray pending;
    std::vector<Binding> bindings;
    std::vector<KPageWidgetItem*> pageItems;
    QString locale, error, theme;
    KColorSchemeManager schemes;
    std::vector<std::function<void()>> translations;
    bool syncing = false, live = false, closing = false;
    int sent = 0;
    std::vector<QAction*> deviceActions;

    QString text(const QJsonValue& label) const {
        return label.isObject() ? label.toObject().value(locale).toString(label.toObject().value("en").toString()) : label.toString();
    }
    QString dynamic(const QString& value) const {
        const auto messages = catalog.value("messages").toObject();
        if (messages.contains(value)) return text(messages.value(value));
        for (auto it = messages.begin(); it != messages.end(); ++it) {
            const int index = it.key().indexOf("%1");
            if (index < 0) continue;
            const auto prefix = it.key().left(index), suffix = it.key().mid(index + 2);
            if (value.startsWith(prefix) && value.endsWith(suffix))
                return text(it.value()).replace("%1", value.mid(prefix.size(), value.size() - prefix.size() - suffix.size()));
        }
        return value;
    }
    QString connectionError() const {
        const auto connection = state.value("connection").toObject();
        const auto code = connection.value("errorCode").toString(), detail = connection.value("errorDetail").toString();
        return code.isEmpty() ? detail : dynamic(code) + (detail.isEmpty() ? QString() : ": " + detail);
    }
    QString display(const QJsonValue& value, const QString& key) const {
        if (key == "errorDetail") return connectionError();
        if (value.isBool()) return dynamic(value.toBool() ? "Yes" : "No");
        if (value.isNull() || value.isUndefined()) return "—";
        if (value.isArray()) {
            QStringList names;
            for (const auto item : value.toArray()) names.append(item.toObject().value("name").toString());
            return names.join(" · ");
        }
        const auto string = value.toVariant().toString();
        return key == "state" || key == "errorDetail" || key == "alertMessage" ? dynamic(string) : string;
    }
    QString tr(const char* en, const char* zh) const { return QString::fromUtf8(locale == "zh_CN" ? zh : en); }
    QJsonValue value(const QJsonObject& field) const {
        auto current = state.value(field.value("group").toString()).toObject().value(field.value("key").toString());
        if (field.contains("index")) {
            const auto array = current.toArray(); const auto i = field.value("index").toInt();
            return i < array.size() ? array[i] : QJsonValue();
        }
        return current;
    }
    bool enabled(const QJsonObject& field) const {
        if (field.value("kind") == "label") return true;
        if (!backend.ready) return false;
        if (field.value("group") == "app" || field.value("group") == "preferences") return true;
        auto group = state.value(field.value("group").toString()).toObject();
        const auto cap = field.value("capability").toString();
        return state.value("connection").toObject().value("state") == "connected"
            && (cap.isEmpty() || group.value(cap).toBool())
            && (!field.contains("index") || !value(field).isNull());
    }
    void send(const QString& target, const QString& method, const QJsonArray& args = {}) {
        if (syncing || closing) return;
        backend.send(target, method, args);
        sent = backend.sent;
    }
    void write(const QJsonObject& field, const QJsonValue& value = {}) {
        QJsonArray args;
        if (field.contains("index")) args.append(field.value("index"));
        if (field.value("kind") != "button") args.append(value);
        const auto method = field.value("method").toString();
        if (method == "powerOff" || method == "unpairDevice") {
            const auto question = method == "powerOff" ? tr("Power off headphones?", "是否关闭耳机电源？") : tr("Unpair device?", "是否取消设备配对？");
            if (QMessageBox::question(&window, "Sound Connect", question, QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Yes) return;
        }
        send(field.value("group").toString(), method, args);
    }
    QWidget* makePage(const QJsonObject& spec) {
        auto body = new QWidget;
        auto form = new QFormLayout(body);
        form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
        form->setRowWrapPolicy(QFormLayout::WrapLongRows);
        for (const auto entry : spec.value("fields").toArray()) {
            const auto field = entry.toObject(); const auto kind = field.value("kind").toString();
            const auto label = text(field.value("label"));
            QWidget* widget = nullptr;
            if (kind == "toggle") {
                auto check = new QCheckBox(label); widget = check;
                connect(check, &QCheckBox::clicked, this, [this, field](bool checked) { write(field, checked); });
            } else if (kind == "slider") {
                auto slider = new QSlider(Qt::Horizontal); widget = slider;
                slider->setRange(field.value("min").toInt(), field.value("max").toInt());
                slider->setTracking(false); slider->setTickPosition(QSlider::TicksBelow); slider->setSingleStep(1);
                connect(slider, &QSlider::valueChanged, this, [this, field](int v) { write(field, v); });
            } else if (kind == "choice" || kind == "devices") {
                auto choice = new QComboBox; widget = choice;
                choice->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon); choice->setMinimumContentsLength(12);
                connect(choice, QOverload<int>::of(&QComboBox::activated), this, [this, field, choice, kind](int index) {
                    if (index < 0) return;
                    auto data = QJsonValue::fromVariant(choice->itemData(index));
                    if (kind == "devices") {
                        const auto device = data.toObject(); send("app", "connectDevice", {device.value("name"), device.value("mac")});
                    } else write(field, data);
                });
            } else if (kind == "button") {
                auto button = new QPushButton(label); widget = button;
                connect(button, &QPushButton::clicked, this, [this, field] { write(field); });
            } else {
                auto valueLabel = new QLabel; widget = valueLabel;
                valueLabel->setWordWrap(true); valueLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
            }
            widget->setAccessibleName(label); widget->setToolTip(label);
            if (kind == "toggle" || kind == "button") form->addRow(QString(), widget);
            else form->addRow(label, widget);
            bindings.push_back({field, widget, qobject_cast<QLabel*>(form->labelForField(widget))});
        }
        auto scroll = new QScrollArea; scroll->setWidgetResizable(true); scroll->setFrameShape(QFrame::NoFrame); scroll->setWidget(body);
        return scroll;
    }
    void refresh() {
        syncing = true;
        const auto prefs = state.value("preferences").toObject();
        if (prefs.contains("locale")) locale = prefs.value("locale").toString();
        const auto mode = prefs.value("themeMode").toString("system");
        if (mode != theme) {
            theme = mode;
            // Official KDE schemes, never a private palette. Invalid index restores
            // system colors and KDE's automatic desktop palette updates.
            QModelIndex selected;
            const auto filename = mode == "dark" ? QString("BreezeDark.colors") : QString("BreezeLight.colors");
            if (mode != "system") for (int row = 0; row < schemes.model()->rowCount(); ++row) {
                auto index = schemes.model()->index(row, 0);
                if (index.data(Qt::UserRole).toString().endsWith(filename)) { selected = index; break; }
            }
            schemes.activateScheme(selected);
        }
        for (const auto& update : translations) update();
        for (auto action : deviceActions) action->setEnabled(backend.ready);
        retry->setText(dynamic("backend-retry"));
        retry->setVisible(!backend.ready);
        retry->setEnabled(error != "backend-starting");
        auto status = error.isEmpty() ? (live ? tr("Live device", "实时设备") : tr("Isolated device preview", "独立设备预览")) : dynamic(error);
        if (error.isEmpty() && !connectionError().isEmpty()) status = connectionError();
        if (!backend.ready && !backend.diagnostic.isEmpty()) status += "\n" + backend.diagnostic;
        message->setText(status); quickMessage->setText(status);
        for (const auto& binding : bindings) {
            const auto field = binding.field; auto widget = binding.widget;
            const auto group = state.value(field.value("group").toString()).toObject();
            const auto kind = field.value("kind").toString(); const auto current = value(field);
            QSignalBlocker block(widget); widget->setEnabled(enabled(field));
            const auto label = text(field.value("label"));
            widget->setAccessibleName(label); widget->setToolTip(label);
            if (binding.label) binding.label->setText(label);
            if (auto button = qobject_cast<QAbstractButton*>(widget)) button->setText(label);
            if (auto check = qobject_cast<QCheckBox*>(widget)) check->setChecked(current.toBool());
            if (auto slider = qobject_cast<QSlider*>(widget); slider && !slider->isSliderDown()) slider->setValue(current.toInt());
            if (auto label = qobject_cast<QLabel*>(widget)) {
                label->setText(display(current, field.value("key").toString()));
            }
            if (auto choice = qobject_cast<QComboBox*>(widget)) {
                // Rebuilding an open combobox dismisses it on async snapshots.
                if (choice->view()->isVisible()) continue;
                choice->clear();
                if (kind == "devices") {
                    for (auto item : current.toArray()) choice->addItem(item.toObject().value("name").toString(), item.toVariant());
                    choice->setCurrentIndex(-1); choice->setEnabled(choice->count() > 0 && state.value("connection").toObject().value("state") != "connected" && state.value("connection").toObject().value("state") != "connecting");
                    continue;
                }
                const auto source = field.value("source").toString();
                if (!source.isEmpty()) {
                    for (auto item : group.value(source).toArray()) {
                        const auto entry = item.toObject();
                        choice->addItem(source == "devices" ? entry.value("name").toString() : dynamic(entry.value("name").toString()), entry.value(field.value("valueKey").toString()).toVariant());
                    }
                } else {
                    for (auto item : field.value("options").toArray()) {
                        const auto entry = item.toObject(); const auto cap = entry.value("capability").toString();
                        if (!cap.isEmpty() && !group.value(cap).toBool()) continue;
                        choice->addItem(text(entry.value("label")), entry.value("value").toVariant());
                    }
                }
                choice->setCurrentIndex(choice->findData(current.toVariant()));
            }
        }
        syncing = false;
    }
    void showSettings() { quick.hide(); window.show(); window.raise(); window.activateWindow(); }
    void showQuick(QPoint position = QCursor::pos()) {
        if (quick.isVisible()) { quick.hide(); return; }
        quick.setWindowFlags(QSystemTrayIcon::isSystemTrayAvailable() ? Qt::Popup : Qt::Window);
        quick.adjustSize();
        auto screen = QApplication::screenAt(position); if (!screen) screen = QApplication::primaryScreen();
        const auto rect = screen->availableGeometry();
        quick.move(std::clamp(position.x(), rect.left(), std::max(rect.left(), rect.right() - quick.width())),
                   std::clamp(position.y() - quick.height(), rect.top(), std::max(rect.top(), rect.bottom() - quick.height())));
        quick.show(); quick.activateWindow();
    }
    void start(const QCommandLineParser& options) {
        locale = options.value("locale"); live = options.isSet("live");
        schemes.setAutosaveChanges(false);
        QFile file(options.value("catalog"));
        if (!file.open(QIODevice::ReadOnly)) { throw std::runtime_error("Cannot read binding catalog"); }
        catalog = QJsonDocument::fromJson(file.readAll()).object();
        if (catalog.value("version").toInt() != 1) throw std::runtime_error("Invalid binding catalog");
        auto content = new QWidget; auto layout = new QVBoxLayout(content);
        message->setWordWrap(true); layout->addWidget(message); layout->addWidget(retry); layout->addWidget(pages);
        connect(retry, &QPushButton::clicked, this, [this] { backend.restart(); });
        pages->setFaceType(KPageView::List);
        for (const auto entry : catalog.value("pages").toArray()) {
            const auto page = entry.toObject();
            auto item = pages->addPage(makePage(page), text(page.value("label"))); pageItems.push_back(item);
            translations.push_back([this, item, page] { item->setName(text(page.value("label"))); });
            if (page.value("id").toString() == options.value("route")) pages->setCurrentPage(item);
        }
        window.setCentralWidget(content); window.setWindowTitle("Sound Connect"); window.resize(900, 680);
        auto toolbar = window.addToolBar(tr("Actions", "操作")); toolbar->setMovable(false);
        auto action = toolbar->addAction(tr("Quick controls", "快捷控制")); connect(action, &QAction::triggered, this, [this] { showQuick(); });
        translations.push_back([this, action] { action->setText(tr("Quick controls", "快捷控制")); });
        auto quit = toolbar->addAction(tr("Quit", "退出")); connect(quit, &QAction::triggered, qApp, &QApplication::quit);
        translations.push_back([this, quit] { quit->setText(tr("Quit", "退出")); });
        quick.setWindowTitle(tr("Quick controls", "快捷控制")); quick.setMinimumWidth(410);
        auto quickLayout = new QVBoxLayout(&quick); quickMessage->setWordWrap(true); quickLayout->addWidget(quickMessage);
        auto quickPage = makePage(catalog.value("quick").toObject()); quickPage->setMinimumHeight(280); quickLayout->addWidget(quickPage);
        auto settings = new QPushButton(tr("All settings…", "全部设置…")); quickLayout->addWidget(settings);
        translations.push_back([this, settings] { settings->setText(tr("All settings…", "全部设置…")); quick.setWindowTitle(tr("Quick controls", "快捷控制")); });
        connect(settings, &QPushButton::clicked, this, &App::showSettings);
        auto tray = new KStatusNotifierItem("SoundConnectNativePreview", this);
        tray->setTitle("Sound Connect"); tray->setIconByName("audio-headphones"); tray->setStatus(KStatusNotifierItem::Active);
        tray->setStandardActionsEnabled(false);
        auto menu = new QMenu;
        const auto addMenu = [this, menu](const char* en, const char* zh, std::function<void()> callback) {
            auto action = menu->addAction(tr(en, zh));
            if (QString(en) == "Play / Pause" || QString(en) == "Next track" || QString(en) == "Disconnect") deviceActions.push_back(action);
            connect(action, &QAction::triggered, this, [callback] { callback(); });
            translations.push_back([this, action, en, zh] { action->setText(tr(en, zh)); });
        };
        addMenu("All settings", "全部设置", [this] { showSettings(); });
        addMenu("Quick controls", "快捷控制", [this] { showQuick(); });
        addMenu("Play / Pause", "播放 / 暂停", [this] { send("playback", "togglePlayback"); });
        addMenu("Next track", "下一曲", [this] { send("playback", "nextTrack"); });
        addMenu("Disconnect", "断开连接", [this] { send("app", "disconnectDevice"); });
        addMenu("Quit", "退出", [] { qApp->quit(); });
        tray->setContextMenu(menu);
        for (const auto key : {QString("P"), QString("N"), QString("M")}) {
            auto shortcut = new QAction(this);
            shortcut->setShortcut(QKeySequence("Ctrl+Alt+" + key));
            shortcut->setShortcutContext(Qt::ApplicationShortcut);
            window.addAction(shortcut); quick.addAction(shortcut);
            connect(shortcut, &QAction::triggered, this, [this, key] {
                if (key == "M") showQuick();
                else send("playback", key == "P" ? "togglePlayback" : "nextTrack");
            });
        }
        auto monitor = new QTimer(this);
        connect(monitor, &QTimer::timeout, this, [this] {
            if (!QSystemTrayIcon::isSystemTrayAvailable() && !window.isVisible()) showSettings();
        });
        monitor->start(2000);
        connect(tray, &KStatusNotifierItem::activateRequested, this, [this](bool, const QPoint& p) { showQuick(p); });
        backend.changed = [this] {
            state = backend.state; error = backend.error; sent = backend.sent;
            refresh();
        };
        connect(qApp, &QApplication::aboutToQuit, this, [this] {
            closing = true; backend.shutdown();
        });
        auto arguments = live ? QStringList{"--live"} : QStringList{"--profile", options.value("profile")};
        if (options.isSet("locale")) arguments << "--locale" << options.value("locale");
        if (options.isSet("theme")) arguments << "--theme" << options.value("theme");
        backend.start(options.value("backend"), arguments);
        refresh();
        // Always keep an ordinary window reachable if no tray host is present.
        window.show(); if (options.isSet("panel")) showQuick();
        if (options.isSet("smoke-test")) QTimer::singleShot(3000, this, [this] { smoke(); });
    }
    void smoke() {
        bool ok = state.value("app").toObject().value("previewMode").toBool() && sent == 0 && pageItems.size() == static_cast<size_t>(catalog.value("pages").toArray().size());
        for (auto item : pageItems) pages->setCurrentPage(item);
        for (const auto& b : bindings) {
            if (b.field.value("key") == "dseeEnabled") { qobject_cast<QCheckBox*>(b.widget)->click(); break; }
        }
        ok = ok && sent == 1; refresh(); ok = ok && sent == 1;
        for (const auto& b : bindings) {
            if (b.field.value("key") == "volume") { auto s = qobject_cast<QSlider*>(b.widget); s->setValue(12); s->setValue(12); break; }
        }
        ok = ok && sent == 2;
        const auto targetLocale = locale == "zh_CN" ? QString("en") : QString("zh_CN");
        for (const auto& b : bindings) {
            if (b.field.value("group") != "preferences") continue;
            auto combo = qobject_cast<QComboBox*>(b.widget);
            int index = combo->findData(b.field.value("key") == "locale" ? targetLocale : QString("dark"));
            combo->setCurrentIndex(index);
            QMetaObject::invokeMethod(combo, "activated", Q_ARG(int, index));
        }
        QTimer::singleShot(600, this, [this, ok, targetLocale] {
            bool passed = ok && sent == 4 && locale == targetLocale && theme == "dark"
                && !state.value("preferences").toObject().value("persistent").toBool();
            passed = passed && dynamic("Device function 99") == (locale == "en" ? "Device function 99" : "设备功能 99");
            for (auto action : window.actions()) {
                if (action->shortcut() == QKeySequence("Ctrl+Alt+P")) action->trigger();
            }
            passed = passed && sent == 5;
            passed = passed && qApp->palette().color(QPalette::Window).lightnessF() < 0.5;
            QTimer::singleShot(0, this, [] {
                if (auto dialog = qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) dialog->done(QMessageBox::Cancel);
            });
            write({{"group", "playback"}, {"method", "powerOff"}, {"kind", "button"}});
            passed = passed && sent == 5;
            showQuick(); showSettings();
            passed = passed && !quick.isVisible() && window.isVisible();
            refresh(); passed = passed && sent == 5;
            std::cout << (passed ? "KDE native controls: PASS (preferences, translations, theme, shortcuts, popup, single writes)" : "KDE native controls: FAIL") << std::endl;
            qApp->exit(passed ? 0 : 1);
        });

    }
};

int main(int argc, char** argv) {
    QApplication application(argc, argv);
    application.setApplicationName("SoundConnectKDE"); application.setQuitOnLastWindowClosed(false);
    QCommandLineParser parser; parser.addHelpOption();
    parser.addOption({"backend", "Absolute path to shared host", "path", "/usr/libexec/soundconnect/SoundConnectBackend"});
    const auto localCatalog = QCoreApplication::applicationDirPath() + "/catalog.json";
    parser.addOption({"catalog", "Binding catalog", "path", QFileInfo::exists(localCatalog) ? localCatalog : "/usr/share/soundconnect/clients/common/catalog.json"});
    parser.addOption({"locale", "en or zh_CN", "locale", "en"});
    parser.addOption({"theme", "system, light or dark session override", "theme"});
    parser.addOption({"profile", "Isolated preview profile", "profile", "WH-1000XM6"});
    parser.addOption({"route", "Initial page", "route", "home"});
    parser.addOption({"live", "Enable real Bluetooth"}); parser.addOption({"panel", "Open quick controls"});
    parser.addOption({"smoke-test", "Test native bindings in preview and exit"}); parser.process(application);
    if ((parser.isSet("live") && parser.isSet("smoke-test"))
        || (parser.value("locale") != "en" && parser.value("locale") != "zh_CN")) parser.showHelp(2);
    App app;
    try { app.start(parser); } catch (const std::exception& e) { std::cerr << e.what() << std::endl; return 2; }
    return application.exec();
}
