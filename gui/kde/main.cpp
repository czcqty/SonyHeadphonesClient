#include <QApplication>
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

struct Binding { QJsonObject field; QWidget* widget; };

class App : public QObject {
public:
    SettingsWindow window;
    QWidget quick{nullptr, Qt::Popup};
    KPageWidget* pages = new KPageWidget;
    QLabel* message = new QLabel;
    QLabel* quickMessage = new QLabel;
    QProcess backend;
    QJsonObject state, catalog;
    QByteArray pending;
    std::vector<Binding> bindings;
    std::vector<KPageWidgetItem*> pageItems;
    QString locale, error;
    bool syncing = false, live = false, closing = false;
    int sent = 0;

    QString text(const QJsonValue& label) const {
        return label.isObject() ? label.toObject().value(locale).toString(label.toObject().value("en").toString()) : label.toString();
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
        if (field.value("kind") == "label" || field.value("group") == "app") return true;
        auto group = state.value(field.value("group").toString()).toObject();
        const auto cap = field.value("capability").toString();
        return state.value("connection").toObject().value("state") == "connected"
            && (cap.isEmpty() || group.value(cap).toBool())
            && (!field.contains("index") || !value(field).isNull());
    }
    void send(const QString& target, const QString& method, const QJsonArray& args = {}) {
        if (syncing || closing || backend.state() != QProcess::Running) return;
        auto data = QJsonDocument(QJsonObject{{"target", target}, {"method", method}, {"args", args}}).toJson(QJsonDocument::Compact) + '\n';
        if (backend.write(data) < 0) { error = backend.errorString(); refresh(); }
        else ++sent;
    }
    void write(const QJsonObject& field, const QJsonValue& value = {}) {
        QJsonArray args;
        if (field.contains("index")) args.append(field.value("index"));
        if (field.value("kind") != "button") args.append(value);
        send(field.value("group").toString(), field.value("method").toString(), args);
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
            bindings.push_back({field, widget});
        }
        auto scroll = new QScrollArea; scroll->setWidgetResizable(true); scroll->setFrameShape(QFrame::NoFrame); scroll->setWidget(body);
        return scroll;
    }
    void refresh() {
        syncing = true;
        auto status = error.isEmpty() ? (live ? tr("Live device", "实时设备") : tr("Isolated device preview", "独立设备预览")) : error;
        message->setText(status); quickMessage->setText(status);
        for (const auto& binding : bindings) {
            const auto field = binding.field; auto widget = binding.widget;
            const auto group = state.value(field.value("group").toString()).toObject();
            const auto kind = field.value("kind").toString(); const auto current = value(field);
            QSignalBlocker block(widget); widget->setEnabled(enabled(field));
            if (auto check = qobject_cast<QCheckBox*>(widget)) check->setChecked(current.toBool());
            if (auto slider = qobject_cast<QSlider*>(widget); slider && !slider->isSliderDown()) slider->setValue(current.toInt());
            if (auto label = qobject_cast<QLabel*>(widget)) {
                label->setText(current.isArray() ? QString::fromUtf8(QJsonDocument(current.toArray()).toJson(QJsonDocument::Compact)) : current.toVariant().toString());
            }
            if (auto choice = qobject_cast<QComboBox*>(widget)) {
                choice->clear();
                if (kind == "devices") {
                    for (auto item : current.toArray()) choice->addItem(item.toObject().value("name").toString(), item.toVariant());
                    choice->setCurrentIndex(-1); choice->setEnabled(choice->count() > 0);
                    continue;
                }
                const auto source = field.value("source").toString();
                if (!source.isEmpty()) {
                    for (auto item : group.value(source).toArray()) {
                        const auto entry = item.toObject();
                        choice->addItem(entry.value("name").toString(), entry.value(field.value("valueKey").toString()).toVariant());
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
        quick.adjustSize();
        auto screen = QApplication::screenAt(position); if (!screen) screen = QApplication::primaryScreen();
        const auto rect = screen->availableGeometry();
        quick.move(std::clamp(position.x(), rect.left(), std::max(rect.left(), rect.right() - quick.width())),
                   std::clamp(position.y() - quick.height(), rect.top(), std::max(rect.top(), rect.bottom() - quick.height())));
        quick.show(); quick.activateWindow();
    }
    void start(const QCommandLineParser& options) {
        locale = options.value("locale"); live = options.isSet("live");
        QFile file(options.value("catalog"));
        if (!file.open(QIODevice::ReadOnly)) { throw std::runtime_error("Cannot read binding catalog"); }
        catalog = QJsonDocument::fromJson(file.readAll()).object();
        if (catalog.value("version").toInt() != 1) throw std::runtime_error("Invalid binding catalog");
        auto content = new QWidget; auto layout = new QVBoxLayout(content);
        message->setWordWrap(true); layout->addWidget(message); layout->addWidget(pages);
        pages->setFaceType(KPageView::List);
        for (const auto entry : catalog.value("pages").toArray()) {
            const auto page = entry.toObject();
            auto item = pages->addPage(makePage(page), text(page.value("label"))); pageItems.push_back(item);
            if (page.value("id").toString() == options.value("route")) pages->setCurrentPage(item);
        }
        window.setCentralWidget(content); window.setWindowTitle("Sound Connect"); window.resize(900, 680);
        auto toolbar = window.addToolBar(tr("Actions", "操作")); toolbar->setMovable(false);
        auto action = toolbar->addAction(tr("Quick controls", "快捷控制")); connect(action, &QAction::triggered, this, [this] { showQuick(); });
        auto quit = toolbar->addAction(tr("Quit", "退出")); connect(quit, &QAction::triggered, qApp, &QApplication::quit);
        quick.setWindowTitle(tr("Quick controls", "快捷控制")); quick.setMinimumWidth(410);
        auto quickLayout = new QVBoxLayout(&quick); quickMessage->setWordWrap(true); quickLayout->addWidget(quickMessage);
        auto quickPage = makePage(catalog.value("quick").toObject()); quickPage->setMinimumHeight(280); quickLayout->addWidget(quickPage);
        auto settings = new QPushButton(tr("All settings…", "全部设置…")); quickLayout->addWidget(settings);
        connect(settings, &QPushButton::clicked, this, &App::showSettings);
        auto tray = new KStatusNotifierItem("SoundConnectNativePreview", this);
        tray->setTitle("Sound Connect"); tray->setIconByName("audio-headphones"); tray->setStatus(KStatusNotifierItem::Active);
        tray->setStandardActionsEnabled(false);
        auto menu = new QMenu;
        menu->addAction(tr("All settings", "全部设置"), this, &App::showSettings);
        menu->addAction(tr("Quick controls", "快捷控制"), this, [this] { showQuick(); });
        menu->addAction(tr("Quit", "退出"), qApp, &QApplication::quit); tray->setContextMenu(menu);
        connect(tray, &KStatusNotifierItem::activateRequested, this, [this](bool, const QPoint& p) { showQuick(p); });
        connect(&backend, &QProcess::readyReadStandardOutput, this, [this] {
            pending += backend.readAllStandardOutput();
            while (pending.contains('\n')) {
                const auto index = pending.indexOf('\n');
                const auto event = QJsonDocument::fromJson(pending.left(index)).object(); pending.remove(0, index + 1);
                if (event.value("type") == "state") state = event;
                if (event.value("type") == "result") error = event.value("ok").toBool() ? QString() : event.value("error").toString();
                refresh();
            }
        });
        connect(&backend, &QProcess::readyReadStandardError, this, [this] { backend.readAllStandardError(); });
        connect(&backend, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) { error = backend.errorString(); refresh(); });
        connect(&backend, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, [this](int, QProcess::ExitStatus) {
            if (!closing) { error = tr("Backend stopped", "后端已停止"); state = {}; refresh(); }
        });
        connect(qApp, &QApplication::aboutToQuit, this, [this] {
            closing = true; backend.closeWriteChannel();
            if (!backend.waitForFinished(2000)) { backend.kill(); backend.waitForFinished(1000); }
        });
        backend.start(options.value("backend"), live ? QStringList{"--live"} : QStringList{"--profile", options.value("profile")});
        refresh();
        // Always keep an ordinary window reachable if no tray host is present.
        window.show(); if (options.isSet("panel")) showQuick();
        if (options.isSet("smoke-test")) QTimer::singleShot(3000, this, [this] { smoke(); });
    }
    void smoke() {
        bool ok = state.value("app").toObject().value("previewMode").toBool() && sent == 0 && pageItems.size() == 9;
        for (auto item : pageItems) pages->setCurrentPage(item);
        for (const auto& b : bindings) {
            if (b.field.value("key") == "dseeEnabled") { qobject_cast<QCheckBox*>(b.widget)->click(); break; }
        }
        ok = ok && sent == 1; refresh(); ok = ok && sent == 1;
        for (const auto& b : bindings) {
            if (b.field.value("key") == "volume") { auto s = qobject_cast<QSlider*>(b.widget); s->setValue(12); s->setValue(12); break; }
        }
        ok = ok && sent == 2;
        std::cout << (ok ? "KDE native controls: PASS (all pages, startup isolation, toggle/slider writes)" : "KDE native controls: FAIL") << std::endl;
        qApp->exit(ok ? 0 : 1);
    }
};

int main(int argc, char** argv) {
    QApplication application(argc, argv);
    application.setApplicationName("SoundConnectKDE"); application.setQuitOnLastWindowClosed(false);
    QCommandLineParser parser; parser.addHelpOption();
    parser.addOption({"backend", "Absolute path to shared host", "path"});
    parser.addOption({"catalog", "Binding catalog", "path", QCoreApplication::applicationDirPath() + "/catalog.json"});
    parser.addOption({"locale", "en or zh_CN", "locale", "zh_CN"});
    parser.addOption({"profile", "Isolated preview profile", "profile", "WH-1000XM6"});
    parser.addOption({"route", "Initial page", "route", "home"});
    parser.addOption({"live", "Enable real Bluetooth"}); parser.addOption({"panel", "Open quick controls"});
    parser.addOption({"smoke-test", "Test native bindings in preview and exit"}); parser.process(application);
    if (!parser.isSet("backend") || (parser.isSet("live") && parser.isSet("smoke-test"))
        || (parser.value("locale") != "en" && parser.value("locale") != "zh_CN")) parser.showHelp(2);
    App app;
    try { app.start(parser); } catch (const std::exception& e) { std::cerr << e.what() << std::endl; return 2; }
    return application.exec();
}
