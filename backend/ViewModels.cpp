#include "ViewModels.hpp"

#include <QVariantMap>
#include <QRegularExpression>
#include <QHash>
#include <algorithm>

using namespace mdr::v2::t1;

namespace {

mdr::MDRProperty<bool>* generalBoolean(mdr::MDRHeadphones& device, int slot)
{
    switch (slot) {
    case 0: return &device.mGsParamBool1;
    case 1: return &device.mGsParamBool2;
    case 2: return &device.mGsParamBool3;
    case 3: return &device.mGsParamBool4;
    default: return nullptr;
    }
}

QString touchPresetName(Preset value)
{
    switch (value) {
    case Preset::AMBIENT_SOUND_CONTROL: return QObject::tr("Ambient sound control");
    case Preset::VOLUME_CONTROL: return QObject::tr("Volume control");
    case Preset::PLAYBACK_CONTROL: return QObject::tr("Playback control");
    case Preset::TRACK_CONTROL: return QObject::tr("Track control");
    case Preset::PLAYBACK_CONTROL_VOICE_ASSISTANT_LIMITATION: return QObject::tr("Playback / voice assistant");
    case Preset::VOICE_RECOGNITION: return QObject::tr("Voice recognition");
    case Preset::GOOGLE_ASSIST: return QStringLiteral("Google Assistant");
    case Preset::AMAZON_ALEXA: return QStringLiteral("Amazon Alexa");
    case Preset::MS: return QStringLiteral("Microsoft Teams");
    case Preset::TEAMS: return QStringLiteral("Microsoft Teams");
    case Preset::AMBIENT_SOUND_CONTROL_MIC: return QObject::tr("Ambient sound / microphone");
    case Preset::CHAT_MIX: return QObject::tr("Chat mix");
    case Preset::CUSTOM1: return QObject::tr("Custom 1");
    case Preset::CUSTOM2: return QObject::tr("Custom 2");
    case Preset::NO_FUNCTION: return QObject::tr("No function");
    default: return QObject::tr("Device function %1").arg(QString::number(static_cast<int>(value), 16).toUpper());
    }
}

bool validMac(const QString& value)
{
    static const QRegularExpression pattern(
        QStringLiteral("^(?:[0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}$"));
    return pattern.match(value).hasMatch();
}

QString eqPresetName(EqPresetId value)
{
    switch (value) {
    case EqPresetId::OFF: return QStringLiteral("flat");
    case EqPresetId::ROCK: return QStringLiteral("rock");
    case EqPresetId::POP: return QStringLiteral("pop");
    case EqPresetId::JAZZ: return QStringLiteral("jazz");
    case EqPresetId::DANCE: return QStringLiteral("dance");
    case EqPresetId::EDM: return QStringLiteral("edm");
    case EqPresetId::R_AND_B_HIP_HOP: return QStringLiteral("r-and-b");
    case EqPresetId::ACOUSTIC: return QStringLiteral("acoustic");
    case EqPresetId::BRIGHT: return QStringLiteral("bright");
    case EqPresetId::EXCITED: return QStringLiteral("excited");
    case EqPresetId::MELLOW: return QStringLiteral("mellow");
    case EqPresetId::RELAXED: return QStringLiteral("relaxed");
    case EqPresetId::VOCAL: return QStringLiteral("vocal");
    case EqPresetId::TREBLE: return QStringLiteral("treble");
    case EqPresetId::BASS: return QStringLiteral("bass");
    case EqPresetId::SPEECH: return QStringLiteral("speech");
    case EqPresetId::GAMING_EQ: return QStringLiteral("gaming");
    case EqPresetId::HEAVY: return QStringLiteral("heavy");
    case EqPresetId::CLEAR: return QStringLiteral("clear");
    case EqPresetId::HARD: return QStringLiteral("hard");
    case EqPresetId::SOFT: return QStringLiteral("soft");
    case EqPresetId::USER_SETTING1: return QStringLiteral("user-1");
    case EqPresetId::USER_SETTING2: return QStringLiteral("user-2");
    case EqPresetId::USER_SETTING3: return QStringLiteral("user-3");
    case EqPresetId::USER_SETTING4: return QStringLiteral("user-4");
    case EqPresetId::USER_SETTING5: return QStringLiteral("user-5");
    default: return QStringLiteral("custom");
    }
}

EqPresetId eqPresetValue(const QString& id, bool* ok)
{
    static const QHash<QString, EqPresetId> values{
        {QStringLiteral("flat"), EqPresetId::OFF}, {QStringLiteral("rock"), EqPresetId::ROCK},
        {QStringLiteral("pop"), EqPresetId::POP}, {QStringLiteral("jazz"), EqPresetId::JAZZ},
        {QStringLiteral("dance"), EqPresetId::DANCE}, {QStringLiteral("edm"), EqPresetId::EDM},
        {QStringLiteral("r-and-b"), EqPresetId::R_AND_B_HIP_HOP}, {QStringLiteral("acoustic"), EqPresetId::ACOUSTIC},
        {QStringLiteral("bright"), EqPresetId::BRIGHT}, {QStringLiteral("excited"), EqPresetId::EXCITED},
        {QStringLiteral("mellow"), EqPresetId::MELLOW}, {QStringLiteral("relaxed"), EqPresetId::RELAXED},
        {QStringLiteral("vocal"), EqPresetId::VOCAL}, {QStringLiteral("treble"), EqPresetId::TREBLE},
        {QStringLiteral("bass"), EqPresetId::BASS}, {QStringLiteral("speech"), EqPresetId::SPEECH},
        {QStringLiteral("gaming"), EqPresetId::GAMING_EQ}, {QStringLiteral("heavy"), EqPresetId::HEAVY},
        {QStringLiteral("clear"), EqPresetId::CLEAR}, {QStringLiteral("hard"), EqPresetId::HARD},
        {QStringLiteral("soft"), EqPresetId::SOFT}, {QStringLiteral("custom"), EqPresetId::CUSTOM},
        {QStringLiteral("user-1"), EqPresetId::USER_SETTING1}, {QStringLiteral("user-2"), EqPresetId::USER_SETTING2},
        {QStringLiteral("user-3"), EqPresetId::USER_SETTING3}, {QStringLiteral("user-4"), EqPresetId::USER_SETTING4},
        {QStringLiteral("user-5"), EqPresetId::USER_SETTING5},
    };
    const auto it = values.constFind(id.toLower());
    *ok = it != values.cend();
    return *ok ? it.value() : EqPresetId::UNSPECIFIED;
}

} // namespace

