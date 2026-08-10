#pragma once

#include <QObject>
#include <QVariantList>

#include <mdr/Headphones.hpp>

#include "../DeviceCapabilities.hpp"

struct DeviceState
{
    mdr::MDRHeadphones device;
    DeviceCapabilities capabilities;
    int rejectedWrites = 0;
};

class PlaybackViewModel final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString title READ title NOTIFY changed)
    Q_PROPERTY(QString artist READ artist NOTIFY changed)
    Q_PROPERTY(QString album READ album NOTIFY changed)
    Q_PROPERTY(QString firmware READ firmware NOTIFY changed)
    Q_PROPERTY(QString codec READ codec NOTIFY changed)
    Q_PROPERTY(QString alertMessage READ alertMessage NOTIFY changed)
    Q_PROPERTY(int volume READ volume NOTIFY changed)
    Q_PROPERTY(int batteryLeft READ batteryLeft NOTIFY changed)
    Q_PROPERTY(int batteryRight READ batteryRight NOTIFY changed)
    Q_PROPERTY(int batteryCase READ batteryCase NOTIFY changed)
    Q_PROPERTY(bool hasRightBattery READ hasRightBattery NOTIFY changed)
    Q_PROPERTY(bool hasCaseBattery READ hasCaseBattery NOTIFY changed)
    Q_PROPERTY(bool powerOffSupported READ powerOffSupported NOTIFY changed)
    Q_PROPERTY(bool playbackSupported READ playbackSupported NOTIFY changed)
    Q_PROPERTY(bool volumeSupported READ volumeSupported NOTIFY changed)
    Q_PROPERTY(bool safeListeningSupported READ safeListeningSupported NOTIFY changed)
    Q_PROPERTY(int safeListeningPressure READ safeListeningPressure NOTIFY changed)
    Q_PROPERTY(bool batteryLeftCharging READ batteryLeftCharging NOTIFY changed)
    Q_PROPERTY(bool batteryRightCharging READ batteryRightCharging NOTIFY changed)
    Q_PROPERTY(bool batteryCaseCharging READ batteryCaseCharging NOTIFY changed)

public:
    explicit PlaybackViewModel(DeviceState* state, QObject* parent = nullptr);

    QString title() const;
    QString artist() const;
    QString album() const;
    QString firmware() const;
    QString codec() const;
    QString alertMessage() const;
    int volume() const;
    int batteryLeft() const;
    int batteryRight() const;
    int batteryCase() const;
    bool hasRightBattery() const;
    bool hasCaseBattery() const;
    bool powerOffSupported() const;
    bool playbackSupported() const;
    bool volumeSupported() const;
    bool safeListeningSupported() const;
    int safeListeningPressure() const;
    bool batteryLeftCharging() const;
    bool batteryRightCharging() const;
    bool batteryCaseCharging() const;

    Q_INVOKABLE bool setVolume(int value);
    Q_INVOKABLE bool togglePlayback();
    Q_INVOKABLE bool previousTrack();
    Q_INVOKABLE bool nextTrack();
    Q_INVOKABLE bool powerOff();
    void refresh();

signals:
    void changed();

private:
    DeviceState* m_state;
};

class NoiseControlViewModel final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool supported READ supported NOTIFY changed)
    Q_PROPERTY(bool ambientSupported READ ambientSupported NOTIFY changed)
    Q_PROPERTY(int mode READ mode NOTIFY changed)
    Q_PROPERTY(int ambientLevel READ ambientLevel NOTIFY changed)
    Q_PROPERTY(bool focusOnVoice READ focusOnVoice NOTIFY changed)
    Q_PROPERTY(bool adaptiveSupported READ adaptiveSupported NOTIFY changed)
    Q_PROPERTY(bool adaptiveEnabled READ adaptiveEnabled NOTIFY changed)
    Q_PROPERTY(int adaptiveSensitivity READ adaptiveSensitivity NOTIFY changed)

public:
    explicit NoiseControlViewModel(DeviceState* state, QObject* parent = nullptr);
    bool supported() const;
    bool ambientSupported() const;
    int mode() const;
    int ambientLevel() const;
    bool focusOnVoice() const;
    bool adaptiveSupported() const;
    bool adaptiveEnabled() const;
    int adaptiveSensitivity() const;

    Q_INVOKABLE bool setMode(int mode);
    Q_INVOKABLE bool setAmbientLevel(int level);
    Q_INVOKABLE bool setFocusOnVoice(bool enabled);
    Q_INVOKABLE bool setAdaptiveEnabled(bool enabled);
    Q_INVOKABLE bool setAdaptiveSensitivity(int value);
    void refresh();

signals:
    void changed();

private:
    DeviceState* m_state;
};

class EqualizerViewModel final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool supported READ supported NOTIFY changed)
    Q_PROPERTY(QString presetId READ presetId NOTIFY changed)
    Q_PROPERTY(QVariantList bands READ bands NOTIFY changed)
    Q_PROPERTY(int clearBass READ clearBass NOTIFY changed)
    Q_PROPERTY(QVariantList presetOptions READ presetOptions CONSTANT)

