#include "BackendClient.hpp"
#include <QFileInfo>
#include <QApplication>
#include <QAbstractItemView>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QCommandLineParser>
#include "PanelPlacement.hpp"
#include "PlasmaPanel.hpp"
#include <QStandardPaths>
#include <QLineEdit>
#include <QCursor>
#include <QKeyEvent>
#include <QPointer>
#include <QToolButton>
#include <QWindow>
#include <QStyle>
#include <QSpinBox>
#include <QElapsedTimer>
#include <QSet>
#include <QThread>
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
    std::function<void(bool)> compactNavigation;
    void resizeEvent(QResizeEvent* event) override {
        QMainWindow::resizeEvent(event);
        if (compactNavigation) compactNavigation(width() < 620);
    }
    void closeEvent(QCloseEvent* event) override {
        if (QSystemTrayIcon::isSystemTrayAvailable()) { hide(); event->ignore(); }
        else { qApp->quit(); event->accept(); }
    }
};

class QuickWindow : public QWidget {
public:
    QuickWindow() : QWidget(nullptr, Qt::Window | Qt::FramelessWindowHint) {}
protected:
    void closeEvent(QCloseEvent* event) override { event->ignore(); }
};

struct Binding { QString page; QJsonObject field; QWidget* widget; QWidget* row; QLabel* label; QSpinBox* number; };

