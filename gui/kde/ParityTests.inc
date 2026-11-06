// The installed --parity-test entry exercises this same process's widgets. The
// backend is restarted in isolated preview for each advertised profile.
void App::parity(const QString& executable)
{
    const auto require = [](bool condition, const QString& message) {
        if (!condition) throw std::runtime_error(message.toStdString());
    };
    const auto until = [&](const std::function<bool()>& condition, const QString& message) {
        QElapsedTimer timer;
        timer.start();
        while (!condition() && timer.elapsed() < 12000) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
            QThread::msleep(1);
        }
        require(condition(), message + " (" + backend.error + ")");
    };
    QString step = "startup";
    QSet<QString> exercised, expected;
    QJsonObject result;
    QString snapshotAfter;
    backend.received = [&](const QJsonObject& event) {
        if (event.value("type") == "result") result = event;
        if (event.value("type") == "state") snapshotAfter = result.value("id").toString();
    };
    const auto edit = [&](const QString& command, const std::function<void()>& activate) {
        const int before = backend.sent;
        activate();
        require(backend.sent == before + 1, step + ": expected exactly one " + command);
        until([&] { return snapshotAfter == QString::number(before + 1); }, "Missing post-command snapshot: " + command);
        require(result.value("ok").toBool() && backend.sent == before + 1, step + ": " + command + ": " + result.value("error").toString());
        exercised.insert(command);
    };
    const auto bindingFor = [&](const QString& page, const QString& key) -> const Binding& {
        for (const auto& b : bindings)
            if (b.page == page && (b.field.value("key") == key || b.field.value("method") == key)) return b;
        throw std::runtime_error(("Missing control: " + page + "/" + key).toStdString());
    };
    const auto navigate = [&](const QString& page) {
        const auto specs = catalog.value("pages").toArray();
        for (int i = 0; i < specs.size(); ++i) {
            if (specs.at(i).toObject().value("id") != page) continue;
            pages->setCurrentPage(pageItems.at(i));
            QCoreApplication::processEvents();
            return;
        }
        throw std::runtime_error(("Missing page: " + page).toStdString());
    };
    const auto choose = [&](QComboBox* combo, int index) {
        require(index >= 0 && index < combo->count(), "Picker value is unavailable");
        combo->setCurrentIndex(index);
        QMetaObject::invokeMethod(combo, "activated", Q_ARG(int, index));
    };
    const auto confirm = [](QMessageBox::StandardButton response) {
        QTimer::singleShot(0, [response] {
            if (auto box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) box->button(response)->click();
        });
    };
    const auto change = [&](const Binding& b, const QJsonValue& target, bool numeric = false) {
        const auto method = b.field.value("method").toString();
        const auto command = b.field.value("group").toString() + "." + method;
        require(b.row->isVisible() && b.widget->isEnabled(), step + ": unavailable control " + method);
        edit(command, [&] {
            if (auto check = qobject_cast<QCheckBox*>(b.widget)) check->click();
            else if (numeric) b.number->setValue(target.toInt());
            else if (auto slider = qobject_cast<QSlider*>(b.widget)) slider->setValue(target.toInt());
            else if (auto combo = qobject_cast<QComboBox*>(b.widget)) choose(combo, combo->findData(target.toVariant()));
            else qobject_cast<QPushButton*>(b.widget)->click();
        });
        if (!b.field.value("key").toString().isEmpty() && !target.isNull())
            require(value(b.field) == target, step + ": readback mismatch for " + method);
    };
    const auto prepare = [&](const QString& page, const QString& key, const QJsonValue& target) {
        const auto& b = bindingFor(page, key);
        if (b.widget->isEnabled() && value(b.field) != target) {
            auto choice = qobject_cast<QComboBox*>(b.widget);
            if (!choice || choice->findData(target.toVariant()) >= 0) change(b, target);
        }
    };
    const auto startPreview = [&](const QString& path, const QString& profile, const QString& language) {
        backend.start(path, {"--profile", profile, "--locale", language, "--theme", "system"});
        require(!backend.ready && state.isEmpty(), "Restart retained a stale device snapshot");
    };
    try {
        until([&] { return backend.ready; }, "Backend did not start");
        require(!live && flag("app", "previewMode") && backend.sent == 0, "Parity requires an isolated preview");
        const auto language = locale;
        edit("app.describe", [&] { send("app", "describe"); });
        exercised.remove("app.describe");
        const auto contract = result.value("contract").toObject();
        const auto profiles = contract.value("profiles").toArray();
        require(profiles.size() == 12, "Expected twelve device profiles");
        for (const auto entry : contract.value("commands").toArray()) {
            const auto command = entry.toObject();
            expected.insert(command.value("target").toString() + "." + command.value("method").toString());
        }
        for (const auto profile : profiles) {
            step = profile.toString();
            startPreview(executable, step, language);
            until([&] { return backend.ready; }, "Could not start " + step);
            const auto startCount = backend.sent;
            for (const auto entry : catalog.value("pages").toArray()) {
                const auto page = entry.toObject().value("id").toString();
                const int before = backend.sent;
                navigate(page);
                refresh();
                require(before == backend.sent, "Page refresh sent a command: " + page);
                if (page == "connection" || page == "application") continue;
                for (const auto& b : bindings) {
                    if (b.page != page) continue;
                    const auto method = b.field.value("method").toString();
                    if (method.isEmpty() || method == "powerOff" || b.field.value("source") == "devices") continue;
                    if (b.field.value("key") == "listeningRoomSize") prepare(page, "listeningModeEnabled", true);
                    if (b.field.value("key") == "adaptiveSensitivity") prepare(page, "adaptiveEnabled", true);
                    if (b.field.value("key") == "ambientLevel" || b.field.value("key") == "focusOnVoice") prepare(page, "mode", 2);
                    if (method == "setSpeakToChatSensitivity" || method == "setSpeakToChatModeOut") prepare(page, "speakToChatEnabled", true);
                    const auto cap = b.field.value("capability").toString();
                    const auto group = state.value(b.field.value("group").toString()).toObject();
                    const bool supported = (cap.isEmpty() || group.value(cap).toBool()) && (!b.field.contains("index") || !value(b.field).isNull());
                    if (!supported) {
                        require(!b.widget->isEnabled() && !b.widget->accessibleDescription().isEmpty(), "Unsupported control lacks a reason: " + method);
                        continue;
                    }
                    const auto original = value(b.field);
                    if (auto combo = qobject_cast<QComboBox*>(b.widget)) {
                        for (int i = 0; i < combo->count(); ++i) {
                            const auto target = QJsonValue::fromVariant(combo->itemData(i));
                            if (target != value(b.field)) change(b, target);
                        }
                        if (!original.isNull() && original != value(b.field)) change(b, original);
                        combo->showPopup();
                        const int selected = combo->currentIndex();
                        refresh();
                        require(combo->view()->isVisible() && combo->currentIndex() == selected, "Refresh dismissed a picker: " + method);
                        combo->hidePopup();
                    } else if (b.number) {
                        auto slider = qobject_cast<QSlider*>(b.widget);
                        change(b, original.toInt() == slider->minimum() ? slider->maximum() : slider->minimum());
                        change(b, original, true);
                    } else if (qobject_cast<QCheckBox*>(b.widget)) {
                        change(b, !original.toBool());
                        change(b, original);
                    } else change(b, {});
                }
            }
            navigate("devices");
            if (flag("deviceManager", "supported")) {
                // The preview backend acknowledges list commands but, like the live
                // backend, only device reports update the paired-device list.
                for (const auto method : {"disconnectDevice", "connectDevice", "switchDevice", "unpairDevice"}) {
                    const auto& b = bindingFor("devices", method);
                    auto combo = qobject_cast<QComboBox*>(b.widget);
                    if (!combo->count() && (QString(method) == "connectDevice" || QString(method) == "switchDevice")) {
                        auto group = state.value("deviceManager").toObject();
                        auto devices = group.value("devices").toArray();
                        if (devices.size() > 1) {
                            auto device = devices[1].toObject();
                            device["connected"] = QString(method) == "switchDevice";
                            device["playback"] = false;
                            devices[1] = device; group["devices"] = devices; state["deviceManager"] = group;
                            refresh();
                        }
                    }
                    if (!combo->isEnabled() || !combo->count()) continue;
                    if (QString(method) == "unpairDevice") {
                        const auto before = backend.sent;
                        confirm(QMessageBox::Cancel); choose(combo, 0);
                        require(before == backend.sent, "Canceled unpair sent a command");
                        confirm(QMessageBox::Yes);
                    }
                    edit("deviceManager." + QString(method), [&] { choose(combo, 0); });
                }
            }
            navigate("home");
            const auto& power = bindingFor("home", "powerOff");
            if (power.widget->isEnabled()) {
                int before = backend.sent;
                confirm(QMessageBox::Cancel);
                qobject_cast<QPushButton*>(power.widget)->click();
                require(before == backend.sent, "Canceled power off sent a command");
                confirm(QMessageBox::Yes);
                change(power, {});
                require(!power.widget->isEnabled(), "Power off still offered while pending");
            }
            std::cout << "  " << step.toStdString() << ": " << backend.sent - startCount << " accepted widget requests\n";
        }
        step = "discovery/preferences/quick controls";
        navigate("connection");
        change(bindingFor("connection", "disconnectDevice"), {});
        change(bindingFor("connection", "refreshDevices"), {});
        const auto& discover = bindingFor("connection", "devices");
        edit("app.connectDevice", [&] { choose(qobject_cast<QComboBox*>(discover.widget), 0); });
        until([&] { return state.value("connection").toObject().value("state") == "connected"; }, "Could not reconnect");
        auto connection = state.value("connection").toObject();
        connection["state"] = "initializing"; state["connection"] = connection; refresh();
        require(!discover.widget->isEnabled() && !bindingFor("connection", "refreshDevices").widget->isEnabled()
            && bindingFor("connection", "disconnectDevice").widget->isEnabled(), "Initialization controls allow overlapping connections");
        state = backend.state; refresh();
        navigate("application");
        change(bindingFor("application", "locale"), language == "en" ? "zh_CN" : "en");
        change(bindingFor("application", "locale"), language);
        change(bindingFor("application", "themeMode"), "dark");
        change(bindingFor("application", "themeMode"), "system");
        require(!flag("preferences", "persistent"), "Preview wrote a persistent preference");
        startPreview(executable, "generic-max", language);
        until([&] { return backend.ready; }, "Full-featured preview did not start");
        navigate("sound");
        auto group = state.value("settings").toObject();
        group["safeListeningStateKnown"] = false; state["settings"] = group; refresh();
        const auto& safe = bindingFor("sound", "safeListeningEnabled");
        require(safe.widget->isEnabled() && qobject_cast<QComboBox*>(safe.widget)->currentIndex() == -1, QString("Unknown safe listening: enabled=%1, index=%2, profile=%3, capability=%4, connected=%5").arg(safe.widget->isEnabled())
            .arg(qobject_cast<QComboBox*>(safe.widget)->currentIndex()).arg(state.value("app").toObject().value("currentProfile").toString())
            .arg(flag("settings", "safeListeningSupported")).arg(state.value("connection").toObject().value("state").toString()));
        change(safe, false);
        showQuick(); prepare("quick", "mode", 2);
        const auto& ambient = bindingFor("quick", "ambientLevel");
        change(ambient, value(ambient.field).toInt() == 1 ? 20 : 1);
        const auto& voice = bindingFor("quick", "focusOnVoice");
        change(voice, !value(voice.field).toBool());
        const auto& volume = bindingFor("quick", "volume");
        change(volume, value(volume.field).toInt() == 12 ? 13 : 12);
        for (const auto method : {"togglePlayback", "nextTrack", "previousTrack"}) change(bindingFor("quick", method), {});
        closeQuick->click();

        step = "backend recovery";
        startPreview(executable + ".missing", profiles.first().toString(), language);
        until([&] { return backend.error == "backend-start-failed"; }, "Missing backend error not reported");
        require(!volume.widget->isEnabled() && retry->isEnabled() && retry->isVisible(), "Missing backend did not expose recovery");
        const int before = backend.sent;
        startPreview(executable, profiles.first().toString(), language);
        until([&] { return backend.ready; }, "Backend retry failed");
        require(volume.widget->isEnabled() && before == backend.sent, "Recovery disabled controls or replayed commands");
        require(exercised == expected, "Native command coverage mismatch: " + QStringList((expected - exercised).values()).join(", "));
        std::cout << "KDE native parity: PASS (" << profiles.size() << " profiles, " << expected.size()
                  << " commands, " << backend.sent << " requests, " << language.toStdString() << ")" << std::endl;
        qApp->exit(0);
    } catch (const std::exception& error) {
        std::cerr << "KDE native parity: FAIL (" << step.toStdString() << "): " << error.what() << std::endl;
        qApp->exit(1);
    }
    backend.received = {};
}
