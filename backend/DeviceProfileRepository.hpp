#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

#include <mdr/Headphones.hpp>

#include "DeviceCapabilities.hpp"

struct DeviceProfile
{
    QString id;
    QString modelName;
    QString family;
    QString protocol;
    QString assetPath;
    QString firmware;
    bool referenceBlocked = false;
    DeviceCapabilities capabilities;
};

class DeviceProfileRepository
{
public:
    DeviceProfileRepository();

    const DeviceProfile* find(const QString& id) const;
    const DeviceProfile& fallback() const;
    QStringList profileIds() const;
    const QVector<DeviceProfile>& profiles() const { return m_profiles; }

    void apply(const DeviceProfile& profile,
               mdr::MDRHeadphones& device,
               DeviceCapabilities& capabilities) const;

private:
    QVector<DeviceProfile> m_profiles;
};