public:
    explicit EqualizerViewModel(DeviceState* state, QObject* parent = nullptr);
    bool supported() const;
    QString presetId() const;
    QVariantList bands() const;
    int clearBass() const;
    QVariantList presetOptions() const;

    Q_INVOKABLE bool setBand(int index, int value);
    Q_INVOKABLE bool setClearBass(int value);
    Q_INVOKABLE bool selectPreset(const QString& presetId);
    void refresh();

signals:
    void changed();

private:
    DeviceState* m_state;
};

class SettingsViewModel final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool dseeSupported READ dseeSupported NOTIFY changed)
    Q_PROPERTY(bool dseeEnabled READ dseeEnabled NOTIFY changed)
    Q_PROPERTY(bool speakToChatSupported READ speakToChatSupported NOTIFY changed)
    Q_PROPERTY(bool speakToChatEnabled READ speakToChatEnabled NOTIFY changed)
    Q_PROPERTY(bool autoPauseSupported READ autoPauseSupported NOTIFY changed)
    Q_PROPERTY(bool autoPauseEnabled READ autoPauseEnabled NOTIFY changed)
    Q_PROPERTY(bool touchSupported READ touchSupported NOTIFY changed)
    Q_PROPERTY(int touchLeft READ touchLeft NOTIFY changed)
    Q_PROPERTY(int touchRight READ touchRight NOTIFY changed)
    Q_PROPERTY(bool ncButtonSupported READ ncButtonSupported NOTIFY changed)
    Q_PROPERTY(int ncButtonFunction READ ncButtonFunction NOTIFY changed)
    Q_PROPERTY(bool voiceGuidanceSupported READ voiceGuidanceSupported NOTIFY changed)
    Q_PROPERTY(bool voiceGuidanceEnabled READ voiceGuidanceEnabled NOTIFY changed)
    Q_PROPERTY(bool voiceGuidanceVolumeSupported READ voiceGuidanceVolumeSupported NOTIFY changed)
    Q_PROPERTY(int voiceGuidanceVolume READ voiceGuidanceVolume NOTIFY changed)
    Q_PROPERTY(bool connectionQualitySupported READ connectionQualitySupported NOTIFY changed)
    Q_PROPERTY(bool soundQualityPrior READ soundQualityPrior NOTIFY changed)
    Q_PROPERTY(bool autoPowerOffSupported READ autoPowerOffSupported NOTIFY changed)
    Q_PROPERTY(bool autoPowerOffEnabled READ autoPowerOffEnabled NOTIFY changed)
    Q_PROPERTY(bool headGestureSupported READ headGestureSupported NOTIFY changed)
    Q_PROPERTY(bool headGestureEnabled READ headGestureEnabled NOTIFY changed)
    Q_PROPERTY(bool listeningModeSupported READ listeningModeSupported NOTIFY changed)
    Q_PROPERTY(bool listeningModeEnabled READ listeningModeEnabled NOTIFY changed)
    Q_PROPERTY(bool adaptiveVolumeSupported READ adaptiveVolumeSupported NOTIFY changed)
    Q_PROPERTY(bool adaptiveVolumeEnabled READ adaptiveVolumeEnabled NOTIFY changed)
    Q_PROPERTY(bool safeListeningSupported READ safeListeningSupported NOTIFY changed)
    Q_PROPERTY(bool safeListeningEnabled READ safeListeningEnabled NOTIFY changed)
    Q_PROPERTY(bool captureVoiceSupported READ captureVoiceSupported NOTIFY changed)
    Q_PROPERTY(bool captureVoiceEnabled READ captureVoiceEnabled NOTIFY changed)
    Q_PROPERTY(int speakToChatSensitivity READ speakToChatSensitivity NOTIFY changed)
    Q_PROPERTY(int speakToChatModeOut READ speakToChatModeOut NOTIFY changed)
    Q_PROPERTY(int autoPowerOffValue READ autoPowerOffValue NOTIFY changed)
    Q_PROPERTY(bool autoPowerOffWearingDetectionSupported READ autoPowerOffWearingDetectionSupported NOTIFY changed)
    Q_PROPERTY(bool cinemaModeEnabled READ cinemaModeEnabled NOTIFY changed)
    Q_PROPERTY(int listeningRoomSize READ listeningRoomSize NOTIFY changed)
    Q_PROPERTY(QVariantList touchOptions READ touchOptions NOTIFY changed)

