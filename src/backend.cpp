#include "backend.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTimer>

#include <cstdio>

namespace {

QByteArray jsonBytes(const QJsonObject &object) {
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

}  // namespace

Backend::Backend(QObject *parent) : QObject(parent) {
    load();
    m_applied = m_profile;
    QTimer::singleShot(0, this, &Backend::probeDevice);
}

QString Backend::configPath() {
    const QString base = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    return base + "/shark/profiles.json";
}

void Backend::load() {
    QFile file(configPath());
    QHash<QString, r1::Profile> profiles;
    QString active;
    if (file.open(QIODevice::ReadOnly)) {
        const auto document = QJsonDocument::fromJson(file.readAll());
        const auto root = document.object();
        const auto stored = root.value("profiles").toObject();
        for (auto it = stored.begin(); it != stored.end(); ++it)
            profiles.insert(it.key(), r1::Profile::fromJson(it.value().toObject()));
        active = root.value("active").toString();
    }
    if (profiles.isEmpty())
        profiles.insert(QStringLiteral("Default"), r1::Profile());
    if (!profiles.contains(active))
        active = profiles.keys().value(0);

    m_profiles = profiles;
    m_active = active;
    m_profile = m_profiles.value(active);
    m_loaded = true;
}

void Backend::save() {
    QDir().mkpath(QFileInfo(configPath()).absolutePath());
    QJsonObject stored;
    for (auto it = m_profiles.begin(); it != m_profiles.end(); ++it)
        stored.insert(it.key(), it.value().toJson());

    QJsonObject root;
    root["version"] = 1;
    root["active"] = m_active;
    root["profiles"] = stored;

    // Written beside the target and renamed, so a crash never leaves a
    // half-written profile behind.
    QSaveFile file(configPath());
    if (!file.open(QIODevice::WriteOnly)) {
        notify(tr("Cannot save profiles to %1").arg(configPath()), false);
        return;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    if (!file.commit())
        notify(tr("Cannot save profiles to %1").arg(configPath()), false);
}

QVariantMap Backend::settings() const {
    QVariantList stages;
    for (int dpi : m_profile.dpi)
        stages.append(dpi);
    QVariantMap map;
    map["dpi"] = stages;
    map["activeStage"] = m_profile.activeStage;
    map["pollingHz"] = m_profile.pollingHz;
    map["ripple"] = m_profile.ripple;
    map["angleSnap"] = m_profile.angleSnap;
    map["sleepMin"] = m_profile.sleepMin;
    map["deepSleepMin"] = m_profile.deepSleepMin;
    map["debounceMs"] = m_profile.debounceMs;
    return map;
}

QString Backend::deviceStatus() const {
    switch (m_device.state()) {
    case Device::State::Open:
        return m_device.isWired() ? tr("Connected over the cable") : tr("Connected over the 2.4 GHz dongle");
    case Device::State::NoMouse:
        return tr("No mouse: plug in the dongle or the cable");
    case Device::State::Error:
        return m_device.lastError();
    case Device::State::Closed:
        break;
    }
    return tr("Not connected");
}

void Backend::notify(const QString &text, bool ok) {
    m_message = text;
    m_ok = ok;
    emit messageChanged();
}

r1::Profile Backend::fromSettings() const {
    return m_profile;
}

void Backend::publish(const r1::Profile &profile) {
    m_profile = profile;
    m_profiles[m_active] = profile;
    emit settingsChanged();
}

void Backend::publishAll() {
    emit settingsChanged();
    emit profilesChanged();
}

void Backend::probeDevice() {
    if (!m_device.open()) {
        emit deviceChanged();
        return;
    }
    emit deviceChanged();
    // The level only arrives when the mouse is awake, so this may come back
    // empty; pressing Battery retries.
    QTimer::singleShot(300, this, &Backend::refreshBattery);
}

void Backend::setValue(const QString &key, const QVariant &value) {
    r1::Profile profile = m_profile;
    if (key.startsWith("dpi")) {
        // dpi0 .. dpi5 address the six stages directly.
        const int index = key.mid(3).toInt();
        if (index >= 0 && index < r1::kStages) {
            int dpi = qBound(r1::kMinDpi, value.toInt(), r1::kMaxDpi);
            dpi = dpi / r1::kDpiStep * r1::kDpiStep;
            while (profile.dpi.size() <= index)
                profile.dpi.append(800);
            profile.dpi[index] = dpi;
        }
    } else if (key == "activeStage") {
        profile.activeStage = qBound(1, value.toInt(), r1::kStages);
    } else if (key == "pollingHz") {
        profile.pollingHz = value.toInt();
    } else if (key == "ripple") {
        profile.ripple = value.toBool();
    } else if (key == "angleSnap") {
        profile.angleSnap = value.toBool();
    } else if (key == "sleepMin") {
        profile.sleepMin = qBound(r1::kMinSleepMin, value.toDouble(), r1::kMaxSleepMin);
    } else if (key == "deepSleepMin") {
        profile.deepSleepMin = qBound(r1::kMinDeepSleepMin, value.toInt(), r1::kMaxDeepSleepMin);
    } else if (key == "debounceMs") {
        const int ms = qBound(r1::kMinDebounceMs, value.toInt(), r1::kMaxDebounceMs);
        profile.debounceMs = ms / 2 * 2;
    } else {
        return;
    }
    publish(profile);
}

void Backend::apply() {
    if (m_busy)
        return;
    m_busy = true;
    emit busyChanged();

    if (!m_device.open()) {
        emit deviceChanged();
        notify(m_device.lastError(), false);
        m_busy = false;
        emit busyChanged();
        return;
    }
    emit deviceChanged();

    const r1::Profile profile = m_profile;
    struct Step {
        QString what;
        QByteArray report;
    };
    const QList<Step> steps{
        {tr("polling rate"), r1::pollingReport(profile.pollingHz)},
        {tr("timing and sleep"), r1::timingReport(profile)},
        {tr("DPI stages"), r1::dpiReport(profile)},
    };

    QString error;
    for (const auto &step : steps) {
        if (!m_device.send(step.report, &error)) {
            notify(tr("Could not apply the %1: %2").arg(step.what, error), false);
            m_busy = false;
            emit busyChanged();
            return;
        }
    }

    m_applied = profile;
    save();
    emit settingsChanged();
    notify(tr("Applied to the mouse"), true);
    m_busy = false;
    emit busyChanged();
    refreshBattery();
}

void Backend::refreshBattery() {
    QString state;
    const int percent = m_device.battery(&state);
    if (percent < 0) {
        if (m_battery != -1) {
            m_battery = -1;
            m_batteryState.clear();
            emit batteryChanged();
        }
        return;
    }
    if (percent == m_battery && state == m_batteryState)
        return;
    m_battery = percent;
    m_batteryState = state;
    emit batteryChanged();
}

void Backend::resetDefaults() {
    const QString name = m_profile.name;
    r1::Profile profile;
    profile.name = name;
    publish(profile);
    notify(tr("Back to the factory defaults -- press Apply to send them"), true);
}

void Backend::addProfile(const QString &name) {
    const QString key = name.trimmed();
    if (key.isEmpty() || m_profiles.contains(key)) {
        notify(tr("That profile name is already taken"), false);
        return;
    }
    r1::Profile profile = m_profile;
    profile.name = key;
    m_profiles.insert(key, profile);
    m_active = key;
    m_profile = profile;
    save();
    publishAll();
    notify(tr("Profile %1 added").arg(key), true);
}

void Backend::deleteProfile(const QString &name) {
    if (m_profiles.size() < 2) {
        notify(tr("At least one profile has to stay"), false);
        return;
    }
    if (!m_profiles.contains(name))
        return;
    m_profiles.remove(name);
    if (m_active == name)
        m_active = m_profiles.keys().value(0);
    m_profile = m_profiles.value(m_active);
    save();
    publishAll();
}

void Backend::selectProfile(const QString &name) {
    if (!m_profiles.contains(name) || name == m_active)
        return;
    m_profiles[m_active] = m_profile;
    m_active = name;
    m_profile = m_profiles.value(name);
    save();
    publishAll();
}

void Backend::renameProfile(const QString &name) {
    const QString key = name.trimmed();
    if (key.isEmpty() || key == m_active || m_profiles.contains(key)) {
        notify(tr("That profile name is already taken"), false);
        return;
    }
    r1::Profile profile = m_profile;
    profile.name = key;
    m_profiles.remove(m_active);
    m_profiles.insert(key, profile);
    m_active = key;
    m_profile = profile;
    m_applied.name = key;
    save();
    publishAll();
}

// ---------------------------------------------------------------------- CLI

void Backend::runCli(const QStringList &arguments) {
    int code = 0;
    const QString command = arguments.value(1);
    const bool json = arguments.contains("--json");

    auto say = [&](const QString &text) {
        const QByteArray bytes = text.toUtf8() + '\n';
        std::fwrite(bytes.constData(), 1, size_t(bytes.size()), stdout);
        std::fflush(stdout);
    };

    if (command.isEmpty() || command == "--help" || command == "help") {
        say(QStringLiteral(
            "shark -- configure the Attack Shark R1\n\n"
            "  shark status [--json]     the active profile, what differs, the mouse\n"
            "  shark apply [--json]      send the active profile to the mouse\n"
            "  shark battery [--json]    battery level, read from the mouse\n"
            "  shark defaults [--json]   restore the factory defaults in the profile\n"
            "  shark set <key> <value>   change one setting (dpi0..dpi5, activeStage,\n"
            "                            pollingHz, ripple, angleSnap, sleepMin,\n"
            "                            deepSleepMin, debounceMs)\n"
            "  shark profile <name>      switch the active profile\n\n"
            "Add --json for machine-readable output. The window is `shark open`."));
    } else if (command == "status") {
        m_profiles[m_active] = m_profile;
        QJsonObject out;
        out["profile"] = m_active;
        out["settings"] = m_profile.toJson();
        out["applied"] = !dirty();
        out["device"] = m_device.open() ? (m_device.isWired() ? "wired" : "dongle") : "absent";
        QJsonArray names;
        for (const auto &name : profileNames())
            names.append(name);
        out["profiles"] = names;
        out["config"] = configPath();
        say(json ? QString::fromUtf8(jsonBytes(out)) : QString::fromUtf8(jsonBytes(out)));
    } else if (command == "apply") {
        apply();
        QJsonObject out;
        out["ok"] = m_ok;
        out["message"] = m_message;
        out["device"] = m_device.isWired() ? "wired" : "dongle";
        say(QString::fromUtf8(jsonBytes(out)));
        code = m_ok ? 0 : 1;
    } else if (command == "battery") {
        if (!m_device.open()) {
            QJsonObject out;
            out["ok"] = false;
            out["message"] = m_device.lastError();
            say(QString::fromUtf8(jsonBytes(out)));
            code = 1;
        } else {
            QString state;
            const int percent = m_device.battery(&state);
            QJsonObject out;
            out["ok"] = percent >= 0;
            out["percent"] = percent;
            out["state"] = state;
            if (percent < 0)
                out["message"] = tr("The mouse did not report a level -- move it and try again");
            say(QString::fromUtf8(jsonBytes(out)));
            code = percent >= 0 ? 0 : 1;
        }
    } else if (command == "defaults") {
        resetDefaults();
        save();
        say(QString::fromUtf8(jsonBytes(m_profile.toJson())));
    } else if (command == "set") {
        if (arguments.size() < 4) {
            say(QStringLiteral("set needs a key and a value"));
            code = 1;
        } else {
            const QString key = arguments.at(2);
            const QString value = arguments.at(3);
            const bool isNumber = key.startsWith("dpi") || key == "activeStage" || key == "deepSleepMin" ||
                                  key == "debounceMs" || key == "pollingHz";
            if (key == "sleepMin")
                setValue(key, value.toDouble());
            else if (isNumber)
                setValue(key, value.toInt());
            else if (key == "ripple" || key == "angleSnap")
                setValue(key, value == "true" || value == "1" || value == "on");
            else {
                say(QStringLiteral("unknown setting: %1").arg(key));
                code = 1;
            }
            if (!code) {
                save();
                say(QString::fromUtf8(jsonBytes(m_profile.toJson())));
            }
        }
    } else if (command == "profile") {
        selectProfile(arguments.value(2));
        say(QString::fromUtf8(jsonBytes(m_profile.toJson())));
    } else {
        say(QStringLiteral("unknown command: %1 -- try `shark help`").arg(command));
        code = 1;
    }

    emit cliFinished(code);
}
