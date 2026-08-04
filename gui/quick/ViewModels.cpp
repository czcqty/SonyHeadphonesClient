#include "ViewModels.hpp"

#include <QVariantMap>
#include <algorithm>

using namespace mdr::v2::t1;

namespace {

bool isAssignablePreset(int value)
{
    switch (static_cast<Preset>(value)) {
    case Preset::AMBIENT_SOUND_CONTROL:
    case Preset::VOLUME_CONTROL:
    case Preset::PLAYBACK_CONTROL:
    case Preset::TRACK_CONTROL:
    case Preset::NO_FUNCTION:
        return true;
    default:
        return false;
    }
}

} // namespace

PlaybackViewModel::PlaybackViewModel(DeviceState* state, QObject* parent)
    : QObject(parent), m_state(state)
{
}

QString PlaybackViewModel::title() const { return QString::fromStdString(m_state->device.mPlayTrackTitle); }
QString PlaybackViewModel::artist() const { return QString::fromStdString(m_state->device.mPlayTrackArtist); }
int PlaybackViewModel::volume() const { return m_state->device.mPlayVolume.desired; }
int PlaybackViewModel::batteryLeft() const { return m_state->device.mBatteryL.level; }
int PlaybackViewModel::batteryRight() const { return m_state->device.mBatteryR.level; }
int PlaybackViewModel::batteryCase() const { return m_state->device.mBatteryCase.level; }
bool PlaybackViewModel::hasRightBattery() const { return m_state->capabilities.hasBatteryR; }
bool PlaybackViewModel::hasCaseBattery() const { return m_state->capabilities.hasBatteryCase; }
bool PlaybackViewModel::powerOffSupported() const { return m_state->capabilities.hasPowerOff; }

void PlaybackViewModel::setVolume(int value)
{
    m_state->device.mPlayVolume.desired = std::clamp(value, 0, 30);
    emit changed();
}

void PlaybackViewModel::togglePlayback()
{
    m_state->device.mPlayControl.desired = m_state->device.mPlayPause == PlaybackStatus::PLAY
        ? PlaybackControl::PAUSE : PlaybackControl::PLAY;
    emit changed();
}

void PlaybackViewModel::previousTrack()
{
    m_state->device.mPlayControl.desired = PlaybackControl::TRACK_DOWN;
    emit changed();
}

void PlaybackViewModel::nextTrack()
{
    m_state->device.mPlayControl.desired = PlaybackControl::TRACK_UP;
    emit changed();
}

bool PlaybackViewModel::powerOff()
{
    if (!powerOffSupported()) {
        ++m_state->rejectedWrites;
        return false;
    }
    m_state->device.mShutdown.desired = true;
    emit changed();
    return true;
}

void PlaybackViewModel::refresh() { emit changed(); }

NoiseControlViewModel::NoiseControlViewModel(DeviceState* state, QObject* parent)
    : QObject(parent), m_state(state)
{
}

bool NoiseControlViewModel::supported() const { return m_state->capabilities.hasNoiseCancelling; }
bool NoiseControlViewModel::ambientSupported() const { return m_state->capabilities.hasAmbientSound; }
int NoiseControlViewModel::mode() const
{
    if (!m_state->device.mNcAsmEnabled.desired)
        return 0;
    return m_state->device.mNcAsmMode.desired == NcAsmMode::ASM ? 2 : 1;
}
int NoiseControlViewModel::ambientLevel() const { return m_state->device.mNcAsmAmbientLevel.desired; }
bool NoiseControlViewModel::focusOnVoice() const { return m_state->device.mNcAsmFocusOnVoice.desired; }

bool NoiseControlViewModel::setMode(int value)
{
    if (!supported() || (value == 2 && !ambientSupported())) {
        ++m_state->rejectedWrites;
        return false;
    }
    m_state->device.mNcAsmEnabled.desired = value != 0;
    m_state->device.mNcAsmMode.desired = value == 2 ? NcAsmMode::ASM : NcAsmMode::NC;
    emit changed();
    return true;
}

bool NoiseControlViewModel::setAmbientLevel(int level)
{
    if (!ambientSupported()) {
        ++m_state->rejectedWrites;
        return false;
    }
    m_state->device.mNcAsmAmbientLevel.desired = std::clamp(level, 1, 20);
    emit changed();
    return true;
}

