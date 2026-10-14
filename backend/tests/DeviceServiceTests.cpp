#include <QtTest>

#include <QSettings>
#include <QTemporaryDir>

#include "../DeviceService.hpp"
#include "../DeviceProfileRepository.hpp"

class DeviceServiceTests final : public QObject
{
    Q_OBJECT

private slots:
    void repositoryContainsDocumentedProfiles()
    {
        DeviceProfileRepository repository;
        const QStringList ids = repository.profileIds();
        QCOMPARE(ids.size(), 12);
        for (const QString& id : {
                 QStringLiteral("WF-1000XM5"),
                 QStringLiteral("WF-1000XM6"),
                 QStringLiteral("WF-C510"),
                 QStringLiteral("WF-L910"),
                 QStringLiteral("WF-LS900N"),
                 QStringLiteral("WH-1000XM5"),
                 QStringLiteral("WH-1000XM6"),
                 QStringLiteral("WH-CH720N")})
        {
            QVERIFY2(ids.contains(id), qPrintable(id));
        }
    }

    void unsupportedFeatureDoesNotChangeDesiredState()
    {
        DeviceService controller(true);
        QVERIFY(controller.applyProfile(QStringLiteral("WF-L910")));
        QVERIFY(!controller.noiseControl()->supported());
        QCOMPARE(controller.noiseControl()->mode(), 0);

        QVERIFY(!controller.noiseControl()->setMode(1));
        QCOMPARE(controller.noiseControl()->mode(), 0);
        QCOMPARE(controller.rejectedWriteCount(), 1);
    }

    void supportedFeatureUpdatesDesiredState()
    {
        DeviceService controller(true);
        QVERIFY(controller.applyProfile(QStringLiteral("WH-1000XM6")));
        QVERIFY(controller.noiseControl()->setMode(2));
        QCOMPARE(controller.noiseControl()->mode(), 2);
        QVERIFY(controller.noiseControl()->setAmbientLevel(17));
        QCOMPARE(controller.noiseControl()->ambientLevel(), 17);
        QCOMPARE(controller.rejectedWriteCount(), 0);
    }

    void equalizerWritesAreCapabilityGated()
    {
        DeviceService controller(true);
        QVERIFY(controller.applyProfile(QStringLiteral("WH-1000XM6")));
        QVERIFY(controller.equalizer()->setBand(2, 7));
        QCOMPARE(controller.equalizer()->bands().at(2).toInt(), 7);
        QCOMPARE(controller.equalizer()->presetId(), QStringLiteral("custom"));
        QVERIFY(controller.equalizer()->selectPreset(QStringLiteral("flat")));
        QCOMPARE(controller.equalizer()->presetId(), QStringLiteral("flat"));
        QVERIFY(!controller.equalizer()->setBand(99, 3));
        QCOMPARE(controller.rejectedWriteCount(), 1);
    }

    void settingsWritesAreMappedToDesiredState()
    {
        DeviceService controller(true);
        QVERIFY(controller.applyProfile(QStringLiteral("WH-1000XM6")));
        QVERIFY(controller.settings()->setDseeEnabled(false));
        QVERIFY(!controller.settings()->dseeEnabled());
        QVERIFY(controller.settings()->setAutoPowerOffEnabled(false));
        QVERIFY(!controller.settings()->autoPowerOffEnabled());
        QVERIFY(controller.settings()->setSoundQualityPrior(false));
        QVERIFY(!controller.settings()->soundQualityPrior());
        QCOMPARE(controller.rejectedWriteCount(), 0);
    }

    void powerOffMapsDesiredState()
    {
        DeviceService controller(true);
        QVERIFY(controller.applyProfile(QStringLiteral("generic-v1")));
        QVERIFY(controller.playback()->powerOff());
        QVERIFY(controller.playback()->powerOffPending());
        QCOMPARE(controller.rejectedWriteCount(), 0);
    }

    void playbackWritesAreCapabilityGated()
    {
        DeviceService controller(true);
        QVERIFY(controller.applyProfile(QStringLiteral("WH-1000XM6")));
        QVERIFY(controller.playback()->playbackSupported());
        QVERIFY(controller.playback()->setVolume(31));
        QCOMPARE(controller.playback()->volume(), 30);
        QVERIFY(controller.playback()->nextTrack());

        DeviceState unsupported;
        PlaybackViewModel playback(&unsupported);
        QVERIFY(!playback.setVolume(20));
        QVERIFY(!playback.togglePlayback());
        QCOMPARE(unsupported.rejectedWrites, 2);
    }

    void p1SettingsMapToProtocolProperties()
    {
        DeviceService controller(true);
        QVERIFY(controller.applyProfile(QStringLiteral("WH-1000XM6")));
        QVERIFY(controller.noiseControl()->setAdaptiveEnabled(true));
        QVERIFY(controller.noiseControl()->setAdaptiveSensitivity(1));
        QVERIFY(controller.settings()->setAdaptiveVolumeEnabled(true));
        QVERIFY(controller.settings()->setSafeListeningEnabled(false));
        QVERIFY(!controller.settings()->safeListeningEnabled());
        QVERIFY(controller.settings()->setCaptureVoiceEnabled(false));
        QVERIFY(controller.settings()->setSpeakToChatSensitivity(2));
        QVERIFY(controller.settings()->setSpeakToChatModeOut(3));
        QVERIFY(controller.settings()->setAutoPowerOffValue(0x03));
        QVERIFY(controller.settings()->setCinemaModeEnabled(true));
        QVERIFY(!controller.settings()->listeningModeEnabled());
        QVERIFY(controller.settings()->setListeningModeEnabled(true));
        QVERIFY(!controller.settings()->cinemaModeEnabled());
        QCOMPARE(controller.rejectedWriteCount(), 0);
    }

    void fullEqPresetSelectionUsesDevicePresetId()
    {
        DeviceService controller(true);
        QVERIFY(controller.applyProfile(QStringLiteral("WH-1000XM6")));
        QVERIFY(controller.equalizer()->selectPreset(QStringLiteral("rock")));
        QCOMPARE(controller.equalizer()->presetId(), QStringLiteral("rock"));
        QVERIFY(!controller.equalizer()->selectPreset(QStringLiteral("not-a-preset")));
        QCOMPARE(controller.rejectedWriteCount(), 1);
    }

    void multipointUsesReportedDevicesAndValidatesMac()
    {
        DeviceService controller(true);
        QVERIFY(controller.applyProfile(QStringLiteral("WH-1000XM6")));
        QCOMPARE(controller.deviceManager()->devices().size(), 2);
        const auto first = controller.deviceManager()->devices().first().toMap();
        QCOMPARE(first.value(QStringLiteral("name")).toString(), QStringLiteral("Work laptop"));
        QVERIFY(controller.deviceManager()->disconnectDevice(first.value(QStringLiteral("mac")).toString()));
        QVERIFY(!controller.deviceManager()->unpairDevice(QStringLiteral("invalid")));
        QCOMPARE(controller.rejectedWriteCount(), 1);
    }
};

QTEST_GUILESS_MAIN(DeviceServiceTests)

#include "DeviceServiceTests.moc"