PlaybackViewModel::PlaybackViewModel(DeviceState* state, QObject* parent)
    : QObject(parent), m_state(state)
{
}

QString PlaybackViewModel::title() const { return QString::fromStdString(m_state->device.mPlayTrackTitle); }
QString PlaybackViewModel::artist() const { return QString::fromStdString(m_state->device.mPlayTrackArtist); }
QString PlaybackViewModel::album() const { return QString::fromStdString(m_state->device.mPlayTrackAlbum); }
QString PlaybackViewModel::firmware() const { return QString::fromStdString(m_state->device.mFWVersion); }
QString PlaybackViewModel::alertMessage() const
{
    if (!m_state->device.mLastInteractionMessage.empty())
        return QString::fromStdString(m_state->device.mLastInteractionMessage);
    return QString::number(static_cast<int>(m_state->device.mLastAlertMessage), 16).toUpper();
}
QString PlaybackViewModel::codec() const
{
    switch (m_state->device.mAudioCodec) {
    case AudioCodec::SBC: return QStringLiteral("SBC");
    case AudioCodec::AAC: return QStringLiteral("AAC");
    case AudioCodec::LDAC: return QStringLiteral("LDAC");
    case AudioCodec::APT_X: return QStringLiteral("aptX");
    case AudioCodec::APT_X_HD: return QStringLiteral("aptX HD");
    case AudioCodec::LC3: return QStringLiteral("LC3");
    default: return {};
    }
}
int PlaybackViewModel::volume() const { return m_state->device.mPlayVolume.desired; }
int PlaybackViewModel::batteryLeft() const { return m_state->device.mBatteryL.level; }
int PlaybackViewModel::batteryRight() const { return m_state->device.mBatteryR.level; }
int PlaybackViewModel::batteryCase() const { return m_state->device.mBatteryCase.level; }
bool PlaybackViewModel::hasRightBattery() const { return m_state->capabilities.hasBatteryR; }
bool PlaybackViewModel::hasCaseBattery() const { return m_state->capabilities.hasBatteryCase; }
bool PlaybackViewModel::powerOffSupported() const { return m_state->capabilities.hasPowerOff; }
bool PlaybackViewModel::powerOffPending() const { return m_state->device.mShutdown.desired; }
bool PlaybackViewModel::playbackSupported() const { return m_state->capabilities.hasPlaybackControl; }
bool PlaybackViewModel::volumeSupported() const { return m_state->capabilities.hasVolumeControl; }
bool PlaybackViewModel::safeListeningSupported() const { return m_state->capabilities.hasSafeListening; }
int PlaybackViewModel::safeListeningPressure() const { return m_state->device.mSafeListeningSoundPressure; }
bool PlaybackViewModel::batteryLeftCharging() const { return m_state->device.mBatteryL.charging == BatteryChargingStatus::CHARGING; }
bool PlaybackViewModel::batteryRightCharging() const { return m_state->device.mBatteryR.charging == BatteryChargingStatus::CHARGING; }
bool PlaybackViewModel::batteryCaseCharging() const { return m_state->device.mBatteryCase.charging == BatteryChargingStatus::CHARGING; }