bool NoiseControlViewModel::setFocusOnVoice(bool enabled)
{
    if (!ambientSupported()) {
        ++m_state->rejectedWrites;
        return false;
    }
    m_state->device.mNcAsmFocusOnVoice.desired = enabled;
    emit changed();
    return true;
}

void NoiseControlViewModel::refresh() { emit changed(); }

EqualizerViewModel::EqualizerViewModel(DeviceState* state, QObject* parent)
    : QObject(parent), m_state(state)
{
}

bool EqualizerViewModel::supported() const { return m_state->capabilities.hasEqualizer; }
QString EqualizerViewModel::presetId() const { return m_presetId; }
QVariantList EqualizerViewModel::bands() const
{
    QVariantList result;
    for (int value : m_state->device.mEqConfig.desired)
        result.push_back(value);
    return result;
}
int EqualizerViewModel::clearBass() const { return m_state->device.mEqClearBass.desired; }

bool EqualizerViewModel::setBand(int index, int value)
{
    if (!supported() || index < 0 || index >= static_cast<int>(m_state->device.mEqConfig.desired.size())) {
        ++m_state->rejectedWrites;
        return false;
    }
    m_state->device.mEqConfig.desired[static_cast<size_t>(index)] = std::clamp(value, -10, 10);
    m_presetId = QStringLiteral("custom");
    emit changed();
    return true;
}

bool EqualizerViewModel::setClearBass(int value)
{
    if (!supported()) {
        ++m_state->rejectedWrites;
        return false;
    }
    m_state->device.mEqClearBass.desired = std::clamp(value, -10, 10);
    m_presetId = QStringLiteral("custom");
    emit changed();
    return true;
}

bool EqualizerViewModel::selectPreset(const QString& presetId)
{
    if (!supported()) {
        ++m_state->rejectedWrites;
        return false;
    }
    const QString normalized = presetId.compare(QStringLiteral("flat"), Qt::CaseInsensitive) == 0
        ? QStringLiteral("flat") : QStringLiteral("custom");
    m_presetId = normalized;
    if (normalized == QStringLiteral("flat"))
        m_state->device.mEqConfig.desired.assign(m_state->device.mEqConfig.desired.size(), 0);
    emit changed();
    return true;
}

void EqualizerViewModel::refresh() { emit changed(); }

SettingsViewModel::SettingsViewModel(DeviceState* state, QObject* parent)
    : QObject(parent), m_state(state)
{
}

bool SettingsViewModel::dseeSupported() const { return m_state->capabilities.hasDSEE; }
bool SettingsViewModel::dseeEnabled() const { return m_state->device.mUpscalingEnabled.desired; }
bool SettingsViewModel::speakToChatSupported() const { return m_state->capabilities.hasSpeakToChat; }
bool SettingsViewModel::speakToChatEnabled() const { return m_state->device.mSpeakToChatEnabled.desired; }
bool SettingsViewModel::autoPauseSupported() const { return m_state->capabilities.hasAutoPause; }
bool SettingsViewModel::autoPauseEnabled() const { return m_state->device.mAutoPauseEnabled.desired; }
bool SettingsViewModel::touchSupported() const { return m_state->capabilities.hasTouchFunction; }
int SettingsViewModel::touchLeft() const { return static_cast<int>(m_state->device.mTouchFunctionLeft.desired); }
int SettingsViewModel::touchRight() const { return static_cast<int>(m_state->device.mTouchFunctionRight.desired); }
bool SettingsViewModel::ncButtonSupported() const { return m_state->capabilities.hasNcButtonFunction; }
int SettingsViewModel::ncButtonFunction() const
{
    return static_cast<int>(m_state->device.mNcAsmButtonFunction.desired);
}
bool SettingsViewModel::voiceGuidanceSupported() const { return m_state->capabilities.hasVoiceGuidance; }
bool SettingsViewModel::voiceGuidanceEnabled() const { return m_state->device.mVoiceGuidanceEnabled.desired; }
bool SettingsViewModel::voiceGuidanceVolumeSupported() const
{
    return m_state->capabilities.hasVoiceGuidanceVolume;
}
int SettingsViewModel::voiceGuidanceVolume() const { return m_state->device.mVoiceGuidanceVolume.desired; }
bool SettingsViewModel::connectionQualitySupported() const { return m_state->capabilities.hasConnectionQuality; }
bool SettingsViewModel::soundQualityPrior() const
{
    return m_state->device.mAudioPriorityMode.desired == PriorMode::SOUND_QUALITY_PRIOR;
}
bool SettingsViewModel::autoPowerOffSupported() const { return m_state->capabilities.hasAutoPowerOff; }
bool SettingsViewModel::autoPowerOffEnabled() const
{
    return m_state->device.mPowerAutoOff.desired != AutoPowerOffElements::POWER_OFF_DISABLE;
}
bool SettingsViewModel::headGestureSupported() const { return m_state->capabilities.hasHeadGesture; }
bool SettingsViewModel::headGestureEnabled() const { return m_state->device.mHeadGestureEnabled.desired; }
bool SettingsViewModel::listeningModeSupported() const { return m_state->capabilities.hasListeningMode; }
bool SettingsViewModel::listeningModeEnabled() const { return m_state->device.mBGMModeEnabled.desired; }