class App : public QObject {
public:
    SettingsWindow window;
    QuickWindow quick;
    PlasmaPanel plasmaPanel;
    QToolButton* closeQuick = new QToolButton;
    QPointer<QScreen> trayScreen;
    QPoint trayPoint;
    KStatusNotifierItem* tray = nullptr;
    KPageWidget* pages = new KPageWidget;
    QComboBox* compactPages = new QComboBox;
    QLabel* message = new QLabel;
    QLabel* quickMessage = new QLabel;
    BackendClient backend;
    QPushButton* retry = new QPushButton;
    QJsonObject state, catalog;
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
            for (const auto item : value.toArray()) {
                const auto device = item.toObject();
                auto description = device.value("name").toString();
                if (device.contains("connected")) description += " · " + dynamic(device.value("connected").toBool() ? "connected" : "disconnected");
                if (device.value("playback").toBool()) description += " · " + tr("Playback", "播放");
                names.append(description);
            }
            return names.join(" · ");
        }
        const auto string = value.toVariant().toString();
        return key == "state" || key == "errorDetail" || key == "alertMessage" ? dynamic(string) : string;
    }
    QString tr(const char* en, const char* zh) const { return QString::fromUtf8(locale == "zh_CN" ? zh : en); }
    QJsonValue value(const QJsonObject& field) const {
        const auto group = state.value(field.value("group").toString()).toObject();
        if (field.contains("knownKey") && !group.value(field.value("knownKey").toString()).toBool()) return {};
        auto current = group.value(field.value("key").toString());
        if (field.contains("index")) {
            const auto array = current.toArray(); const auto i = field.value("index").toInt();
            return i < array.size() ? array[i] : QJsonValue();
        }
        return current;
    }
    bool flag(const char* group, const char* key) const {
        return state.value(group).toObject().value(key).toBool();
    }
    QString unavailable(const QJsonObject& field) const {
        if (field.value("kind") == "label") return {};
        if (!backend.ready) return tr("Waiting for the backend.", "正在等待后端。");
        const auto connection = state.value("connection").toObject().value("state").toString();
        const bool busy = connection == "connecting" || connection == "initializing" || connection == "scanning";
        const bool connected = connection == "connected";
        const auto method = field.value("method").toString();
        if (field.value("group") == "preferences") return {};
        if (field.value("group") == "app" || field.value("kind") == "devices") {
            if (method == "disconnectDevice") return busy || connected ? QString() : tr("No active connection.", "当前没有活动的连接。");
            if (busy || connected) return tr("Cancel or disconnect the current connection first.", "请先取消或断开当前连接。");
            if (field.value("kind") == "devices" && value(field).toArray().isEmpty())
                return tr("No devices found.", "未找到设备。");
            return {};
        }
        if (!connected) return tr("Connect headphones on the Connection page first.", "请先在“连接”页连接耳机。");
        const auto group = state.value(field.value("group").toString()).toObject();
        const auto cap = field.value("capability").toString();
        if (!cap.isEmpty() && !group.value(cap).toBool()) return tr("Unsupported by these headphones.", "此耳机不支持该功能。");
        if (field.contains("index") && value(field).isNull()) return tr("Band unavailable on these headphones.", "此耳机未提供该频段。");
        if (method == "powerOff" && group.value("powerOffPending").toBool()) return tr("Powering off headphones.", "正在关闭耳机电源。");
        if (field.value("source") == "devices") {
            const auto devices = group.value("devices").toArray();
            if (std::none_of(devices.begin(), devices.end(), [&](const QJsonValue& d) { return deviceActionAvailable(method, d.toObject()); }))
                return tr("No eligible paired devices.", "没有适用的已配对设备。");
        }
        return {};
    }
    static bool deviceActionAvailable(const QString& method, const QJsonObject& device) {
        const bool connected = device.value("connected").toBool();
        if (method == "connectDevice") return !connected;
        if (method == "disconnectDevice") return connected;
        if (method == "switchDevice") return connected && !device.value("playback").toBool();
        return true;
    }
    bool visible(const QJsonObject& field) const {
        const auto key = field.value("key").toString();
        if (key == "touchLeft" || key == "touchRight") return !flag("settings", "touchPanelSupported") || flag("settings", "touchSupported");
        if (key == "adaptiveEnabled") return flag("noiseControl", "adaptiveSupported");
        if (key == "ambientLevel" || key == "focusOnVoice") return state.value("noiseControl").toObject().value("mode").toInt() == 2;
        if (key == "adaptiveSensitivity") return flag("noiseControl", "adaptiveEnabled");
        if (key == "listeningRoomSize") return flag("settings", "listeningModeEnabled");
        if (key == "speakToChatSensitivity" || key == "speakToChatModeOut") return flag("settings", "speakToChatEnabled");
        if (key == "batteryRight" || key == "batteryRightCharging") return flag("playback", "hasRightBattery");
        if (key == "batteryCase" || key == "batteryCaseCharging") return flag("playback", "hasCaseBattery");
        if (key == "safeListeningPressure") return flag("playback", "safeListeningSupported");
        return true;
    }
    void send(const QString& target, const QString& method, const QJsonArray& args = {}) {
        if (syncing || closing) return;
        backend.send(target, method, args);
        sent = backend.sent;
    }
    void write(const QJsonObject& field, const QJsonValue& value = {}) {
        if (!unavailable(field).isEmpty()) return;
        QJsonArray args;
        if (field.contains("index")) args.append(field.value("index"));
        if (field.value("kind") != "button") args.append(value);
        const auto method = field.value("method").toString();
        if (method == "powerOff" || method == "unpairDevice") {
            const auto question = method == "powerOff" ? tr("Power off headphones?", "是否关闭耳机电源？") : tr("Unpair device?", "是否取消设备配对？");
            QMessageBox dialog(QMessageBox::Question, "Sound Connect", question, QMessageBox::Yes | QMessageBox::Cancel, &window);
            dialog.setDefaultButton(QMessageBox::Cancel);
            dialog.button(QMessageBox::Yes)->setText(tr("Yes", "是"));
            dialog.button(QMessageBox::Cancel)->setText(tr("Cancel", "取消"));
            if (dialog.exec() != QMessageBox::Yes) return;
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
            QWidget* row = nullptr;
            QSpinBox* number = nullptr;
            if (kind == "toggle") {
                auto check = new QCheckBox(label); widget = check;
                connect(check, &QCheckBox::clicked, this, [this, field](bool checked) { write(field, checked); });
            } else if (kind == "slider") {
                auto slider = new QSlider(Qt::Horizontal); widget = slider;
                slider->setRange(field.value("min").toInt(), field.value("max").toInt());
                slider->setTracking(false); slider->setTickPosition(QSlider::TicksBelow); slider->setSingleStep(1);
                number = new QSpinBox;
                number->setRange(slider->minimum(), slider->maximum()); number->setKeyboardTracking(false);
                row = new QWidget;
                auto layout = new QHBoxLayout(row); layout->setContentsMargins(0, 0, 0, 0);
                layout->addWidget(slider, 1); layout->addWidget(number);
                connect(slider, &QSlider::valueChanged, this, [this, field](int v) { write(field, v); });
                connect(number, QOverload<int>::of(&QSpinBox::valueChanged), this, [this, field](int v) { write(field, v); });
            } else if (kind == "choice" || kind == "devices") {
                auto choice = new QComboBox; widget = choice;
                choice->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon); choice->setMinimumContentsLength(12);
                connect(choice, QOverload<int>::of(&QComboBox::activated), this, [this, field, choice, kind](int index) {
                    if (index < 0 || !unavailable(field).isEmpty()) return;
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
            if (!row) row = widget;
            if (kind == "toggle" || kind == "button") form->addRow(row);
            else form->addRow(label, row);
            bindings.push_back({spec.value("id").toString(), field, widget, row, qobject_cast<QLabel*>(form->labelForField(row)), number});
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
        for (auto action : deviceActions) {
            const bool playback = action->data().toString() == "playback";
            const QJsonObject field{{"group", playback ? "playback" : "app"},
                {"method", playback ? "togglePlayback" : "disconnectDevice"},
                {"capability", playback ? "playbackSupported" : ""}};
            action->setEnabled(unavailable(field).isEmpty());
        }
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
            QSignalBlocker block(widget);
            const auto reason = unavailable(field);
            const bool shown = visible(field);
            binding.row->setVisible(shown); binding.row->setEnabled(reason.isEmpty());
            if (binding.label) binding.label->setVisible(shown);
            const auto label = text(field.value("label"));
            widget->setAccessibleName(label); widget->setAccessibleDescription(reason); widget->setToolTip(reason.isEmpty() ? label : reason);
            if (binding.label) binding.label->setToolTip(reason);
            if (binding.label) binding.label->setText(label);
            if (auto button = qobject_cast<QAbstractButton*>(widget)) button->setText(label);
            if (auto check = qobject_cast<QCheckBox*>(widget)) check->setChecked(current.toBool());
            if (auto slider = qobject_cast<QSlider*>(widget); slider && !slider->isSliderDown()) slider->setValue(current.toInt());
            if (binding.number) {
                QSignalBlocker numberBlock(binding.number);
                if (!binding.number->hasFocus()) binding.number->setValue(current.toInt());
                binding.number->setAccessibleName(label); binding.number->setToolTip(widget->toolTip());
            }
            if (auto label = qobject_cast<QLabel*>(widget)) {
                label->setText(display(current, field.value("key").toString()));
            }
            if (auto choice = qobject_cast<QComboBox*>(widget)) {
                // Rebuilding an open combobox dismisses it on async snapshots.
                if (choice->view()->isVisible()) continue;
                choice->clear();
                if (kind == "devices") {
                    for (auto item : current.toArray()) choice->addItem(item.toObject().value("name").toString(), item.toVariant());
                    choice->setCurrentIndex(-1);
                    continue;
                }
                const auto source = field.value("source").toString();
                if (!source.isEmpty()) {
                    for (auto item : group.value(source).toArray()) {
                        const auto entry = item.toObject();
                        if (source == "devices" && !deviceActionAvailable(field.value("method").toString(), entry)) continue;
                        choice->addItem(source == "devices" ? entry.value("name").toString() : dynamic(entry.value("name").toString()), entry.value(field.value("valueKey").toString()).toVariant());
                    }
                } else {
                    for (auto item : field.value("options").toArray()) {
                        const auto entry = item.toObject(); const auto cap = entry.value("capability").toString();
                        if (!cap.isEmpty() && !group.value(cap).toBool()) continue;
                        choice->addItem(text(entry.value("label")), entry.value("value").toVariant());
                    }
                }
                if (field.contains("knownKey"))
                    choice->setPlaceholderText(tr("State not reported; choose Off or On", "状态尚未确认，请选择关或开"));
                choice->setCurrentIndex(current.isNull() || current.isUndefined() ? -1 : choice->findData(current.toVariant()));
            }
        }
        syncing = false;
    }
    void showSettings() { window.showNormal(); window.raise(); window.activateWindow(); }

    void rememberTray(QPoint point) {
        if (auto screen = QGuiApplication::screenAt(point)) { trayScreen = screen; trayPoint = point; }
    }
    void showQuick() {
        // StatusNotifierItem supplies a position on activation. In its XEmbed
        // fallback the child Qt tray icon also exposes the current geometry.
        if (auto legacy = tray->findChild<QSystemTrayIcon*>(); legacy && legacy->geometry().isValid())
            rememberTray(legacy->geometry().center());
        QScreen* screen = trayScreen;
        if (!screen) {
            screen = window.screen();
            if (!screen) screen = QGuiApplication::primaryScreen();
            // A shortcut can be used before the first StatusNotifier activation.
            // Prefer the settings display if it has a panel, otherwise the first
            // display with desktop-reserved space. Never follow the pointer.
            if (screen && PlasmaPanel::workArea(screen) == screen->geometry())
                for (auto candidate : QGuiApplication::screens()) {
                    if (PlasmaPanel::workArea(candidate) != candidate->geometry()) { screen = candidate; break; }
                }
        }
        QPoint origin;
        if (screen) {
            const auto work = PlasmaPanel::workArea(screen);
            const auto anchor = trayScreen ? trayPoint : defaultTrayCorner(screen->geometry(), work);
            quick.winId();
            if (quick.windowHandle()->screen() != screen) quick.windowHandle()->setScreen(screen);
            quick.resize(QSize(430, 480).boundedTo(work.size()));
            origin = quickPanelOrigin(work, quick.size(), anchor);
            if (!QGuiApplication::platformName().startsWith("wayland")) quick.move(origin);
        }
        quick.showNormal();
        if (screen) plasmaPanel.place(quick.windowHandle(), origin);
        quick.raise(); quick.activateWindow();
    }

    QJsonObject quickSpec() const {
        auto quick = catalog.value("quick").toObject();
        auto fields = quick.value("fields").toArray();
        for (const auto entry : catalog.value("pages").toArray()) {
            for (const auto field : entry.toObject().value("fields").toArray()) {
                const auto method = field.toObject().value("method").toString();
                if (method == "previousTrack" || method == "nextTrack" || method == "setAmbientLevel" || method == "setFocusOnVoice")
                    fields.append(field);
            }
        }
        quick["fields"] = fields;
        quick["id"] = "quick";
        return quick;
    }
    void start(const QCommandLineParser& options) {
        locale = options.value("locale"); live = options.isSet("live");
        schemes.setAutosaveChanges(false);
        QFile file(options.value("catalog"));
        if (!file.open(QIODevice::ReadOnly)) { throw std::runtime_error("Cannot read binding catalog"); }
        catalog = QJsonDocument::fromJson(file.readAll()).object();
        if (catalog.value("version").toInt() != 1) throw std::runtime_error("Invalid binding catalog");
        auto content = new QWidget; auto layout = new QVBoxLayout(content);
        message->setWordWrap(true); layout->addWidget(message); layout->addWidget(retry); layout->addWidget(compactPages); layout->addWidget(pages);
        connect(retry, &QPushButton::clicked, this, [this] { backend.restart(); });
        pages->setFaceType(KPageView::FlatList);
        if (auto search = qobject_cast<QLineEdit*>(pages->focusProxy()))
            translations.push_back([this, search] { search->setPlaceholderText(tr("Search…", "搜索…")); });
        const QHash<QString, QString> pageIcons{
            {"home", "go-home"}, {"connection", "network-bluetooth"}, {"sound", "audio-volume-high"},
            {"noise", "audio-volume-muted"}, {"equalizer", "view-media-equalizer"},
            {"behavior", "preferences-desktop-accessibility"}, {"touch", "input-touchpad"},
            {"voice", "audio-input-microphone"}, {"devices", "audio-headphones"}, {"application", "configure"}
        };
        for (const auto entry : catalog.value("pages").toArray()) {
            const auto page = entry.toObject();
            auto item = pages->addPage(makePage(page), text(page.value("label"))); pageItems.push_back(item);
            item->setIcon(QIcon::fromTheme(pageIcons.value(page.value("id").toString())));
            const int index = compactPages->count();
            compactPages->addItem(item->icon(), item->name());
            translations.push_back([this, item, page, index] {
                const auto label = text(page.value("label"));
                item->setName(label); compactPages->setItemText(index, label);
            });
            if (page.value("id").toString() == options.value("route")) pages->setCurrentPage(item);
        }
        compactPages->hide();
        connect(compactPages, QOverload<int>::of(&QComboBox::activated), this, [this](int index) {
            if (index >= 0) pages->setCurrentPage(pageItems.at(index));
        });
        connect(pages, &KPageWidget::currentPageChanged, this, [this](KPageWidgetItem* current, KPageWidgetItem*) {
            auto item = std::find(pageItems.begin(), pageItems.end(), current);
            if (item != pageItems.end()) compactPages->setCurrentIndex(int(item - pageItems.begin()));
        });
        window.compactNavigation = [this](bool compact) {
            if (compact == !compactPages->isHidden()) return;
            auto current = pages->currentPage();
            pages->setFaceType(compact ? KPageView::Plain : KPageView::FlatList);
            pages->setCurrentPage(current);
            compactPages->setCurrentIndex(int(std::find(pageItems.begin(), pageItems.end(), current) - pageItems.begin()));
            compactPages->setVisible(compact);
        };
        translations.push_back([this] { compactPages->setAccessibleName(tr("Settings page", "设置页面")); });
        window.setCentralWidget(content); window.setWindowTitle("Sound Connect"); window.resize(900, 680);
        auto toolbar = window.addToolBar(tr("Actions", "操作")); toolbar->setMovable(false);
        auto brand = new QLabel;
        brand->setPixmap(qApp->windowIcon().pixmap(24, 24)); brand->setAccessibleName("Sound Connect");
        toolbar->addWidget(brand);
        auto action = toolbar->addAction(tr("Quick controls", "快捷控制")); connect(action, &QAction::triggered, this, [this] { showQuick(); });
        translations.push_back([this, action] { action->setText(tr("Quick controls", "快捷控制")); });
        auto quit = toolbar->addAction(tr("Quit", "退出")); connect(quit, &QAction::triggered, qApp, &QApplication::quit);
        translations.push_back([this, quit] { quit->setText(tr("Quit", "退出")); });
        auto quickLayout = new QVBoxLayout(&quick);
        auto header = new QHBoxLayout;
        auto quickIcon = new QLabel;
        quickIcon->setPixmap(qApp->windowIcon().pixmap(24, 24)); quickIcon->setAccessibleName("Sound Connect");
        auto title = new QLabel;
        closeQuick->setIcon(quick.style()->standardIcon(QStyle::SP_TitleBarCloseButton));
        header->addWidget(quickIcon); header->addWidget(title); header->addStretch(); header->addWidget(closeQuick);
        quickLayout->addLayout(header);
        connect(closeQuick, &QToolButton::clicked, &quick, &QWidget::hide);
        quickMessage->setWordWrap(true); quickLayout->addWidget(quickMessage);
        quickLayout->addWidget(makePage(quickSpec()), 1);
        auto settings = new QPushButton; quickLayout->addWidget(settings);
        translations.push_back([this, settings, title] {
            settings->setText(tr("All settings…", "全部设置…")); quick.setWindowTitle(tr("Quick controls", "快捷控制"));
            title->setText(quick.windowTitle());
            closeQuick->setAccessibleName(tr("Close", "关闭")); closeQuick->setToolTip(closeQuick->accessibleName());
        });
        connect(settings, &QPushButton::clicked, this, &App::showSettings);
        tray = new KStatusNotifierItem("SoundConnectKDE", this);
        tray->setTitle("Sound Connect"); tray->setIconByPixmap(qApp->windowIcon()); tray->setStatus(KStatusNotifierItem::Active);
        tray->setStandardActionsEnabled(false);
        auto menu = new QMenu;
        const auto addMenu = [this, menu](const char* en, const char* zh, std::function<void()> callback) {
            auto action = menu->addAction(tr(en, zh));
            if (QString(en) == "Play / Pause" || QString(en) == "Next track" || QString(en) == "Disconnect") {
                action->setData(QString(en) == "Disconnect" ? "connection" : "playback");
                deviceActions.push_back(action);
            }
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
                else if (backend.ready && flag("playback", "playbackSupported") && state.value("connection").toObject().value("state") == "connected")
                    send("playback", key == "P" ? "togglePlayback" : "nextTrack");
            });
        }
        auto monitor = new QTimer(this);
        connect(monitor, &QTimer::timeout, this, [this] {
            if (!QSystemTrayIcon::isSystemTrayAvailable() && !window.isVisible()) showSettings();
        });
        monitor->start(2000);
        connect(tray, &KStatusNotifierItem::activateRequested, this, [this](bool, const QPoint& p) { if (!p.isNull()) rememberTray(p); showQuick(); });
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
        if (options.isSet("parity-test")) QTimer::singleShot(0, this, [this, executable = options.value("backend")] { parity(executable); });
        if (options.isSet("smoke-test")) QTimer::singleShot(3000, this, [this] { smoke(); });
    }
    void parity(const QString& executable);
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
            if (!passed) std::cerr << "Smoke: preference/shortcut/confirmation failure, initial=" << ok << ", requests=" << sent
                << ", language=" << locale.toStdString() << ", theme=" << theme.toStdString()
                << ", window lightness=" << qApp->palette().color(QPalette::Window).lightnessF() << std::endl;
            const auto screen = window.screen();
            const auto work = PlasmaPanel::workArea(screen);
            window.resize(510, 520);
            passed = passed && compactPages->isVisible() && compactPages->count() == int(pageItems.size());
            for (int row = 0; row < compactPages->count(); ++row) {
                QMetaObject::invokeMethod(compactPages, "activated", Q_ARG(int, row));
                passed = passed && pages->currentPage() == pageItems.at(row)
                    && compactPages->itemText(row) == pageItems.at(row)->name() && !compactPages->itemIcon(row).pixmap(24, 24).isNull();
            }
            window.resize(900, 680);
            passed = passed && compactPages->isHidden();
            tray->activateRequested(true, screen->geometry().bottomRight());
            for (auto item : pageItems) if (item->icon().pixmap(24, 24).isNull()) {
                std::cerr << "Smoke: missing page icon for " << item->name().toStdString() << std::endl; passed = false;
            }
            const auto panelId = quick.winId();
            const auto openedAt = quick.pos();
            QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
            for (const auto& b : bindings) {
                if (b.page != "quick" || b.field.value("key") != "mode") continue;
                auto combo = qobject_cast<QComboBox*>(b.widget);
                combo->showPopup();
                passed = passed && combo->view()->isVisible();
                QApplication::sendEvent(combo->view(), &escape);
                passed = passed && !combo->view()->isVisible() && quick.isVisible();
            }
            QApplication::sendEvent(&quick, &escape);
            quick.close();
            passed = passed && quick.isVisible();
            showSettings();
            passed = passed && quick.isVisible();
            for (auto action : window.actions())
                if (action->shortcut() == QKeySequence("Ctrl+Alt+M")) action->trigger();
            passed = passed && quick.isVisible() && window.isVisible() && quick.winId() == panelId
                && quick.pos() == openedAt && !quick.windowIcon().isNull() && !window.windowIcon().isNull();
            if (QGuiApplication::platformName() == "xcb" && quick.pos() != quickPanelOrigin(work, quick.size(), screen->geometry().bottomRight())) {
                std::cerr << "Smoke: wrong panel corner (" << quick.x() << ", " << quick.y() << ")" << std::endl;
                passed = false;
            }
            closeQuick->click();
            passed = passed && !quick.isVisible() && window.isVisible();
            refresh(); passed = passed && sent == 5;
            std::cout << (passed ? "KDE native controls: PASS (preferences, translations, theme, shortcuts, persistent panel, compact navigation, icons, single writes)" : "KDE native controls: FAIL") << std::endl;
            qApp->exit(passed ? 0 : 1);
        });

    }
};

