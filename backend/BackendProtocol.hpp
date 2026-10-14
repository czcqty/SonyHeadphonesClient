#pragma once

#include <QJsonObject>
#include "DeviceService.hpp"
#include "ApplicationPreferences.hpp"

// Versioned pipe protocol. Success means accepted into desired state; it is
// deliberately not an acknowledgement that the headphone applied the command.
class BackendProtocol final
{
public:
    static constexpr int Version = 1;
    explicit BackendProtocol(DeviceService& service) : m_service(service), m_preferences(service.previewMode()) {}
    bool overridePreference(const QString& key, const QString& value) { return m_preferences.set(key, value, false); }
    QJsonObject snapshot() const;
    QJsonObject describe() const;
    QJsonObject execute(const QJsonObject& request);

private:
    QObject* target(const QString& group) const;
    DeviceService& m_service;
    ApplicationPreferences m_preferences;
};
