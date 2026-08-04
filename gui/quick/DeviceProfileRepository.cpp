#include "DeviceProfileRepository.hpp"

#include <algorithm>

namespace {

DeviceCapabilities capabilities(
    bool tws,
    bool noiseControl,
    bool speakToChat,
    bool dsee,
    bool listeningMode,
    bool autoPause,
    bool headGesture)
{
    DeviceCapabilities value;
    value.hasBatteryL = true;
    value.hasBatteryR = tws;
    value.hasBatteryCase = tws;
    value.hasPowerOff = true;
    value.hasNoiseCancelling = noiseControl;
    value.hasAmbientSound = noiseControl;
    value.hasEqualizer = true;
    value.hasDSEE = dsee;
    value.hasConnectionQuality = true;
    value.hasSpeakToChat = speakToChat;
    value.hasMultipoint = true;
    value.hasAutoPowerOff = true;
    value.hasAutoPause = autoPause;
    value.hasTouchFunction = true;
    value.hasNcButtonFunction = noiseControl;
    value.hasHeadGesture = headGesture;
    value.hasVoiceGuidance = true;
    value.hasVoiceGuidanceVolume = true;
    value.hasDeviceManagement = true;
    value.hasListeningMode = listeningMode;
    return value;
}

DeviceProfile makeProfile(const QString& id,
                          const QString& family,
                          const QString& protocol,
                          const QString& assetPath,
                          const DeviceCapabilities& caps,
                          bool referenceBlocked = false)
{
    return {
        id,
        id,
        family,
        protocol,
        assetPath,
        QStringLiteral("1.0.0"),
        referenceBlocked,
        caps,
    };
}

} // namespace

DeviceProfileRepository::DeviceProfileRepository()
{
    auto maxTws = capabilities(true, true, true, true, false, true, true);
    maxTws.hasAutoAsm = true;
    maxTws.hasNoiseAdaptiveSensitivity = true;
    maxTws.hasSafeListening = true;

    auto maxHeadband = capabilities(false, true, true, true, true, true, true);
    maxHeadband.hasAutoAsm = true;
    maxHeadband.hasNoiseAdaptiveSensitivity = true;
    maxHeadband.hasSafeListening = true;

    m_profiles = {
        makeProfile(QStringLiteral("WH-1000XM6"), QStringLiteral("headband"),
                    QStringLiteral("MDR v2"), QStringLiteral("qrc:/devices/wh-1000xm6.png"),
                    maxHeadband),
        makeProfile(QStringLiteral("WH-1000XM5"), QStringLiteral("headband"),
                    QStringLiteral("MDR v2"), QStringLiteral("qrc:/devices/wh-1000xm5.png"),
                    capabilities(false, true, true, true, false, true, true)),
        makeProfile(QStringLiteral("WH-CH720N"), QStringLiteral("headband"),
                    QStringLiteral("MDR v2"), QStringLiteral("qrc:/devices/wh-ch720n.png"),
                    capabilities(false, true, false, true, false, false, false)),
        makeProfile(QStringLiteral("WF-1000XM6"), QStringLiteral("tws"),
                    QStringLiteral("MDR v2"), QStringLiteral("qrc:/devices/wf-1000xm5.png"),
                    maxTws, true),
        makeProfile(QStringLiteral("WF-1000XM5"), QStringLiteral("tws"),
                    QStringLiteral("MDR v2"), QStringLiteral("qrc:/devices/wf-1000xm5.png"),
                    maxTws),
        makeProfile(QStringLiteral("WF-C510"), QStringLiteral("tws"),
                    QStringLiteral("MDR v2"), QStringLiteral("qrc:/devices/wf-c700n.png"),
                    capabilities(true, true, false, false, false, false, false), true),
        makeProfile(QStringLiteral("WF-L910"), QStringLiteral("open-tws"),
                    QStringLiteral("MDR v2"), QStringLiteral("qrc:/devices/linkbuds-s.png"),
                    capabilities(true, false, true, true, true, true, true), true),
        makeProfile(QStringLiteral("WF-LS900N"), QStringLiteral("tws"),
                    QStringLiteral("MDR v2"), QStringLiteral("qrc:/devices/linkbuds-s.png"),
                    capabilities(true, true, true, true, false, true, false)),
        makeProfile(QStringLiteral("generic-max"), QStringLiteral("headband"),
                    QStringLiteral("MDR v2"), QStringLiteral("qrc:/devices/wh-1000xm6.png"),
                    maxHeadband, true),
        makeProfile(QStringLiteral("generic-tws"), QStringLiteral("tws"),
                    QStringLiteral("MDR v2"), QStringLiteral("qrc:/devices/wf-1000xm5.png"),
                    maxTws, true),
        makeProfile(QStringLiteral("generic-headband"), QStringLiteral("headband"),
                    QStringLiteral("MDR v2"), QStringLiteral("qrc:/devices/wh-1000xm5.png"),
                    capabilities(false, true, true, true, false, true, false), true),
        makeProfile(QStringLiteral("generic-v1"), QStringLiteral("headband"),
                    QStringLiteral("MDR v1"), QStringLiteral("qrc:/devices/wh-1000xm3.png"),
                    capabilities(false, true, false, false, false, false, false), true),
    };
}