bool SettingsViewModel::setDseeEnabled(bool enabled)
{
    if (!dseeSupported()) {
        ++m_state->rejectedWrites;
        return false;
    }
    m_state->device.mUpscalingEnabled.desired = enabled;
    emit changed();
    return true;
}

bool SettingsViewModel::setSpeakToChatEnabled(bool enabled)
{
    if (!speakToChatSupported()) {
        ++m_state->rejectedWrites;
        return false;
    }
    m_state->device.mSpeakToChatEnabled.desired = enabled;
    emit changed();
    return true;
}

bool SettingsViewModel::setAutoPauseEnabled(bool enabled)
{
    if (!autoPauseSupported()) {
        ++m_state->rejectedWrites;
        return false;
    }
    m_state->device.mAutoPauseEnabled.desired = enabled;
    emit changed();
    return true;
}

bool SettingsViewModel::setVoiceGuidanceEnabled(bool enabled)
{
    if (!voiceGuidanceSupported()) {
        ++m_state->rejectedWrites;
        return false;
    }
    m_state->device.mVoiceGuidanceEnabled.desired = enabled;
    emit changed();
    return true;
}

bool SettingsViewModel::setVoiceGuidanceVolume(int volume)
{
    if (!voiceGuidanceVolumeSupported()) {
        ++m_state->rejectedWrites;
        return false;
    }
    m_state->device.mVoiceGuidanceVolume.desired = std::clamp(volume, 1, 5);
    emit changed();
    return true;
}

bool SettingsViewModel::setTouchLeft(int preset)
{
    if (!touchSupported() || !isAssignablePreset(preset)) {
        ++m_state->rejectedWrites;
        return false;
    }
    m_state->device.mTouchFunctionLeft.desired = static_cast<Preset>(preset);
    emit changed();
    return true;
}

bool SettingsViewModel::setTouchRight(int preset)
{
    if (!touchSupported() || !isAssignablePreset(preset)) {
        ++m_state->rejectedWrites;
        return false;
    }
    m_state->device.mTouchFunctionRight.desired = static_cast<Preset>(preset);
    emit changed();
    return true;
}

bool SettingsViewModel::setNcButtonFunction(int function)
{
    const auto value = static_cast<Function>(function);
    const bool allowed = value == Function::NC_ASM_OFF
        || value == Function::NC_ASM
        || value == Function::NC_OFF
        || value == Function::ASM_OFF;
    if (!ncButtonSupported() || !allowed) {
        ++m_state->rejectedWrites;
        return false;
    }
    m_state->device.mNcAsmButtonFunction.desired = value;
    emit changed();
    return true;
}

bool SettingsViewModel::setSoundQualityPrior(bool enabled)
{
    if (!connectionQualitySupported()) {
        ++m_state->rejectedWrites;
        return false;
    }
    m_state->device.mAudioPriorityMode.desired = enabled
        ? PriorMode::SOUND_QUALITY_PRIOR : PriorMode::CONNECTION_QUALITY_PRIOR;
    emit changed();
    return true;
}

