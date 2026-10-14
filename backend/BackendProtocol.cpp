#include "BackendProtocol.hpp"

#include <QJsonArray>
#include <QMetaMethod>
#include <QMetaProperty>
#include <QRegularExpression>
#include <QSet>
#include <cmath>
#include <limits>

namespace {
const QStringList groups{"playback", "noiseControl", "equalizer", "settings", "deviceManager"};

// An explicit allowlist prevents future QObject methods from silently becoming
// remote operations. Tests compare this contract with the complete business API.
const QSet<QByteArray> commands{
    "playback.setVolume", "playback.togglePlayback", "playback.previousTrack",
    "playback.nextTrack", "playback.powerOff",
    "noiseControl.setMode", "noiseControl.setAmbientLevel", "noiseControl.setFocusOnVoice",
    "noiseControl.setAdaptiveEnabled", "noiseControl.setAdaptiveSensitivity",
    "equalizer.setBand", "equalizer.setClearBass", "equalizer.selectPreset",
    "settings.setDseeEnabled", "settings.setSpeakToChatEnabled", "settings.setAutoPauseEnabled",
    "settings.setVoiceGuidanceEnabled", "settings.setVoiceGuidanceVolume",
    "settings.setTouchLeft", "settings.setTouchRight", "settings.setNcButtonFunction",
    "settings.setSoundQualityPrior", "settings.setAutoPowerOffEnabled", "settings.setHeadGestureEnabled",
    "settings.setListeningModeEnabled", "settings.setAdaptiveVolumeEnabled",
    "settings.setSafeListeningEnabled", "settings.setCaptureVoiceEnabled",
    "settings.setSpeakToChatSensitivity", "settings.setSpeakToChatModeOut",
    "settings.setAutoPowerOffValue", "settings.setCinemaModeEnabled", "settings.setListeningRoomSize",
    "deviceManager.setPairingMode", "deviceManager.switchDevice", "deviceManager.connectDevice",
    "deviceManager.disconnectDevice", "deviceManager.unpairDevice"
};

QJsonObject properties(QObject* object)
{
    QJsonObject result;
    const auto* meta = object->metaObject();
    // Include inherited business properties, but never QObject's objectName.
    for (int i = QObject::staticMetaObject.propertyCount(); i < meta->propertyCount(); ++i) {
        const auto property = meta->property(i);
        const auto value = property.read(object);
        if (!value.canConvert<QObject*>())
            result.insert(QString::fromLatin1(property.name()), QJsonValue::fromVariant(value));
    }
    return result;
}
}

QObject* BackendProtocol::target(const QString& group) const
{
    if (group == "playback") return m_service.playback();
    if (group == "noiseControl") return m_service.noiseControl();
    if (group == "equalizer") return m_service.equalizer();
    if (group == "settings") return m_service.settings();
    if (group == "deviceManager") return m_service.deviceManager();
    return nullptr;
}

QJsonObject BackendProtocol::snapshot() const
{
    QJsonObject state{{"type", "state"}, {"version", Version},
        {"app", properties(&m_service)}, {"connection", properties(m_service.connection())}};
    for (const auto& group : groups) state.insert(group, properties(target(group)));
    return state;
}

QJsonObject BackendProtocol::describe() const
{
    QJsonArray list;
    for (const auto& group : groups) {
        const auto* meta = target(group)->metaObject();
        for (int i = meta->methodOffset(); i < meta->methodCount(); ++i) {
            const auto method = meta->method(i);
            if (!commands.contains(group.toLatin1() + '.' + method.name())) continue;
            QJsonArray args;
            for (const auto& type : method.parameterTypes()) args.append(QString::fromLatin1(type));
            list.append(QJsonObject{{"target", group}, {"method", QString::fromLatin1(method.name())}, {"args", args}});
        }
    }
    for (const auto& name : {"refreshDevices", "disconnectDevice", "connectDevice"}) {
        QJsonArray args;
        if (QByteArray(name) == "connectDevice") args = {"QString", "QString"};
        list.append(QJsonObject{{"target", "app"}, {"method", name}, {"args", args}});
    }
    return {{"version", Version}, {"commands", list},
            {"profiles", QJsonArray::fromStringList(m_service.availableProfiles())},
            {"successMeaning", "accepted"}};
}

