#pragma once

#include <QJsonObject>
#include <QObject>
#include <QStringList>
#include <QVariantMap>

#include "device.h"
#include "r1.h"

// Everything the window and the command line talk to: the profile on disk,
// the mouse over USB, and the state in between.
class Backend : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap settings READ settings NOTIFY settingsChanged)
    Q_PROPERTY(QStringList profileNames READ profileNames NOTIFY profilesChanged)
    Q_PROPERTY(QString activeProfile READ activeProfile NOTIFY profilesChanged)
    Q_PROPERTY(QString deviceStatus READ deviceStatus NOTIFY deviceChanged)
    Q_PROPERTY(bool connected READ connected NOTIFY deviceChanged)
    Q_PROPERTY(int battery READ battery NOTIFY batteryChanged)
    Q_PROPERTY(QString batteryState READ batteryState NOTIFY batteryChanged)
    Q_PROPERTY(bool dirty READ dirty NOTIFY settingsChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString message READ message NOTIFY messageChanged)
    Q_PROPERTY(bool ok READ ok NOTIFY messageChanged)

public:
    explicit Backend(QObject *parent = nullptr);

    QVariantMap settings() const;
    QStringList profileNames() const { return m_profiles.keys(); }
    QString activeProfile() const { return m_active; }
    QString deviceStatus() const;
    bool connected() const { return m_device.state() == Device::State::Open; }
    int battery() const { return m_battery; }
    QString batteryState() const { return m_batteryState; }
    bool dirty() const { return m_profile != m_applied; }
    bool busy() const { return m_busy; }
    QString message() const { return m_message; }
    bool ok() const { return m_ok; }

    Q_INVOKABLE void setValue(const QString &key, const QVariant &value);
    Q_INVOKABLE void apply();
    Q_INVOKABLE void refreshBattery();
    Q_INVOKABLE void resetDefaults();
    Q_INVOKABLE void addProfile(const QString &name);
    Q_INVOKABLE void deleteProfile(const QString &name);
    Q_INVOKABLE void selectProfile(const QString &name);
    Q_INVOKABLE void renameProfile(const QString &name);
    Q_INVOKABLE void probeDevice();

    // Agent-facing commands: no display, JSON out, exit 0 or 1.
    void runCli(const QStringList &arguments);
    void save();

signals:
    void settingsChanged();
    void profilesChanged();
    void deviceChanged();
    void batteryChanged();
    void busyChanged();
    void messageChanged();
    void cliFinished(int code);

private:
    static QString configPath();
    void load();
    void notify(const QString &text, bool ok);
    r1::Profile fromSettings() const;
    void publish(const r1::Profile &profile);
    void publishAll();

    Device m_device;
    QHash<QString, r1::Profile> m_profiles;
    QString m_active = QStringLiteral("Default");
    r1::Profile m_profile;
    r1::Profile m_applied;
    int m_battery = -1;
    QString m_batteryState;
    bool m_busy = false;
    bool m_loaded = false;
    QString m_message;
    bool m_ok = true;
};
