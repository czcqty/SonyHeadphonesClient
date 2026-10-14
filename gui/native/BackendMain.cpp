#include "../quick/AppController.hpp"

#include <QCoreApplication>
#include <QCommandLineParser>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaMethod>
#include <QMetaProperty>
#include <QTimer>
#include <iostream>
#include <thread>

namespace {
QJsonObject properties(QObject* object)
{
    QJsonObject result;
    const auto* meta = object->metaObject();
    for (int i = meta->propertyOffset(); i < meta->propertyCount(); ++i) {
        const auto property = meta->property(i);
        const auto value = property.read(object);
        if (value.canConvert<QObject*>()) continue;
        result.insert(QString::fromLatin1(property.name()), QJsonValue::fromVariant(value));
    }
    return result;
}

void publish(const QJsonObject& object)
{
    std::cout << QJsonDocument(object).toJson(QJsonDocument::Compact).constData() << std::endl;
}

// Only the public device-operation surface is exposed, with exact argument
// types. QObject methods, arbitrary properties and route mutation are excluded.
bool invoke(AppController& app, const QJsonObject& request)
{
    const auto group = request.value("target").toString();
    const auto name = request.value("method").toString().toLatin1();
    const auto args = request.value("args").toArray();
    if (group == "app") {
        if (name == "refreshDevices" && args.empty()) { app.refreshDevices(); return true; }
        if (name == "disconnectDevice" && args.empty()) { app.disconnectDevice(); return true; }
        if (name == "connectDevice" && args.size() == 2 && args[0].isString() && args[1].isString()) {
            app.connectDevice(args[0].toString(), args[1].toString()); return true;
        }
        return false;
    }
    if (app.connection()->state() != "connected") return false;
    QObject* target = nullptr;
    if (group == "playback") target = app.playback();
    else if (group == "noiseControl") target = app.noiseControl();
    else if (group == "equalizer") target = app.equalizer();
    else if (group == "settings") target = app.settings();
    else if (group == "deviceManager") target = app.deviceManager();
    if (!target || args.size() > 2) return false;
    const auto* meta = target->metaObject();
    for (int i = meta->methodOffset(); i < meta->methodCount(); ++i) {
        const auto method = meta->method(i);
        if (method.methodType() != QMetaMethod::Method || method.access() != QMetaMethod::Public
            || method.name() != name || method.returnMetaType() != QMetaType::fromType<bool>()
            || method.parameterCount() != args.size()) continue;
        QVariant values[2];
        QGenericArgument arguments[2];
        for (int n = 0; n < args.size(); ++n) {
            const auto type = method.parameterMetaType(n);
            if ((type == QMetaType::fromType<bool>() && !args[n].isBool())
                || (type == QMetaType::fromType<QString>() && !args[n].isString())
                || (type == QMetaType::fromType<int>() && (!args[n].isDouble()
                    || args[n].toDouble() != args[n].toInt()))) return false;
            values[n] = args[n].toVariant();
            if (!values[n].convert(type)) return false;
            arguments[n] = QGenericArgument(type.name(), values[n].constData());
        }
        bool accepted = false;
        return method.invoke(target, Qt::DirectConnection, QGenericReturnArgument("bool", &accepted),
                             arguments[0], arguments[1]) && accepted;
    }
    return false;
}
}

int main(int argc, char** argv)
{
    QCoreApplication application(argc, argv);
    application.setApplicationName("SonyHeadphonesClient");
    application.setOrganizationName("czcqty");
    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addOption({"live", "Connect to real Bluetooth devices (default: isolated preview)"});
    parser.addOption({"profile", "Preview device profile", "profile", "WH-1000XM6"});
    parser.process(application);
    AppController controller(!parser.isSet("live"));
    if (controller.previewMode() && !controller.applyProfile(parser.value("profile"))) return 2;

    QTimer pending;
    pending.setSingleShot(true);
    const auto snapshot = [&] {
        QJsonObject state{{"type", "state"}, {"app", properties(&controller)},
            {"playback", properties(controller.playback())}, {"noiseControl", properties(controller.noiseControl())},
            {"equalizer", properties(controller.equalizer())}, {"settings", properties(controller.settings())},
            {"deviceManager", properties(controller.deviceManager())}, {"connection", properties(controller.connection())}};
        publish(state);
    };
    QObject::connect(&pending, &QTimer::timeout, &application, snapshot);
    const auto schedule = [&] { pending.start(0); };
    QObject::connect(controller.playback(), &PlaybackViewModel::changed, &application, schedule);
    QObject::connect(controller.noiseControl(), &NoiseControlViewModel::changed, &application, schedule);
    QObject::connect(controller.equalizer(), &EqualizerViewModel::changed, &application, schedule);
    QObject::connect(controller.settings(), &SettingsViewModel::changed, &application, schedule);
    QObject::connect(controller.deviceManager(), &DeviceManagerViewModel::changed, &application, schedule);
    QObject::connect(controller.connection(), &ConnectionViewModel::changed, &application, schedule);
    QObject::connect(&controller, &AppController::profileChanged, &application, schedule);

    std::thread input([&] {
        std::string line;
        while (std::getline(std::cin, line)) {
            if (line.size() > 64 * 1024) continue;
            auto document = QJsonDocument::fromJson(QByteArray::fromStdString(line));
            QMetaObject::invokeMethod(&application, [&, document] {
                const bool accepted = document.isObject() && invoke(controller, document.object());
                publish({{"type", "result"}, {"ok", accepted},
                    {"error", accepted ? "" : "Unsupported, unavailable or invalid device command"}});
                schedule();
            }, Qt::QueuedConnection);
        }
        QMetaObject::invokeMethod(&application, &QCoreApplication::quit, Qt::QueuedConnection);
    });
    snapshot();
    const int result = application.exec();
    input.join();
    return result;
}
