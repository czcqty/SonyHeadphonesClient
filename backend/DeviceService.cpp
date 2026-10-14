#include "DeviceService.hpp"
#include "MdrSession.hpp"

DeviceService::DeviceService(bool previewMode, QObject* parent)
    : QObject(parent), m_playback(&m_state, this), m_noiseControl(&m_state, this),
      m_equalizer(&m_state, this), m_settings(&m_state, this),
      m_deviceManager(&m_state, this), m_connection(this), m_previewMode(previewMode)
{
    applyProfile(QStringLiteral("WH-1000XM6"));
    m_connection.setSelectedDevice(modelName(), deviceImage());
    if (m_previewMode) {
        m_connection.setState(QStringLiteral("connected"));
    } else {
        m_connection.setState(QStringLiteral("disconnected"));
        m_connection.setDevices({});
        m_session = std::make_unique<MdrSession>(&m_state, this);
        connect(m_session.get(), &MdrSession::stateChanged,
                &m_connection, &ConnectionViewModel::setState);
        connect(m_session.get(), &MdrSession::errorOccurred,
                &m_connection, &ConnectionViewModel::setError);
        connect(m_session.get(), &MdrSession::devicesChanged,
                &m_connection, &ConnectionViewModel::setDevices);
        connect(m_session.get(), &MdrSession::deviceUpdated,
                this, &DeviceService::refreshViewModels);
        connect(m_session.get(), &MdrSession::initializationCompleted,
                this, &DeviceService::selectLiveProfile);
        connect(m_session.get(), &MdrSession::connectionAbandoned,
                this, &DeviceService::handleConnectionAbandoned);
    }
}

DeviceService::~DeviceService() = default;

QString DeviceService::currentProfile() const { return m_profile ? m_profile->id : QString(); }
QString DeviceService::modelName() const { return m_profile ? m_profile->modelName : QString(); }
QString DeviceService::deviceFamily() const { return m_profile ? m_profile->family : QString(); }
QString DeviceService::deviceImage() const { return m_profile ? m_profile->assetPath : QString(); }
QString DeviceService::protocol() const { return m_profile ? m_profile->protocol : QString(); }
bool DeviceService::profileReferenceBlocked() const { return m_profile && m_profile->referenceBlocked; }

QStringList DeviceService::availableProfiles() const
{
    return m_profiles.profileIds();
}

bool DeviceService::applyProfile(const QString& profileId)
{
    const DeviceProfile* profile = m_profiles.find(profileId);
    if (!profile)
        return false;

    m_profile = profile;
    m_profiles.apply(*profile, m_state.device, m_state.capabilities);
    if (m_previewMode)
        m_connection.setSelectedDevice(profile->modelName, profile->assetPath);
    m_state.rejectedWrites = 0;
    refreshViewModels();
    emit profileChanged();
    emit rejectedWriteCountChanged();
    return true;
}

void DeviceService::refreshDevices()
{
    m_connection.clearError();
    if (m_previewMode) {
        m_connection.refresh();
        m_connection.setState(QStringLiteral("disconnected"));
        return;
    }
    if (m_session)
        m_session->refreshDevices();
}

void DeviceService::connectDevice(const QString& name, const QString& mac)
{
    const DeviceProfile* pendingProfile = profileForDeviceName(name);
    m_connection.setSelectedDevice(name, pendingProfile ? pendingProfile->assetPath : deviceImage());
    m_connection.clearError();
    if (m_previewMode) {
        m_connection.setState(QStringLiteral("connecting"));
        const auto generation = ++m_previewGeneration;
        QMetaObject::invokeMethod(this, [this, name, generation]() {
            if (generation != m_previewGeneration) return;
            selectLiveProfile(name);
            m_connection.setState(QStringLiteral("connected"));
        }, Qt::QueuedConnection);
        return;
    }
    if (m_session)
        m_session->connectDevice(name, mac);
}

void DeviceService::disconnectDevice()
{
    if (m_session)
        m_session->disconnectDevice();
    ++m_previewGeneration;
    if (m_previewMode)
        m_connection.setState(QStringLiteral("disconnected"));
    emit disconnected();
}

void DeviceService::refreshViewModels()
{
    m_playback.refresh();
    m_noiseControl.refresh();
    m_equalizer.refresh();
    m_settings.refresh();
    m_deviceManager.refresh();
}

void DeviceService::selectLiveProfile(const QString& modelName)
{
    if (const DeviceProfile* profile = profileForDeviceName(modelName))
        m_profile = profile;
    emit profileChanged();
    refreshViewModels();
}

const DeviceProfile* DeviceService::profileForDeviceName(const QString& modelName) const
{
    if (const DeviceProfile* exact = m_profiles.find(modelName))
        return exact;

    for (const QString& id : m_profiles.profileIds()) {
        if (!id.startsWith(QStringLiteral("generic"))
            && modelName.contains(id, Qt::CaseInsensitive))
            return m_profiles.find(id);
    }

    if (modelName.contains(QStringLiteral("WF-"), Qt::CaseInsensitive)
        || modelName.contains(QStringLiteral("LinkBuds"), Qt::CaseInsensitive))
        return m_profiles.find(QStringLiteral("generic-tws"));
    return m_profiles.find(QStringLiteral("generic-headband"));
}


void DeviceService::notifyRejectedWriteCountChanged() { emit rejectedWriteCountChanged(); }

void DeviceService::handleConnectionAbandoned(const QString& code, const QString& detail)
{
    m_connection.setError(code, detail);
    emit connectionAbandoned(code, detail);
}
