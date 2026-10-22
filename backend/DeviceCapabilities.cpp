#include "DeviceCapabilities.hpp"

void DeviceCapabilities::probeFromDevice(const mdr::MDRHeadphones& device)
{
    using Table1 = mdr::v2::MessageMdrV2FunctionType_Table1;
    using Table2 = mdr::v2::MessageMdrV2FunctionType_Table2;
    const auto supports = [&device](auto value) { return device.mSupport.contains(value); };

    hasBatteryL = supports(Table1::BATTERY_LEVEL_INDICATOR)
               || supports(Table1::BATTERY_LEVEL_WITH_THRESHOLD)
               || supports(Table1::LEFT_RIGHT_BATTERY_LEVEL_INDICATOR)
               || supports(Table1::LR_BATTERY_LEVEL_WITH_THRESHOLD);
    hasBatteryR = supports(Table1::LEFT_RIGHT_BATTERY_LEVEL_INDICATOR)
               || supports(Table1::LR_BATTERY_LEVEL_WITH_THRESHOLD);
    hasBatteryCase = supports(Table1::CRADLE_BATTERY_LEVEL_INDICATOR)
                  || supports(Table1::CRADLE_BATTERY_LEVEL_WITH_THRESHOLD);
    hasSafeListening = supports(Table2::SAFE_LISTENING_HBS_1)
                    || supports(Table2::SAFE_LISTENING_TWS_1)
                    || supports(Table2::SAFE_LISTENING_HBS_2)
                    || supports(Table2::SAFE_LISTENING_TWS_2);
    hasPowerOff = supports(Table1::POWER_OFF);
    hasPlaybackControl = supports(Table1::PLAYBACK_CONTROLLER_WITH_CALL_VOLUME_ADJUSTMENT)
                      || supports(Table1::PLAYBACK_CONTROLLER_WITH_CALL_VOLUME_ADJUSTMENT_AND_MUTE)
                      || supports(Table1::PLAYBACK_CONTROLLER_WITH_CALL_VOLUME_ADJUSTMENT_AND_FUNCTION_CHANGE)
                      || supports(Table1::PLAYBACK_CONTROLLER_WITH_FUNCTION_CHANGE);
    hasPlaybackMetadata = hasPlaybackControl;
    hasVolumeControl = hasPlaybackControl;

    // Only advertise packet variants that the V2 sync/commit path implements exactly.
    const bool hasDualNcAsm = supports(
        Table1::MODE_NC_ASM_NOISE_CANCELLING_DUAL_AMBIENT_SOUND_MODE_LEVEL_ADJUSTMENT);
    const bool hasAdaptiveDualNcAsm = supports(
        Table1::MODE_NC_ASM_NOISE_CANCELLING_DUAL_AMBIENT_SOUND_MODE_LEVEL_ADJUSTMENT_NOISE_ADAPTATION);
    hasNoiseCancelling = hasDualNcAsm || hasAdaptiveDualNcAsm;
    hasAmbientSound = hasNoiseCancelling || supports(Table1::AMBIENT_SOUND_MODE_LEVEL_ADJUSTMENT);
    hasAutoAsm = supports(Table1::MODE_NC_ASM_NOISE_CANCELLING_DUAL_AMBIENT_SOUND_MODE_LEVEL_ADJUSTMENT_NOISE_ADAPTATION);
    hasNoiseAdaptiveSensitivity = hasAutoAsm;
    hasEqualizer = device.mEqAvailable.current;
    hasDSEE = device.mUpscalingAvailable;
    hasConnectionQuality = supports(
        Table1::CONNECTION_MODE_SOUND_QUALITY_CONNECTION_QUALITY);
    hasSpeakToChat = supports(Table1::SMART_TALKING_MODE_TYPE2);
    hasAdaptiveVolume = supports(Table1::AUTO_VOLUME);
    // General-setting slot numbers are device-specific. WH-1000XM5 reports
    // TOUCH_PANEL_SETTING in slot 1, not the call-voice function.
    captureVoiceSlot = touchPanelSlot = multipointSlot = -1;
    const Table1 functions[] = {Table1::GENERAL_SETTING_1, Table1::GENERAL_SETTING_2,
                               Table1::GENERAL_SETTING_3, Table1::GENERAL_SETTING_4};
    const mdr::MDRHeadphones::GsCapability* general[] = {
        &device.mGsCapability1, &device.mGsCapability2,
        &device.mGsCapability3, &device.mGsCapability4};
    for (int i = 0; i < 4; ++i) {
        if (!supports(functions[i]) || general[i]->type != mdr::v2::t1::GsSettingType::BOOLEAN_TYPE)
            continue;
        const auto& subject = general[i]->value.subject.value;
        if (subject == "TOUCH_PANEL_SETTING") touchPanelSlot = i;
        if (subject == "MULTIPOINT_SETTING") multipointSlot = i;
        if (subject.starts_with("CAPTURE_VOICE")) captureVoiceSlot = i;
    }
    hasCaptureVoice = captureVoiceSlot >= 0;
    hasDeviceManagement = supports(Table2::PAIRING_DEVICE_MANAGEMENT_CLASSIC_BT)
                       || supports(Table2::PAIRING_DEVICE_MANAGEMENT_WITH_BLUETOOTH_CLASS_OF_DEVICE_CLASSIC_BT)
                       || supports(Table2::PAIRING_DEVICE_MANAGEMENT_WITH_BLUETOOTH_CLASS_OF_DEVICE_CLASSIC_LE);
    hasMultipoint = multipointSlot >= 0;
    hasSourceSwitch = supports(Table2::SOURCE_SWITCH_CONTROL);
    hasAutoPowerOff = supports(Table1::AUTO_POWER_OFF)
                   || supports(Table1::AUTO_POWER_OFF_WITH_WEARING_DETECTION);
    hasAutoPowerOffWearingDetection = supports(Table1::AUTO_POWER_OFF_WITH_WEARING_DETECTION);
    hasAutoPause = supports(Table1::PLAYBACK_CONTROL_BY_WEARING_REMOVING_HEADPHONE_ON_OFF);
    hasTouchFunction = supports(Table1::ASSIGNABLE_SETTING)
                    || supports(Table1::ASSIGNABLE_SETTING_WITH_LIMITATION);
    hasLimitedTouchFunction = supports(Table1::ASSIGNABLE_SETTING_WITH_LIMITATION);
    hasNcButtonFunction = supports(Table1::AMBIENT_SOUND_CONTROL_MODE_SELECT);
    hasHeadGesture = supports(Table1::HEAD_GESTURE_ON_OFF_TRAINING);
    hasVoiceGuidance = supports(Table2::VOICE_GUIDANCE_SETTING_MTK_TRANSFER_WITHOUT_DISCONNECTION_NOT_SUPPORT_LANGUAGE_SWITCH)
                    || supports(Table2::VOICE_GUIDANCE_SETTING_MTK_TRANSFER_WITHOUT_DISCONNECTION_SUPPORT_LANGUAGE_SWITCH)
                    || supports(Table2::VOICE_GUIDANCE_SETTING_MTK_TRANSFER_WITHOUT_DISCONNECTION_SUPPORT_LANGUAGE_SWITCH_AND_VOLUME_ADJUSTMENT)
                    || supports(Table2::VOICE_GUIDANCE_SETTING_SUPPORT_LANGUAGE_SWITCH)
                    || supports(Table2::VOICE_GUIDANCE_SETTING_ONLY_ON_OFF_SWITCH);
    hasVoiceGuidanceVolume = supports(Table2::VOICE_GUIDANCE_SETTING_MTK_TRANSFER_WITHOUT_DISCONNECTION_SUPPORT_LANGUAGE_SWITCH_AND_VOLUME_ADJUSTMENT)
                          || supports(Table2::VOICE_GUIDANCE_VOLUME_SETTING_MTK_FIXED_TO_5_STEPS);
    hasListeningMode = supports(Table1::LISTENING_OPTION);
    hasSurround = false; // V1 only
}
