#include <QtTest>
#include <QGuiApplication>
#include <QSettings>
#include <QStyleHints>
#include <QTemporaryDir>
#include "../quick/PlatformAppearance.hpp"

class AppearanceTests final : public QObject
{
    Q_OBJECT
    QTemporaryDir directory;
private slots:
    void initTestCase()
    {
        QVERIFY(directory.isValid());
        QCoreApplication::setOrganizationName("appearance-tests");
        QCoreApplication::setApplicationName("isolated");
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, directory.path());
    }
    void init() { QSettings().clear(); }
    void migratesExistingPreference()
    {
        for (bool dark : {false, true}) {
            QSettings().clear();
            QSettings().setValue("appearance/darkMode", dark);
            PlatformAppearance appearance(false);
            QCOMPARE(appearance.themeMode(), dark ? "dark" : "light");
            QCOMPARE(appearance.dark(), dark);
            QCOMPARE(QSettings().value("appearance/themeMode").toString(), appearance.themeMode());
            appearance.setThemeMode("system");
            QVERIFY(!QSettings().contains("appearance/darkMode"));
            PlatformAppearance restored(false);
            QCOMPARE(restored.themeMode(), "system");
        }
    }
    void previewNeverReadsOrWritesPreferences()
    {
        QSettings().setValue("appearance/themeMode", "dark");
        PlatformAppearance preview(true);
        QCOMPARE(preview.themeMode(), "system");
        preview.setThemeMode("light");
        for (const QString& desktop : {"windows", "macos", "kde", "gnome", "xfce", "linux"}) {
            QVERIFY(preview.setPreviewDesktop(desktop));
            QCOMPARE(preview.desktop(), desktop);
        }
        QVERIFY(!preview.setPreviewDesktop("invalid"));
        QCOMPARE(preview.desktop(), "linux");
        QCOMPARE(QSettings().value("appearance/themeMode").toString(), "dark");
        QVERIFY(!QSettings().contains("appearance/darkMode"));
        PlatformAppearance live(false);
        QVERIFY(!live.setPreviewDesktop("macos"));
    }
    void themeFollowsSystemOnlyWhenRequested()
    {
        PlatformAppearance preview(true);
        auto notify = [](Qt::ColorScheme scheme) {
            return QMetaObject::invokeMethod(QGuiApplication::styleHints(), "colorSchemeChanged",
                Qt::DirectConnection, Q_ARG(Qt::ColorScheme, scheme));
        };
        QVERIFY(notify(Qt::ColorScheme::Dark));
        QVERIFY(preview.dark());
        preview.setThemeMode("light");
        QVERIFY(notify(Qt::ColorScheme::Dark));
        QVERIFY(!preview.dark());
        preview.setThemeMode("system");
        QVERIFY(preview.dark());
        QVERIFY(notify(Qt::ColorScheme::Light));
        QVERIFY(!preview.dark());
        preview.setThemeMode("invalid");
        QCOMPARE(preview.themeMode(), "system");
    }
    void detectsDesktopSessions()
    {
        QCOMPARE(PlatformAppearance::detectDesktop("ubuntu:GNOME"), "gnome");
        QCOMPARE(PlatformAppearance::detectDesktop("KDE"), "kde");
        QCOMPARE(PlatformAppearance::detectDesktop("plasma"), "kde");
        QCOMPARE(PlatformAppearance::detectDesktop("XFCE"), "xfce");
        QCOMPARE(PlatformAppearance::detectDesktop("unknown"), "linux");
    }
};
QTEST_MAIN(AppearanceTests)
#include "AppearanceTests.moc"