const DeviceProfile* DeviceProfileRepository::find(const QString& id) const
{
    const auto it = std::find_if(m_profiles.cbegin(), m_profiles.cend(),
                                 [&id](const DeviceProfile& profile) {
                                     return profile.id.compare(id, Qt::CaseInsensitive) == 0;
                                 });
    return it == m_profiles.cend() ? nullptr : &*it;
}

const DeviceProfile& DeviceProfileRepository::fallback() const
{
    return m_profiles.front();
}

QStringList DeviceProfileRepository::profileIds() const
{
    QStringList result;
    result.reserve(m_profiles.size());
    for (const auto& profile : m_profiles)
        result.push_back(profile.id);
    return result;
}

void DeviceProfileRepository::apply(const DeviceProfile& profile,
                                    mdr::MDRHeadphones& device,
                                    DeviceCapabilities& capabilitiesOut) const
{
    using namespace mdr::v2::t1;

    device = mdr::MDRHeadphones();
    device.mModelName = profile.modelName.toStdString();
    device.mFWVersion = profile.firmware.toStdString();
    device.mUniqueId = "00:11:22:33:44:55";
    device.mProtocol = {profile.protocol.endsWith(QLatin1Char('1')) ? 1 : 2, 1, 1};
    device.mBatteryL.level = 86;
    device.mBatteryR.level = 81;
    device.mBatteryCase.level = 64;
    device.mPlayTrackTitle = "Midnight Drive";
    device.mPlayTrackArtist = "SonyHeadphonesClient";
    device.mPlayPause = PlaybackStatus::PLAY;
    device.mPlayVolume.overwrite(18);
    device.mNcAsmEnabled.overwrite(profile.capabilities.hasNoiseCancelling);
    device.mNcAsmMode.overwrite(NcAsmMode::NC);
    device.mNcAsmAmbientLevel.overwrite(10);
    device.mNcAsmFocusOnVoice.overwrite(true);
    device.mNcAsmAutoAsmEnabled.overwrite(false);
    device.mSpeakToChatEnabled.overwrite(profile.capabilities.hasSpeakToChat);
    device.mEqAvailable.overwrite(profile.capabilities.hasEqualizer);
    device.mEqPresetId.overwrite(EqPresetId::CUSTOM);
    device.mEqClearBass.overwrite(2);
    device.mEqConfig.overwrite({1, 2, 3, 2, 1});
    device.mUpscalingAvailable = profile.capabilities.hasDSEE;
    device.mUpscalingEnabled.overwrite(profile.capabilities.hasDSEE);
    device.mAudioPriorityMode.overwrite(PriorMode::SOUND_QUALITY_PRIOR);
    device.mPowerAutoOff.overwrite(AutoPowerOffElements::POWER_OFF_IN_15_MIN);
    device.mAutoPauseEnabled.overwrite(profile.capabilities.hasAutoPause);
    device.mTouchFunctionLeft.overwrite(Preset::AMBIENT_SOUND_CONTROL);
    device.mTouchFunctionRight.overwrite(Preset::PLAYBACK_CONTROL);
    device.mNcAsmButtonFunction.overwrite(Function::NC_ASM_OFF);
    device.mHeadGestureEnabled.overwrite(profile.capabilities.hasHeadGesture);
    device.mBGMModeEnabled.overwrite(false);
    device.mUpmixCinemaEnabled.overwrite(false);
    device.mVoiceGuidanceEnabled.overwrite(profile.capabilities.hasVoiceGuidance);
    device.mVoiceGuidanceVolume.overwrite(3);

    capabilitiesOut = profile.capabilities;
}