public:
    explicit SettingsViewModel(DeviceState* state, QObject* parent = nullptr);
    bool dseeSupported() const;
    bool dseeEnabled() const;
    bool speakToChatSupported() const;
    bool speakToChatEnabled() const;
    bool autoPauseSupported() const;
    bool autoPauseEnabled() const;
    bool touchSupported() const;
    int touchLeft() const;
    int touchRight() const;
    bool ncButtonSupported() const;
    int ncButtonFunction() const;
    bool voiceGuidanceSupported() const;
    bool voiceGuidanceEnabled() const;
    bool voiceGuidanceVolumeSupported() const;
    int voiceGuidanceVolume() const;
    bool connectionQualitySupported() const;
    bool soundQualityPrior() const;
    bool autoPowerOffSupported() const;
    bool autoPowerOffEnabled() const;
    bool headGestureSupported() const;
    bool headGestureEnabled() const;
    bool listeningModeSupported() const;
    bool listeningModeEnabled() const;
    bool adaptiveVolumeSupported() const;
    bool adaptiveVolumeEnabled() const;
    bool safeListeningSupported() const;
    bool safeListeningEnabled() const;
    bool captureVoiceSupported() const;
    bool captureVoiceEnabled() const;
    int speakToChatSensitivity() const;
    int speakToChatModeOut() const;
    int autoPowerOffValue() const;
    bool autoPowerOffWearingDetectionSupported() const;
    bool cinemaModeEnabled() const;
    int listeningRoomSize() const;
    QVariantList touchOptions() const;

    Q_INVOKABLE bool setDseeEnabled(bool enabled);
    Q_INVOKABLE bool setSpeakToChatEnabled(bool enabled);
    Q_INVOKABLE bool setAutoPauseEnabled(bool enabled);
    Q_INVOKABLE bool setVoiceGuidanceEnabled(bool enabled);
    Q_INVOKABLE bool setVoiceGuidanceVolume(int volume);
    Q_INVOKABLE bool setTouchLeft(int preset);
    Q_INVOKABLE bool setTouchRight(int preset);
    Q_INVOKABLE bool setNcButtonFunction(int function);
    Q_INVOKABLE bool setSoundQualityPrior(bool enabled);
    Q_INVOKABLE bool setAutoPowerOffEnabled(bool enabled);
    Q_INVOKABLE bool setHeadGestureEnabled(bool enabled);
    Q_INVOKABLE bool setListeningModeEnabled(bool enabled);
    Q_INVOKABLE bool setAdaptiveVolumeEnabled(bool enabled);
    Q_INVOKABLE bool setSafeListeningEnabled(bool enabled);
    Q_INVOKABLE bool setCaptureVoiceEnabled(bool enabled);
    Q_INVOKABLE bool setSpeakToChatSensitivity(int value);
    Q_INVOKABLE bool setSpeakToChatModeOut(int value);
    Q_INVOKABLE bool setAutoPowerOffValue(int value);
    Q_INVOKABLE bool setCinemaModeEnabled(bool enabled);
    Q_INVOKABLE bool setListeningRoomSize(int value);
    void refresh();

signals:
    void changed();

private:
    DeviceState* m_state;
};

class DeviceManagerViewModel final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool supported READ supported NOTIFY changed)
    Q_PROPERTY(bool pairingMode READ pairingMode NOTIFY changed)
    Q_PROPERTY(bool sourceSwitchSupported READ sourceSwitchSupported NOTIFY changed)
    Q_PROPERTY(QVariantList devices READ devices NOTIFY changed)

public:
    explicit DeviceManagerViewModel(DeviceState* state, QObject* parent = nullptr);
    bool supported() const;
    bool pairingMode() const;
    bool sourceSwitchSupported() const;
    QVariantList devices() const;

    Q_INVOKABLE bool setPairingMode(bool enabled);
    Q_INVOKABLE bool switchDevice(const QString& mac);
    Q_INVOKABLE bool connectDevice(const QString& mac);
    Q_INVOKABLE bool disconnectDevice(const QString& mac);
    Q_INVOKABLE bool unpairDevice(const QString& mac);
    void refresh();

signals:
    void changed();

private:
    DeviceState* m_state;
};

class ConnectionViewModel final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString state READ state WRITE setState NOTIFY changed)
    Q_PROPERTY(QString errorCode READ errorCode NOTIFY changed)
    Q_PROPERTY(QString errorDetail READ errorDetail NOTIFY changed)
    Q_PROPERTY(QVariantList devices READ devices NOTIFY changed)
    Q_PROPERTY(QString selectedDeviceName READ selectedDeviceName NOTIFY changed)
    Q_PROPERTY(QString selectedDeviceImage READ selectedDeviceImage NOTIFY changed)

public:
    explicit ConnectionViewModel(QObject* parent = nullptr);
    QString state() const { return m_state; }
    QString errorCode() const { return m_errorCode; }
    QString errorDetail() const { return m_errorDetail; }
    QVariantList devices() const;
    QString selectedDeviceName() const { return m_selectedDeviceName; }
    QString selectedDeviceImage() const { return m_selectedDeviceImage; }

    void setState(const QString& state);
    void setError(const QString& code, const QString& detail = {});
    void clearError();
    void setDevices(const QVariantList& devices);
    void setSelectedDevice(const QString& name, const QString& image);
    Q_INVOKABLE void refresh();

signals:
    void changed();

private:
    QString m_state = QStringLiteral("disconnected");
    QString m_errorCode;
    QString m_errorDetail;
    QVariantList m_devices;
    QString m_selectedDeviceName;
    QString m_selectedDeviceImage;
};
