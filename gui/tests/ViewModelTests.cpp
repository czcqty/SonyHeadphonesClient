#include <QtTest>

#include <QSettings>
#include <QTemporaryDir>

#include "../quick/AppController.hpp"
#include "../quick/DeviceProfileRepository.hpp"

class ViewModelTests final : public QObject
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

    void previewArgumentsCanBeReproduced()
    {
        AppController controller(true);
        QVERIFY(controller.applyProfile(QStringLiteral("WF-1000XM5")));
        controller.setCurrentRoute(QStringLiteral("equalizer"));
        controller.setPreviewState(QStringLiteral("loading"));
        controller.setLocale(QStringLiteral("zh_CN"));

        QCOMPARE(controller.currentProfile(), QStringLiteral("WF-1000XM5"));
        QCOMPARE(controller.currentRoute(), QStringLiteral("equalizer"));
        QCOMPARE(controller.previewState(), QStringLiteral("loading"));
        QCOMPARE(controller.locale(), QStringLiteral("zh_CN"));
    }

    void localeToggleUsesStableLocaleIds()
    {
        AppController controller(true);
        QCOMPARE(controller.locale(), QStringLiteral("en"));
        controller.toggleLocale();
        QCOMPARE(controller.locale(), QStringLiteral("zh_CN"));
        controller.toggleLocale();
        QCOMPARE(controller.locale(), QStringLiteral("en"));
        controller.setLocale(QStringLiteral("zh-Hans"));
        QCOMPARE(controller.locale(), QStringLiteral("zh_CN"));
    }

    void localePreferenceIsPersistedAndCanBeOverridden()
    {
        QTemporaryDir settingsDirectory;
        QVERIFY(settingsDirectory.isValid());
        QCoreApplication::setOrganizationName(QStringLiteral("czcqty-tests"));
        QCoreApplication::setApplicationName(QStringLiteral("SonyHeadphonesClient-tests"));
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope,
                           settingsDirectory.path());

        {
            AppController controller(false);
            controller.setLocale(QStringLiteral("zh_CN"));
        }
        QSettings().sync();

        AppController restored(false);
        QCOMPARE(restored.locale(), QStringLiteral("zh_CN"));
        restored.setLocale(QStringLiteral("en"));
        QCOMPARE(restored.locale(), QStringLiteral("en"));
        QSettings().sync();
        QCOMPARE(QSettings().value(QStringLiteral("appearance/locale")).toString(),
                 QStringLiteral("en"));
    }

    void invalidRouteFallsBackToHome()
    {
        AppController controller(true);
        controller.setCurrentRoute(QStringLiteral("account"));
        QCOMPARE(controller.currentRoute(), QStringLiteral("home"));
    }

    void removedPlaybackRouteFallsBackToHome()
    {
        AppController controller(true);
        controller.setCurrentRoute(QStringLiteral("settings"));
        controller.setCurrentRoute(QStringLiteral("playback"));
        QCOMPARE(controller.currentRoute(), QStringLiteral("home"));
    }

    void routeHistoryReturnsToCallingPage()
    {
        AppController controller(true);
        controller.setCurrentRoute(QStringLiteral("settings"));
        controller.setCurrentRoute(QStringLiteral("touch"));
        controller.goBack();
        QCOMPARE(controller.currentRoute(), QStringLiteral("settings"));
        controller.goBack();
        QCOMPARE(controller.currentRoute(), QStringLiteral("home"));
    }

    void liveProfileRefreshDoesNotChangeCurrentRoute()
    {
        AppController controller(true);
        controller.setCurrentRoute(QStringLiteral("equalizer"));
        QVERIFY(QMetaObject::invokeMethod(
            &controller, "selectLiveProfile", Qt::DirectConnection,
            Q_ARG(QString, QStringLiteral("WH-1000XM5"))));
        QCOMPARE(controller.currentProfile(), QStringLiteral("WH-1000XM5"));
        QCOMPARE(controller.currentRoute(), QStringLiteral("equalizer"));
    }

    void connectionLossReturnsToConnectionPage()
    {
        AppController controller(true);
        controller.setCurrentRoute(QStringLiteral("equalizer"));
        QVERIFY(QMetaObject::invokeMethod(
            &controller, "handleConnectionAbandoned", Qt::DirectConnection,
            Q_ARG(QString, QStringLiteral("connection-failed")),
            Q_ARG(QString, QStringLiteral("Connection lost"))));
        QCOMPARE(controller.currentRoute(), QStringLiteral("connection"));
        QCOMPARE(controller.connection()->errorCode(), QStringLiteral("connection-failed"));
        QCOMPARE(controller.connection()->errorDetail(), QStringLiteral("Connection lost"));
    }

    void previewThemeCanBeChangedWithoutSettingsDependency()
    {
        AppController controller(true);
        controller.setPreviewTheme(QStringLiteral("dark"));
        QVERIFY(controller.darkMode());
        controller.setPreviewTheme(QStringLiteral("light"));
        QVERIFY(!controller.darkMode());
        controller.toggleDarkMode();
        QVERIFY(controller.darkMode());
    }

    void unsupportedFeatureDoesNotChangeDesiredState()
    {
        AppController controller(true);
        QVERIFY(controller.applyProfile(QStringLiteral("WF-L910")));
        QVERIFY(!controller.noiseControl()->supported());
        QCOMPARE(controller.noiseControl()->mode(), 0);

        QVERIFY(!controller.noiseControl()->setMode(1));
        QCOMPARE(controller.noiseControl()->mode(), 0);
        QCOMPARE(controller.rejectedWriteCount(), 1);
    }

    void supportedFeatureUpdatesDesiredState()
    {
        AppController controller(true);
        QVERIFY(controller.applyProfile(QStringLiteral("WH-1000XM6")));
        QVERIFY(controller.noiseControl()->setMode(2));
        QCOMPARE(controller.noiseControl()->mode(), 2);
        QVERIFY(controller.noiseControl()->setAmbientLevel(17));
        QCOMPARE(controller.noiseControl()->ambientLevel(), 17);
        QCOMPARE(controller.rejectedWriteCount(), 0);
    }

    void equalizerWritesAreCapabilityGated()
    {
        AppController controller(true);
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
        AppController controller(true);
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
        AppController controller(true);
        QVERIFY(controller.applyProfile(QStringLiteral("generic-v1")));
        QVERIFY(controller.playback()->powerOff());
        QCOMPARE(controller.rejectedWriteCount(), 0);
    }

    void playbackWritesAreCapabilityGated()
    {
        AppController controller(true);
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
        AppController controller(true);
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
        AppController controller(true);
        QVERIFY(controller.applyProfile(QStringLiteral("WH-1000XM6")));
        QVERIFY(controller.equalizer()->selectPreset(QStringLiteral("rock")));
        QCOMPARE(controller.equalizer()->presetId(), QStringLiteral("rock"));
        QVERIFY(!controller.equalizer()->selectPreset(QStringLiteral("not-a-preset")));
        QCOMPARE(controller.rejectedWriteCount(), 1);
    }

    void multipointUsesReportedDevicesAndValidatesMac()
    {
        AppController controller(true);
        QVERIFY(controller.applyProfile(QStringLiteral("WH-1000XM6")));
        QCOMPARE(controller.deviceManager()->devices().size(), 2);
        const auto first = controller.deviceManager()->devices().first().toMap();
        QCOMPARE(first.value(QStringLiteral("name")).toString(), QStringLiteral("Work laptop"));
        QVERIFY(controller.deviceManager()->disconnectDevice(first.value(QStringLiteral("mac")).toString()));
        QVERIFY(!controller.deviceManager()->unpairDevice(QStringLiteral("invalid")));
        QCOMPARE(controller.rejectedWriteCount(), 1);
    }
};

QTEST_APPLESS_MAIN(ViewModelTests)

#include "ViewModelTests.moc"