QJsonObject BackendProtocol::execute(const QJsonObject& request)
{
    const auto result = [&](const QString& code, const QString& detail = {}) {
        QJsonObject value{{"type", "result"}, {"version", Version}, {"ok", code.isEmpty()},
            {"status", code.isEmpty() ? "accepted" : "rejected"},
            {"errorCode", code}, {"error", detail.isEmpty() ? code : detail}};
        if (request.contains("id")) value.insert("id", request.value("id"));
        return value;
    };
    if (request.contains("version") && request.value("version") != QJsonValue(Version))
        return result("unsupported-version");
    if (request.contains("id") && (!request.value("id").isString() || request.value("id").toString().size() > 128))
        return result("invalid-id", "Request id must be a string of at most 128 characters");
    if (!request.value("target").isString() || !request.value("method").isString()
        || !request.value("args").isArray()) return result("invalid-request");
    const auto group = request.value("target").toString();
    const auto name = request.value("method").toString().toLatin1();
    const auto args = request.value("args").toArray();
    if (group == "app") {
        if (name == "describe" && args.isEmpty()) {
            auto reply = result({}); reply.insert("contract", describe()); return reply;
        }
        if (name == "refreshDevices" && args.isEmpty()) { m_service.refreshDevices(); return result({}); }
        if (name == "disconnectDevice" && args.isEmpty()) { m_service.disconnectDevice(); return result({}); }
        if (name == "connectDevice" && args.size() == 2 && args[0].isString() && args[1].isString()) {
            static const QRegularExpression mac("^(?:[0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}$");
            if (!mac.match(args[1].toString()).hasMatch()) return result("invalid-arguments");
            if (m_service.connection()->state() == "connecting" || m_service.connection()->state() == "connected")
                return result("connection-busy");
            m_service.connectDevice(args[0].toString(), args[1].toString()); return result({});
        }
        return result("invalid-command");
    }
    if (!commands.contains(group.toLatin1() + '.' + name)) return result("invalid-command");
    if (m_service.connection()->state() != "connected") return result("not-connected");
    auto* object = target(group);
    const auto* meta = object->metaObject();
    for (int i = meta->methodOffset(); i < meta->methodCount(); ++i) {
        const auto method = meta->method(i);
        if (method.name() != name || method.methodType() != QMetaMethod::Method
            || method.access() != QMetaMethod::Public || method.returnMetaType() != QMetaType::fromType<bool>()) continue;
        if (args.size() != method.parameterCount() || args.size() > 2) return result("invalid-arguments");
        QVariant values[2];
        QGenericArgument arguments[2];
        for (int n = 0; n < args.size(); ++n) {
            const auto type = method.parameterMetaType(n);
            const auto& value = args[n];
            bool valid = false;
            if (type == QMetaType::fromType<bool>()) valid = value.isBool();
            else if (type == QMetaType::fromType<QString>()) valid = value.isString();
            else if (type == QMetaType::fromType<int>()) {
                const double number = value.toDouble();
                valid = value.isDouble() && std::isfinite(number) && std::floor(number) == number
                    && number >= std::numeric_limits<int>::min() && number <= std::numeric_limits<int>::max();
            }
            if (!valid) return result("invalid-arguments");
            values[n] = value.toVariant();
            if (!values[n].convert(type)) return result("invalid-arguments");
            arguments[n] = QGenericArgument(type.name(), values[n].constData());
        }
        bool accepted = false;
        const bool invoked = method.invoke(object, Qt::DirectConnection,
            QGenericReturnArgument("bool", &accepted), arguments[0], arguments[1]);
        m_service.notifyRejectedWriteCountChanged();
        return result(invoked && accepted ? QString() : QStringLiteral("unavailable-or-invalid-value"));
    }
    return result("invalid-command");
}
