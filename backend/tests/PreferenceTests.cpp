#include <QtTest>
#include <QTemporaryDir>
#include <QJsonArray>
#include "ApplicationPreferences.hpp"
#include "BackendProtocol.hpp"

class PreferenceTests final : public QObject {
    Q_OBJECT
private slots:
    void migrationAndPersistence() {
        for (bool dark : {false, true}) {
            QTemporaryDir directory;
            QSettings settings(directory.filePath("preferences.ini"), QSettings::IniFormat);
            settings.setValue("appearance/darkMode", dark);
            settings.setValue("appearance/locale", "zh-CN");
            settings.setValue("unrelated/device", "preserved");
            ApplicationPreferences prefs(false, &settings);
            QCOMPARE(prefs.snapshot()["themeMode"].toString(), dark ? "dark" : "light");
            QCOMPARE(prefs.snapshot()["locale"].toString(), "zh_CN");
            QVERIFY(prefs.set("themeMode", "system"));
            QVERIFY(prefs.set("locale", "en"));
            ApplicationPreferences reloaded(false, &settings);
            QCOMPARE(reloaded.snapshot(), prefs.snapshot());
            QVERIFY(!settings.contains("appearance/darkMode"));
            QCOMPARE(settings.value("unrelated/device").toString(), "preserved");
            QVERIFY(!prefs.set("themeMode", "invalid"));
            QCOMPARE(prefs.snapshot(), reloaded.snapshot());
        }
    }
    void precedenceAndPreviewIsolation() {
        QTemporaryDir directory;
        QSettings settings(directory.filePath("preferences.ini"), QSettings::IniFormat);
        ApplicationPreferences fresh(false, &settings);
        QCOMPARE(fresh.snapshot()["themeMode"].toString(), "system");
        settings.setValue("appearance/darkMode", true);
        settings.setValue("appearance/themeMode", "light");
        settings.setValue("appearance/locale", "zh_CN");
        ApplicationPreferences live(false, &settings);
        QCOMPARE(live.snapshot()["themeMode"].toString(), "light");
        ApplicationPreferences preview(true, &settings);
        QCOMPARE(preview.snapshot()["themeMode"].toString(), "system");
        QCOMPARE(preview.snapshot()["locale"].toString(), "en");
        QVERIFY(preview.set("themeMode", "dark"));
        QVERIFY(preview.set("locale", "en"));
        QCOMPARE(settings.value("appearance/themeMode").toString(), "light");
        QCOMPARE(settings.value("appearance/locale").toString(), "zh_CN");
        QVERIFY(live.set("themeMode", "dark", false));
        QCOMPARE(settings.value("appearance/themeMode").toString(), "light");
    }
    void preferencesWorkWhileDisconnectedWithoutDeviceWrites() {
        DeviceService service(true);
        service.disconnectDevice();
        BackendProtocol protocol(service);
        QSignalSpy writes(service.settings(), SIGNAL(changed()));
        auto result = protocol.execute({{"target", "preferences"}, {"method", "setLocale"}, {"args", QJsonArray{"zh_CN"}}});
        QVERIFY(result["ok"].toBool());
        QCOMPARE(protocol.snapshot()["preferences"].toObject()["locale"].toString(), "zh_CN");
        QCOMPARE(writes.size(), 0);
        QVERIFY(!protocol.execute({{"target", "preferences"}, {"method", "setLocale"}, {"args", QJsonArray{true}}})["ok"].toBool());
    }
};
QTEST_GUILESS_MAIN(PreferenceTests)
#include "PreferenceTests.moc"