bool PlaybackViewModel::setVolume(int value)
{
    if (!volumeSupported()) { ++m_state->rejectedWrites; return false; }
    m_state->device.mPlayVolume.desired = std::clamp(value, 0, 30);
    emit changed();
    return true;
}

bool PlaybackViewModel::togglePlayback()
{
    if (!playbackSupported()) { ++m_state->rejectedWrites; return false; }
    m_state->device.mPlayControl.desired = m_state->device.mPlayPause == PlaybackStatus::PLAY
        ? PlaybackControl::PAUSE : PlaybackControl::PLAY;
    emit changed();
    return true;
}

bool PlaybackViewModel::previousTrack()
{
    if (!playbackSupported()) { ++m_state->rejectedWrites; return false; }
    m_state->device.mPlayControl.desired = PlaybackControl::TRACK_DOWN;
    emit changed();
    return true;
}

bool PlaybackViewModel::nextTrack()
{
    if (!playbackSupported()) { ++m_state->rejectedWrites; return false; }
    m_state->device.mPlayControl.desired = PlaybackControl::TRACK_UP;
    emit changed();
    return true;
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
bool NoiseControlViewModel::adaptiveSupported() const { return m_state->capabilities.hasAutoAsm; }
bool NoiseControlViewModel::adaptiveEnabled() const { return m_state->device.mNcAsmAutoAsmEnabled.desired; }
int NoiseControlViewModel::adaptiveSensitivity() const
{
    return static_cast<int>(m_state->device.mNcAsmNoiseAdaptiveSensitivity.desired);
}

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

bool NoiseControlViewModel::setAdaptiveEnabled(bool enabled)
{
    if (!adaptiveSupported()) { ++m_state->rejectedWrites; return false; }
    m_state->device.mNcAsmAutoAsmEnabled.desired = enabled;
    emit changed();
    return true;
}

bool NoiseControlViewModel::setAdaptiveSensitivity(int value)
{
    if (!m_state->capabilities.hasNoiseAdaptiveSensitivity || value < 0 || value > 2) {
        ++m_state->rejectedWrites;
        return false;
    }
    m_state->device.mNcAsmNoiseAdaptiveSensitivity.desired
        = static_cast<NoiseAdaptiveSensitivity>(value);
    emit changed();
    return true;
}

void NoiseControlViewModel::refresh() { emit changed(); }

EqualizerViewModel::EqualizerViewModel(DeviceState* state, QObject* parent)
    : QObject(parent), m_state(state)
{
}

bool EqualizerViewModel::supported() const { return m_state->capabilities.hasEqualizer; }
QString EqualizerViewModel::presetId() const { return eqPresetName(m_state->device.mEqPresetId.desired); }
QVariantList EqualizerViewModel::bands() const
{
    QVariantList result;
    for (int value : m_state->device.mEqConfig.desired)
        result.push_back(value);
    return result;
}
int EqualizerViewModel::clearBass() const { return m_state->device.mEqClearBass.desired; }
QVariantList EqualizerViewModel::presetOptions() const
{
    const QStringList ids{QStringLiteral("flat"), QStringLiteral("rock"), QStringLiteral("pop"),
        QStringLiteral("jazz"), QStringLiteral("dance"), QStringLiteral("edm"),
        QStringLiteral("r-and-b"), QStringLiteral("acoustic"), QStringLiteral("bright"),
        QStringLiteral("excited"), QStringLiteral("mellow"), QStringLiteral("relaxed"),
        QStringLiteral("vocal"), QStringLiteral("treble"), QStringLiteral("bass"),
        QStringLiteral("speech"), QStringLiteral("gaming"), QStringLiteral("heavy"),
        QStringLiteral("clear"), QStringLiteral("hard"), QStringLiteral("soft"),
        QStringLiteral("custom"), QStringLiteral("user-1"), QStringLiteral("user-2"),
        QStringLiteral("user-3"), QStringLiteral("user-4"), QStringLiteral("user-5")};
    QVariantList result;
    for (const auto& id : ids)
        result.push_back(QVariantMap{{QStringLiteral("id"), id}, {QStringLiteral("name"), id}});
    return result;
}

bool EqualizerViewModel::setBand(int index, int value)
{
    if (!supported() || index < 0 || index >= static_cast<int>(m_state->device.mEqConfig.desired.size())) {
        ++m_state->rejectedWrites;
        return false;
    }
    m_state->device.mEqConfig.desired[static_cast<size_t>(index)] = std::clamp(value, -10, 10);
    m_state->device.mEqPresetId.desired = EqPresetId::CUSTOM;
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
    m_state->device.mEqPresetId.desired = EqPresetId::CUSTOM;
    emit changed();
    return true;
}

bool EqualizerViewModel::selectPreset(const QString& presetId)
{
    if (!supported()) {
        ++m_state->rejectedWrites;
        return false;
    }
    bool valid = false;
    const auto value = eqPresetValue(presetId, &valid);
    if (!valid) {
        ++m_state->rejectedWrites;
        return false;
    }
    m_state->device.mEqPresetId.desired = value;
    if (value == EqPresetId::OFF)
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
    return autoPowerOffValue() != static_cast<int>(AutoPowerOffElements::POWER_OFF_DISABLE);
}
bool SettingsViewModel::headGestureSupported() const { return m_state->capabilities.hasHeadGesture; }
bool SettingsViewModel::headGestureEnabled() const { return m_state->device.mHeadGestureEnabled.desired; }
bool SettingsViewModel::listeningModeSupported() const { return m_state->capabilities.hasListeningMode; }
bool SettingsViewModel::listeningModeEnabled() const { return m_state->device.mBGMModeEnabled.desired; }
bool SettingsViewModel::adaptiveVolumeSupported() const { return m_state->capabilities.hasAdaptiveVolume; }
bool SettingsViewModel::adaptiveVolumeEnabled() const { return m_state->device.mAutoVolumeEnabled.desired; }
bool SettingsViewModel::safeListeningSupported() const { return m_state->capabilities.hasSafeListening; }
bool SettingsViewModel::safeListeningEnabled() const { return m_state->device.mSafeListeningPreviewMode.desired; }
bool SettingsViewModel::captureVoiceSupported() const { return m_state->capabilities.hasCaptureVoice; }
bool SettingsViewModel::captureVoiceEnabled() const
{
    const auto* property = generalBoolean(m_state->device, m_state->capabilities.captureVoiceSlot);
    return property && property->desired;
}
bool SettingsViewModel::touchPanelSupported() const { return m_state->capabilities.touchPanelSlot >= 0; }
bool SettingsViewModel::touchPanelEnabled() const
{
    const auto* property = generalBoolean(m_state->device, m_state->capabilities.touchPanelSlot);
    return property && property->desired;
}
bool SettingsViewModel::multipointSupported() const { return m_state->capabilities.multipointSlot >= 0; }
bool SettingsViewModel::multipointEnabled() const
{
    const auto* property = generalBoolean(m_state->device, m_state->capabilities.multipointSlot);
    return property && property->desired;
}
int SettingsViewModel::speakToChatSensitivity() const
{
    return static_cast<int>(m_state->device.mSpeakToChatDetectSensitivity.desired);
}
int SettingsViewModel::speakToChatModeOut() const
{
    return static_cast<int>(m_state->device.mSpeakToModeOutTime.desired);
}
int SettingsViewModel::autoPowerOffValue() const
{
    return m_state->capabilities.hasAutoPowerOffWearingDetection
        ? static_cast<int>(m_state->device.mPowerAutoOffWearingDetection.desired)
        : static_cast<int>(m_state->device.mPowerAutoOff.desired);
}
bool SettingsViewModel::autoPowerOffWearingDetectionSupported() const
{
    return m_state->capabilities.hasAutoPowerOffWearingDetection;
}
bool SettingsViewModel::cinemaModeEnabled() const { return m_state->device.mUpmixCinemaEnabled.desired; }
int SettingsViewModel::listeningRoomSize() const
{
    return static_cast<int>(m_state->device.mBGMModeRoomSize.desired);
}
QVariantList SettingsViewModel::touchOptions() const
{
    QVariantList result;
    auto append = [&result](Preset value) {
        if (value == Preset::QUICK_ACCESS || value == Preset::AMBIENT_SOUND_CONTROL_QUICK_ACCESS
            || value == Preset::LISTENING_MODE_QUICK_ACCESS)
            return;
        const int raw = static_cast<int>(value);
        for (const auto& item : result)
            if (item.toMap().value(QStringLiteral("value")).toInt() == raw)
                return;
        result.push_back(QVariantMap{{QStringLiteral("value"), raw},
                                     {QStringLiteral("name"), touchPresetName(value)}});
    };
    for (const auto value : m_state->device.mTouchFunctionOptions)
        append(value);
    if (result.isEmpty()) {
        append(Preset::AMBIENT_SOUND_CONTROL); append(Preset::VOLUME_CONTROL);
        append(Preset::PLAYBACK_CONTROL); append(Preset::TRACK_CONTROL); append(Preset::NO_FUNCTION);
    }
    append(m_state->device.mTouchFunctionLeft.desired);
    append(m_state->device.mTouchFunctionRight.desired);
    return result;
}

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
    m_state->device.mVoiceGuidanceVolume.desired = std::clamp(volume, -2, 2);
    emit changed();
    return true;
}