#include "ParityTests.inc"

int main(int argc, char** argv) {
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif
    QApplication application(argc, argv);
    application.setDesktopFileName("io.github.czcqty.SoundConnect.kde");
    auto iconPaths = QIcon::themeSearchPaths() + QStandardPaths::locateAll(QStandardPaths::GenericDataLocation, "icons", QStandardPaths::LocateDirectory);
    iconPaths.removeDuplicates(); QIcon::setThemeSearchPaths(iconPaths);
    if (QIcon::themeName().isEmpty()) QIcon::setThemeName("breeze");
    if (QIcon::fallbackThemeName().isEmpty() || QIcon::fallbackThemeName() == "hicolor") QIcon::setFallbackThemeName("breeze");
    application.setWindowIcon(QIcon(":/icons/soundconnect.png"));
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
    parser.addOption({"parity-test", "Exercise the complete native preview contract and exit"});
    parser.addOption({"smoke-test", "Test native bindings in preview and exit"}); parser.process(application);
    if (((parser.isSet("live") || parser.isSet("smoke-test")) && parser.isSet("parity-test"))
        || (parser.isSet("live") && parser.isSet("smoke-test"))
        || (parser.value("locale") != "en" && parser.value("locale") != "zh_CN")) parser.showHelp(2);
    App app;
    try { app.start(parser); } catch (const std::exception& e) { std::cerr << e.what() << std::endl; return 2; }
    return application.exec();
}
