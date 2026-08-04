#include "DeviceCapabilities.hpp"

void DeviceCapabilities::probeFromDevice(const mdr::MDRHeadphones& device)
{
    using Table1 = mdr::v2::MessageMdrV2FunctionType_Table1;
    using Table2 = mdr::v2::MessageMdrV2FunctionType_Table2;
    const auto supports = [&device](auto value) { return device.mSupport.contains(value); };

    hasBatteryL = supports(Table1::LEFT_RIGHT_BATTERY_LEVEL_INDICATOR)
               || supports(Table1::LR_BATTERY_LEVEL_WITH_THRESHOLD);
    hasBatteryR = hasBatteryL;
    hasBatteryCase = supports(Table1::CRADLE_BATTERY_LEVEL_INDICATOR)
                  || supports(Table1::CRADLE_BATTERY_LEVEL_WITH_THRESHOLD);
    hasSafeListening = supports(Table2::SAFE_LISTENING_HBS_1)
                    || supports(Table2::SAFE_LISTENING_TWS_1)
                    || supports(Table2::SAFE_LISTENING_HBS_2)
                    || supports(Table2::SAFE_LISTENING_TWS_2);
    hasPowerOff = supports(Table1::POWER_OFF);

    hasNoiseCancelling = supports(Table1::NOISE_CANCELLING_ONOFF)
                      || supports(Table1::NOISE_CANCELLING_ONOFF_AND_AMBIENT_SOUND_MODE_ONOFF)
                      || supports(Table1::NOISE_CANCELLING_DUAL_SINGLE_OFF_AND_AMBIENT_SOUND_MODE_ONOFF)
                      || supports(Table1::NOISE_CANCELLING_ONOFF_AND_AMBIENT_SOUND_MODE_LEVEL_ADJUSTMENT)
                      || supports(Table1::NOISE_CANCELLING_DUAL_SINGLE_OFF_AMBIENT_SOUND_MODE_LEVEL_ADJUSTMENT)
                      || supports(Table1::MODE_NC_ASM_NOISE_CANCELLING_DUAL_AUTO_AMBIENT_SOUND_MODE_LEVEL_ADJUSTMENT)
                      || supports(Table1::MODE_NC_ASM_NOISE_CANCELLING_DUAL_SINGLE_AMBIENT_SOUND_MODE_LEVEL_ADJUSTMENT)
                      || supports(Table1::MODE_NC_ASM_NOISE_CANCELLING_DUAL_AMBIENT_SOUND_MODE_LEVEL_ADJUSTMENT)
                      || supports(Table1::MODE_NC_NCSS_ASM_NOISE_CANCELLING_DUAL_AMBIENT_SOUND_MODE_LEVEL_ADJUSTMENT_WITH_TEST_MODE)
                      || supports(Table1::MODE_NC_ASM_NOISE_CANCELLING_DUAL_AMBIENT_SOUND_MODE_LEVEL_ADJUSTMENT_NOISE_ADAPTATION);
    hasAmbientSound = supports(Table1::NOISE_CANCELLING_ONOFF_AND_AMBIENT_SOUND_MODE_ONOFF)
                   || supports(Table1::NOISE_CANCELLING_DUAL_SINGLE_OFF_AND_AMBIENT_SOUND_MODE_ONOFF)
                   || supports(Table1::NOISE_CANCELLING_ONOFF_AND_AMBIENT_SOUND_MODE_LEVEL_ADJUSTMENT)
                   || supports(Table1::NOISE_CANCELLING_DUAL_SINGLE_OFF_AMBIENT_SOUND_MODE_LEVEL_ADJUSTMENT)
                   || supports(Table1::AMBIENT_SOUND_MODE_ONOFF)
                   || supports(Table1::AMBIENT_SOUND_MODE_LEVEL_ADJUSTMENT)
                   || supports(Table1::MODE_NC_ASM_NOISE_CANCELLING_DUAL_AUTO_AMBIENT_SOUND_MODE_LEVEL_ADJUSTMENT)
                   || supports(Table1::AMBIENT_SOUND_CONTROL_MODE_SELECT)
                   || supports(Table1::MODE_NC_ASM_NOISE_CANCELLING_DUAL_SINGLE_AMBIENT_SOUND_MODE_LEVEL_ADJUSTMENT)
                   || supports(Table1::MODE_NC_ASM_NOISE_CANCELLING_DUAL_AMBIENT_SOUND_MODE_LEVEL_ADJUSTMENT)
                   || supports(Table1::MODE_NC_NCSS_ASM_NOISE_CANCELLING_DUAL_AMBIENT_SOUND_MODE_LEVEL_ADJUSTMENT_WITH_TEST_MODE)
                   || supports(Table1::MODE_NC_ASM_NOISE_CANCELLING_DUAL_AMBIENT_SOUND_MODE_LEVEL_ADJUSTMENT_NOISE_ADAPTATION);
    hasAutoAsm = supports(Table1::MODE_NC_ASM_NOISE_CANCELLING_DUAL_AMBIENT_SOUND_MODE_LEVEL_ADJUSTMENT_NOISE_ADAPTATION);
    hasNoiseAdaptiveSensitivity = hasAutoAsm;
    hasEqualizer = device.mEqAvailable.current;
    hasDSEE = device.mUpscalingAvailable;
    hasConnectionQuality = supports(
        Table1::CONNECTION_MODE_SOUND_QUALITY_CONNECTION_QUALITY);
    hasSpeakToChat = supports(Table1::SMART_TALKING_MODE_TYPE2);
    hasAdaptiveVolume = supports(Table1::AUTO_VOLUME);
    hasDeviceManagement = supports(Table2::PAIRING_DEVICE_MANAGEMENT_CLASSIC_BT)
                       || supports(Table2::PAIRING_DEVICE_MANAGEMENT_WITH_BLUETOOTH_CLASS_OF_DEVICE_CLASSIC_BT)
                       || supports(Table2::PAIRING_DEVICE_MANAGEMENT_WITH_BLUETOOTH_CLASS_OF_DEVICE_CLASSIC_LE);
    hasMultipoint = hasDeviceManagement;
    hasAutoPowerOff = supports(Table1::AUTO_POWER_OFF)
                   || supports(Table1::AUTO_POWER_OFF_WITH_WEARING_DETECTION);
    hasAutoPause = supports(Table1::PLAYBACK_CONTROL_BY_WEARING_REMOVING_HEADPHONE_ON_OFF);
    hasTouchFunction = supports(Table1::ASSIGNABLE_SETTING)
                    || supports(Table1::ASSIGNABLE_SETTING_WITH_LIMITATION);
    hasNcButtonFunction = supports(Table1::AMBIENT_SOUND_CONTROL_MODE_SELECT);
    hasHeadGesture = supports(Table1::HEAD_GESTURE_ON_OFF_TRAINING);
    hasVoiceGuidance = supports(Table2::VOICE_GUIDANCE_SETTING_MTK_TRANSFER_WITHOUT_DISCONNECTION_NOT_SUPPORT_LANGUAGE_SWITCH)
                    || supports(Table2::VOICE_GUIDANCE_SETTING_MTK_TRANSFER_WITHOUT_DISCONNECTION_SUPPORT_LANGUAGE_SWITCH)
                    || supports(Table2::VOICE_GUIDANCE_SETTING_MTK_TRANSFER_WITHOUT_DISCONNECTION_SUPPORT_LANGUAGE_SWITCH_AND_VOLUME_ADJUSTMENT)
                    || supports(Table2::VOICE_GUIDANCE_SETTING_SUPPORT_LANGUAGE_SWITCH)
                    || supports(Table2::VOICE_GUIDANCE_SETTING_ONLY_ON_OFF_SWITCH);
    hasVoiceGuidanceVolume = supports(Table2::VOICE_GUIDANCE_SETTING_MTK_TRANSFER_WITHOUT_DISCONNECTION_SUPPORT_LANGUAGE_SWITCH_AND_VOLUME_ADJUSTMENT)
                          || supports(Table2::VOICE_GUIDANCE_VOLUME_SETTING_MTK_FIXED_TO_5_STEPS);
    hasListeningMode = supports(Table1::LISTENING_OPTION)
                    || supports(Table1::BGM_MODE_SMALL_MIDDLE_LARGE)
                    || supports(Table1::BGM_MODE_SMALL_MIDDLE_LARGE_AND_ERRORCODE);
    hasSurround = false; // V1 only
}