bool SettingsViewModel::setTouchLeft(int preset)
{
    const bool reported = std::ranges::any_of(touchOptions(), [preset](const QVariant& item) {
        return item.toMap().value(QStringLiteral("value")).toInt() == preset;
    });
    if (!touchSupported() || !reported) {
        ++m_state->rejectedWrites;
        return false;
    }
    m_state->device.mTouchFunctionLeft.desired = static_cast<Preset>(preset);
    emit changed();
    return true;
}

bool SettingsViewModel::setTouchRight(int preset)
{
    const bool reported = std::ranges::any_of(touchOptions(), [preset](const QVariant& item) {
        return item.toMap().value(QStringLiteral("value")).toInt() == preset;
    });
    if (!touchSupported() || !reported) {
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
    return setAutoPowerOffValue(enabled
        ? static_cast<int>(AutoPowerOffElements::POWER_OFF_IN_15_MIN)
        : static_cast<int>(AutoPowerOffElements::POWER_OFF_DISABLE));
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

bool SettingsViewModel::setAdaptiveVolumeEnabled(bool enabled)
{
    if (!adaptiveVolumeSupported()) { ++m_state->rejectedWrites; return false; }
    m_state->device.mAutoVolumeEnabled.desired = enabled;
    emit changed();
    return true;
}

bool SettingsViewModel::setSafeListeningEnabled(bool enabled)
{
    if (!safeListeningSupported()) {
        ++m_state->rejectedWrites;
        return false;
    }
    m_state->device.mSafeListeningPreviewMode.desired = enabled;
    emit changed();
    return true;
}

bool SettingsViewModel::setCaptureVoiceEnabled(bool enabled)
{
    auto* property = generalBoolean(m_state->device, m_state->capabilities.captureVoiceSlot);
    if (!captureVoiceSupported() || !property) { ++m_state->rejectedWrites; return false; }
    property->desired = enabled;
    emit changed();
    return true;
}

bool SettingsViewModel::setTouchPanelEnabled(bool enabled)
{
    auto* property = generalBoolean(m_state->device, m_state->capabilities.touchPanelSlot);
    if (!property) { ++m_state->rejectedWrites; return false; }
    property->desired = enabled;
    emit changed();
    return true;
}

bool SettingsViewModel::setMultipointEnabled(bool enabled)
{
    auto* property = generalBoolean(m_state->device, m_state->capabilities.multipointSlot);
    if (!property) { ++m_state->rejectedWrites; return false; }
    property->desired = enabled;
    emit changed();
    return true;
}

bool SettingsViewModel::setSpeakToChatSensitivity(int value)
{
    if (!speakToChatSupported() || value < 0 || value > 2) {
        ++m_state->rejectedWrites; return false;
    }
    m_state->device.mSpeakToChatDetectSensitivity.desired = static_cast<DetectSensitivity>(value);
    emit changed();
    return true;
}

bool SettingsViewModel::setSpeakToChatModeOut(int value)
{
    if (!speakToChatSupported() || value < 0 || value > 3) {
        ++m_state->rejectedWrites; return false;
    }
    m_state->device.mSpeakToModeOutTime.desired = static_cast<ModeOutTime>(value);
    emit changed();
    return true;
}

bool SettingsViewModel::setAutoPowerOffValue(int value)
{
    if (!autoPowerOffSupported()) { ++m_state->rejectedWrites; return false; }
    const bool common = value == 0x00 || value == 0x01 || value == 0x02
        || value == 0x03 || value == 0x04 || value == 0x11;
    const bool wearing = m_state->capabilities.hasAutoPowerOffWearingDetection && value == 0x10;
    if (!common && !wearing) { ++m_state->rejectedWrites; return false; }
    if (m_state->capabilities.hasAutoPowerOffWearingDetection)
        m_state->device.mPowerAutoOffWearingDetection.desired
            = static_cast<AutoPowerOffWearingDetectionElements>(value);
    else
        m_state->device.mPowerAutoOff.desired = static_cast<AutoPowerOffElements>(value);
    emit changed();
    return true;
}

bool SettingsViewModel::setCinemaModeEnabled(bool enabled)
{
    if (!listeningModeSupported()) { ++m_state->rejectedWrites; return false; }
    m_state->device.mUpmixCinemaEnabled.desired = enabled;
    if (enabled)
        m_state->device.mBGMModeEnabled.desired = false;
    emit changed();
    return true;
}

bool SettingsViewModel::setListeningRoomSize(int value)
{
    if (!listeningModeSupported() || value < 0 || value > 2) {
        ++m_state->rejectedWrites; return false;
    }
    m_state->device.mBGMModeRoomSize.desired = static_cast<RoomSize>(value);
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
bool DeviceManagerViewModel::sourceSwitchSupported() const { return m_state->capabilities.hasSourceSwitch; }
QVariantList DeviceManagerViewModel::devices() const
{
    QVariantList result;
    int index = 0;
    for (const auto& device : m_state->device.mPairedDevices) {
        result.push_back(QVariantMap{
            {QStringLiteral("name"), QString::fromStdString(device.name)},
            {QStringLiteral("mac"), QString::fromStdString(device.macAddress)},
            {QStringLiteral("connected"), device.connected},
            {QStringLiteral("playback"), index == m_state->device.mPairedDevicesPlaybackDeviceID},
        });
        ++index;
    }
    return result;
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
    if (!supported() || !m_state->capabilities.hasSourceSwitch || !validMac(mac)) {
        ++m_state->rejectedWrites;
        return false;
    }
    m_state->device.mMultipointDeviceMac.desired = mac.toStdString();
    emit changed();
    return true;
}

bool DeviceManagerViewModel::connectDevice(const QString& mac)
{
    if (!supported() || !validMac(mac)) { ++m_state->rejectedWrites; return false; }
    m_state->device.mPairedDeviceConnectMac.desired = mac.toStdString();
    emit changed();
    return true;
}

bool DeviceManagerViewModel::disconnectDevice(const QString& mac)
{
    if (!supported() || !validMac(mac)) { ++m_state->rejectedWrites; return false; }
    m_state->device.mPairedDeviceDisconnectMac.desired = mac.toStdString();
    emit changed();
    return true;
}

bool DeviceManagerViewModel::unpairDevice(const QString& mac)
{
    if (!supported() || !validMac(mac)) { ++m_state->rejectedWrites; return false; }
    m_state->device.mPairedDeviceUnpairMac.desired = mac.toStdString();
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
    const bool clearFailure = state == QStringLiteral("connected")
        && (!m_errorCode.isEmpty() || !m_errorDetail.isEmpty());
    if (m_state == state && !clearFailure)
        return;
    m_state = state;
    if (clearFailure) {
        m_errorCode.clear();
        m_errorDetail.clear();
    }
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
