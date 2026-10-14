#pragma once

#include <QObject>
#include <QStringList>
#include <memory>

#include "../../backend/DeviceService.hpp"
#include "PlatformAppearance.hpp"

class MdrSession;

class AppController final : public DeviceService
{
    Q_OBJECT
    Q_PROPERTY(QString currentRoute READ currentRoute WRITE setCurrentRoute NOTIFY currentRouteChanged)
    Q_PROPERTY(QString locale READ locale WRITE setLocale NOTIFY localeChanged)
    Q_PROPERTY(QString previewState READ previewState WRITE setPreviewState NOTIFY previewStateChanged)
    Q_PROPERTY(QString desktopStyle READ desktopStyle NOTIFY appearanceChanged)
    Q_PROPERTY(PlatformAppearance* appearance READ appearance CONSTANT)
    Q_PROPERTY(bool darkMode READ darkMode NOTIFY darkModeChanged)

public:
    explicit AppController(bool previewMode = false, QObject* parent = nullptr);
    ~AppController() override;

    QString currentRoute() const { return m_currentRoute; }
    QString locale() const { return m_locale; }
    QString previewState() const { return m_previewState; }
    QString desktopStyle() const;
    bool darkMode() const { return m_appearance.dark(); }
    PlatformAppearance* appearance() { return &m_appearance; }
    Q_INVOKABLE void goBack();
    Q_INVOKABLE void toggleDarkMode();
    Q_INVOKABLE void toggleLocale();
    Q_INVOKABLE void completeConnectionTransition();

    void setCurrentRoute(const QString& route);
    void setLocale(const QString& locale);
    void setPreviewState(const QString& state);
    void setPreviewTheme(const QString& theme);

signals:
    void appearanceChanged();
    void currentRouteChanged();
    void localeChanged();
    void previewStateChanged();
    void darkModeChanged();

private:
    void replaceRoute(const QString& route, bool clearHistory = false);

private:
    QString m_currentRoute = QStringLiteral("home");
    QString m_locale = QStringLiteral("en");
    QString m_previewState = QStringLiteral("ready");
    PlatformAppearance m_appearance;
    QStringList m_routeHistory;
};
