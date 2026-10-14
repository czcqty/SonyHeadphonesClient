#pragma once

#include <QObject>
#include <QColor>
#include <QFont>
#include <QPointer>

class QWindow;

// Preferences and Qt system information only. Rendering belongs to Qt styles.
class PlatformAppearance final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString desktop READ desktop NOTIFY changed)
    Q_PROPERTY(QString themeMode READ themeMode WRITE setThemeMode NOTIFY changed)
    Q_PROPERTY(bool dark READ dark NOTIFY changed)
    Q_PROPERTY(QFont systemFont READ systemFont NOTIFY changed)
public:
    explicit PlatformAppearance(bool preview, QObject* parent = nullptr);
    QString desktop() const { return m_desktop; }
    QString themeMode() const { return m_mode; }
    bool dark() const { return m_mode == "dark" || (m_mode == "system" && m_systemDark); }
    QFont systemFont() const;
    void setThemeMode(const QString& mode);
    Q_INVOKABLE bool setPreviewDesktop(const QString& desktop);
    void attachWindow(QWindow* window);
    static QString detectDesktop(const QString& environment);
signals:
    void changed();
private:
    bool eventFilter(QObject*, QEvent*) override;
    void applyTheme();
    bool m_preview;
    QString m_desktop;
    QString m_mode = QStringLiteral("system");
    bool m_systemDark = false;
    QPointer<QWindow> m_window;
};
