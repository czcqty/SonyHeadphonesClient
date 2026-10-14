#include <QtTest>
#include <QJsonArray>
#include <QMetaMethod>
#include "BackendProtocol.hpp"

namespace {
QObject* group(DeviceService& service, const QString& name)
{
    if (name == "playback") return service.playback();
    if (name == "noiseControl") return service.noiseControl();
    if (name == "equalizer") return service.equalizer();
    if (name == "settings") return service.settings();
    return service.deviceManager();
}
QJsonArray sample(const QString& name, const QJsonArray& types)
{
    QJsonArray args;
    for (const auto& type : types) {
        if (type == "bool") args.append(true);
        else if (type == "QString") args.append(name == "selectPreset" ? "rock" : "00:11:22:33:44:55");
        else args.append(name == "setVolume" ? 12 : 1);
    }
    return args;
}
}

class ProtocolTests final : public QObject
{
    Q_OBJECT
private slots:
    void allBusinessMethodsAreExposed()
    {
        DeviceService service(true);
        BackendProtocol protocol(service);
        QSet<QString> exposed;
        for (const auto item : protocol.describe()["commands"].toArray()) {
            const auto command = item.toObject();
            exposed.insert(command["target"].toString() + "." + command["method"].toString());
        }
        int count = 0;
        for (const auto& name : {"playback", "noiseControl", "equalizer", "settings", "deviceManager"}) {
            const auto* meta = group(service, name)->metaObject();
            for (int i = meta->methodOffset(); i < meta->methodCount(); ++i) {
                const auto method = meta->method(i);
                if (method.methodType() != QMetaMethod::Method || method.returnMetaType() != QMetaType::fromType<bool>()) continue;
                const auto key = QString(name) + "." + QString::fromLatin1(method.name());
                QVERIFY2(exposed.contains(key), qPrintable(key));
                ++count;
            }
        }
        QCOMPARE(exposed.size(), count + 3);
    }

    void pipeCommandsMatchDirectBusinessCallsForEveryProfile()
    {
        DeviceService seed(true);
        const auto commands = BackendProtocol(seed).describe()["commands"].toArray();
        for (const auto& profile : seed.availableProfiles()) {
            for (const auto item : commands) {
                const auto command = item.toObject();
                const auto targetName = command["target"].toString();
                if (targetName == "app") continue;
                const auto name = command["method"].toString();
                const auto args = sample(name, command["args"].toArray());
                DeviceService direct(true), piped(true);
                QVERIFY(direct.applyProfile(profile)); QVERIFY(piped.applyProfile(profile));
                BackendProtocol protocol(piped);
                QObject* target = group(direct, targetName);
                QSignalSpy writes(group(piped, targetName), SIGNAL(changed()));
                QVariant values[2]; QGenericArgument arguments[2];
                for (int i = 0; i < args.size(); ++i) {
                    const auto type = command["args"].toArray()[i].toString().toLatin1();
                    values[i] = args[i].toVariant();
                    QVERIFY(values[i].convert(QMetaType::fromName(type)));
                    arguments[i] = QGenericArgument(values[i].typeName(), values[i].constData());
                }
                bool accepted = false;
                QByteArray signature = name.toLatin1() + '(';
                const auto types = command["args"].toArray();
                for (int i = 0; i < types.size(); ++i) {
                    if (i) signature += ',';
                    signature += types[i].toString().toLatin1();
                }
                signature += ')';
                const int index = target->metaObject()->indexOfMethod(signature.constData());
                QVERIFY(index >= 0);
                QVERIFY(target->metaObject()->method(index).invoke(target, Qt::DirectConnection,
                    QGenericReturnArgument("bool", &accepted), arguments[0], arguments[1]));
                const auto result = protocol.execute({{"id", "one"}, {"version", 1},
                    {"target", targetName}, {"method", name}, {"args", args}});
                QCOMPARE(result["ok"].toBool(), accepted);
                QCOMPARE(result["id"].toString(), QString("one"));
                QCOMPARE(protocol.snapshot(), BackendProtocol(direct).snapshot());
                QCOMPARE(writes.size(), accepted ? 1 : 0);
            }
        }
    }

    void malformedRequestsNeverWrite()
    {
        DeviceService service(true);
        BackendProtocol protocol(service);
        const auto initial = protocol.snapshot();
        const QList<QJsonObject> requests{
            {{"target", "settings"}, {"method", "deleteLater"}, {"args", QJsonArray{}}},
            {{"target", "playback"}, {"method", "setVolume"}, {"args", QJsonArray{true}}},
            {{"target", "playback"}, {"method", "setVolume"}, {"args", QJsonArray{1.5}}},
            {{"target", "playback"}, {"method", "setVolume"}, {"args", QJsonArray{1e20}}},
            {{"target", "settings"}, {"method", "setDseeEnabled"}, {"args", QJsonArray{1}}},
            {{"version", 2}, {"target", "playback"}, {"method", "nextTrack"}, {"args", QJsonArray{}}},
            {{"target", "playback"}, {"method", "nextTrack"}, {"args", QJsonObject{}}},
            {{"target", "app"}, {"method", "applyProfile"}, {"args", QJsonArray{"WH-1000XM6"}}}
        };
        for (const auto& request : requests) {
            QVERIFY(!protocol.execute(request)["ok"].toBool());
            QCOMPARE(protocol.snapshot(), initial);
        }
    }

    void disconnectCancelsQueuedPreviewConnection()
    {
        DeviceService service(true);
        service.disconnectDevice();
        service.connectDevice("WH-1000XM5", "00:11:22:33:44:55");
        service.disconnectDevice();
        QCoreApplication::processEvents();
        QCOMPARE(service.connection()->state(), QString("disconnected"));
        BackendProtocol protocol(service);
        const auto reply = protocol.execute({{"target", "playback"}, {"method", "setVolume"}, {"args", QJsonArray{12}}});
        QCOMPARE(reply["errorCode"].toString(), QString("not-connected"));
    }

    void noPresentationPropertiesInBackend()
    {
        DeviceService service(true);
        const auto app = BackendProtocol(service).snapshot()["app"].toObject();
        for (const auto& key : {"currentRoute", "appearance", "desktopStyle", "darkMode", "locale"})
            QVERIFY(!app.contains(key));
    }
};
QTEST_GUILESS_MAIN(ProtocolTests)
#include "ProtocolTests.moc"
