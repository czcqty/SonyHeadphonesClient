#pragma once

#include <mdr/Headphones.hpp>

struct DeviceCapabilities
{
    bool hasBatteryL = false;
    bool hasBatteryR = false;
    bool hasBatteryCase = false;
    bool hasSafeListening = false;
    bool hasPowerOff = false;
    bool hasNoiseCancelling = false;
    bool hasAmbientSound = false;
    bool hasAutoAsm = false;
    bool hasNoiseAdaptiveSensitivity = false;
    bool hasEqualizer = false;
    bool hasDSEE = false;
    bool hasConnectionQuality = false;
    bool hasSpeakToChat = false;
    bool hasAdaptiveVolume = false;
    bool hasMultipoint = false;
    bool hasAutoPowerOff = false;
    bool hasAutoPause = false;
    bool hasTouchFunction = false;
    bool hasNcButtonFunction = false;
    bool hasHeadGesture = false;
    bool hasVoiceGuidance = false;
    bool hasVoiceGuidanceVolume = false;
    bool hasDeviceManagement = false;
    bool hasListeningMode = false;  // BGM/Cinema
    bool hasSurround = false;       // VPT (v1 only)

    void probeFromDevice(const mdr::MDRHeadphones& device);
};