bool SettingsViewModel::setAutoPowerOffEnabled(bool enabled)
{
    if (!autoPowerOffSupported()) {
        ++m_state->rejectedWrites;
        return false;
    }
    m_state->device.mPowerAutoOff.desired = enabled
        ? AutoPowerOffElements::POWER_OFF_IN_15_MIN : AutoPowerOffElements::POWER_OFF_DISABLE;
    emit changed();
    return true;
}

bool SettingsViewModel::setHeadGestureEnabled(bool enabled)
{
    if (!headGestureSupported()) {
        ++m_state->rejectedWrites;
        return false;
    }
    m_state->device.mHeadGestureEnabled.desired = enabled;
    emit changed();
    return true;
}

bool SettingsViewModel::setListeningModeEnabled(bool enabled)
{
    if (!listeningModeSupported()) {
        ++m_state->rejectedWrites;
        return false;
    }
    m_state->device.mBGMModeEnabled.desired = enabled;
    if (enabled)
        m_state->device.mUpmixCinemaEnabled.desired = false;
    emit changed();
    return true;
}

void SettingsViewModel::refresh() { emit changed(); }

DeviceManagerViewModel::DeviceManagerViewModel(DeviceState* state, QObject* parent)
    : QObject(parent), m_state(state)
{
}

bool DeviceManagerViewModel::supported() const { return m_state->capabilities.hasDeviceManagement; }
bool DeviceManagerViewModel::pairingMode() const { return m_state->device.mPairingMode.desired; }
QVariantList DeviceManagerViewModel::devices() const
{
    return {
        QVariantMap{{QStringLiteral("nameId"), QStringLiteral("laptop")},
                    {QStringLiteral("mac"), QStringLiteral("00:AA:BB:CC:DD:01")},
                    {QStringLiteral("connected"), true}},
        QVariantMap{{QStringLiteral("nameId"), QStringLiteral("phone")},
                    {QStringLiteral("mac"), QStringLiteral("00:AA:BB:CC:DD:02")},
                    {QStringLiteral("connected"), true}},
    };
}

bool DeviceManagerViewModel::setPairingMode(bool enabled)
{
    if (!supported()) {
        ++m_state->rejectedWrites;
        return false;
    }
    m_state->device.mPairingMode.desired = enabled;
    emit changed();
    return true;
}

bool DeviceManagerViewModel::switchDevice(const QString& mac)
{
    if (!supported()) {
        ++m_state->rejectedWrites;
        return false;
    }
    m_state->device.mMultipointDeviceMac.desired = mac.toStdString();
    emit changed();
    return true;
}

void DeviceManagerViewModel::refresh() { emit changed(); }

ConnectionViewModel::ConnectionViewModel(QObject* parent) : QObject(parent)
{
    m_devices = {
        QVariantMap{{QStringLiteral("name"), QStringLiteral("WH-1000XM6")},
                    {QStringLiteral("mac"), QStringLiteral("00:11:22:33:44:55")}},
        QVariantMap{{QStringLiteral("name"), QStringLiteral("WF-1000XM5")},
                    {QStringLiteral("mac"), QStringLiteral("AA:BB:CC:DD:EE:FF")}},
    };
}

QVariantList ConnectionViewModel::devices() const
{
    return m_devices;
}

void ConnectionViewModel::setState(const QString& state)
{
    if (m_state == state)
        return;
    m_state = state;
    emit changed();
}

void ConnectionViewModel::setError(const QString& code, const QString& detail)
{
    if (m_errorCode == code && m_errorDetail == detail)
        return;
    m_errorCode = code;
    m_errorDetail = detail;
    emit changed();
}

void ConnectionViewModel::clearError()
{
    if (m_errorCode.isEmpty() && m_errorDetail.isEmpty())
        return;
    m_errorCode.clear();
    m_errorDetail.clear();
    emit changed();
}

void ConnectionViewModel::setDevices(const QVariantList& devices)
{
    m_devices = devices;
    emit changed();
}

void ConnectionViewModel::setSelectedDevice(const QString& name, const QString& image)
{
    if (m_selectedDeviceName == name && m_selectedDeviceImage == image)
        return;
    m_selectedDeviceName = name;
    m_selectedDeviceImage = image;
    emit changed();
}

void ConnectionViewModel::refresh()
{
    m_state = QStringLiteral("scanning");
    m_errorCode.clear();
    m_errorDetail.clear();
    emit changed();
}
