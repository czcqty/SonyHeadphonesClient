#pragma once

#include <QJsonObject>
#include "DeviceService.hpp"

// Versioned pipe protocol. Success means accepted into desired state; it is
// deliberately not an acknowledgement that the headphone applied the command.
class BackendProtocol final
{
public:
    static constexpr int Version = 1;
    explicit BackendProtocol(DeviceService& service) : m_service(service) {}
    QJsonObject snapshot() const;
    QJsonObject describe() const;
    QJsonObject execute(const QJsonObject& request);

private:
    QObject* target(const QString& group) const;
    DeviceService& m_service;
};
