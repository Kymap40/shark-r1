// The Attack Shark R1 protocol. Layouts follow a working Linux driver
// (xb-bx/attack-shark-r1-driver, which captured them from the vendor software)
// and were re-checked against this mouse over USB.

#include "r1.h"

#include <QJsonArray>

namespace r1 {
namespace {

// Report 4 as the vendor software sends it. Only the fields we understand are
// rewritten; every other byte is passed through untouched so a write never
// disturbs settings the app does not model.
const quint8 kDpiTemplate[56] = {
    0x04, 0x38, 0x01, 0x00, 0x00, 0x3f, 0x00, 0x00,
    0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x01, 0xff, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00,
    0x00, 0xff, 0xff, 0xff, 0x00, 0x00, 0xff, 0xff,
    0xff, 0x00, 0xff, 0xff, 0x40, 0x00, 0xff, 0xff,
    0xff, 0x02, 0x0d, 0x75, 0x00, 0x00, 0x00, 0x00,
};

constexpr int kDpiChecksumSeed = 0x0d75;

int clampDpi(int dpi) {
    if (dpi < kMinDpi)
        dpi = kMinDpi;
    if (dpi > kMaxDpi)
        dpi = kMaxDpi;
    return (dpi / kDpiStep) * kDpiStep;
}

}  // namespace

int dpiByte(int dpi) {
    dpi = clampDpi(dpi);
    static const int first[6] = {2, 4, 6, 9, 11, 14};
    int index = dpi / 100 - 1;
    if (index < 6)
        return first[index];
    index -= 6;
    return (14 + (index + 1) / 3 + (index + 1) * 2) & 0xff;
}

QByteArray dpiReport(const Profile &profile) {
    QByteArray report(reinterpret_cast<const char *>(kDpiTemplate), sizeof(kDpiTemplate));

    report[3] = profile.angleSnap ? 1 : 0;
    report[4] = profile.ripple ? 1 : 0;

    quint8 big = 0;
    int checksum = kDpiChecksumSeed;
    for (int i = 0; i < kStages; ++i) {
        const int dpi = i < profile.dpi.size() ? clampDpi(profile.dpi.at(i)) : 800;
        const quint8 x = quint8(dpiByte(dpi));
        const quint8 y = (dpi >= 10100 && dpi <= 12000) ? 1 : 0;
        report[8 + i] = char(x);
        report[16 + i] = char(y);
        checksum += x + y;
        if (dpi > 12000)
            big |= quint8(1 << i);
    }
    report[6] = char(big);
    report[7] = char(big);
    checksum += big * 2;

    const int active = qBound(1, profile.activeStage, kStages);
    report[24] = char(active);
    checksum += active - 1;
    if (profile.ripple)
        ++checksum;
    if (profile.angleSnap)
        ++checksum;

    report[50] = char((checksum >> 8) & 0xff);
    report[51] = char(checksum & 0xff);
    return report;
}

QByteArray timingReport(const Profile &profile) {
    QByteArray report(15, '\0');
    report[0] = 0x05;
    report[1] = 0x0f;
    report[2] = 0x01;

    const int deep = qBound(kMinDeepSleepMin, profile.deepSleepMin, kMaxDeepSleepMin);
    const int sleep = qBound(int(kMinSleepMin * 2), int(profile.sleepMin * 2 + 0.5), int(kMaxSleepMin * 2));
    const int debounce = qBound(kMinDebounceMs, profile.debounceMs, kMaxDebounceMs) / 2;

    report[3] = char((profile.lightMode & 0x0f) << 4);
    report[4] = char((profile.lightSpeed & 0x0f) | (deep & 0xf0));
    report[5] = char((profile.lightBrightness & 0x0f) | ((deep & 0x0f) << 4));
    report[6] = char(profile.lightColor.red());
    report[7] = char(profile.lightColor.green());
    report[8] = char(profile.lightColor.blue());
    report[9] = char(sleep);
    report[10] = char(debounce);
    report[11] = 0x01;

    const int checksum = ((((deep & 0x0f) + ((deep & 0xf0) >> 4)) << 4) + 0x0a + sleep + debounce) & 0xff;
    report[12] = char(checksum);
    return report;
}

QByteArray pollingReport(int hz) {
    QByteArray report(9, '\0');
    report[0] = 0x06;
    report[1] = 0x09;
    report[2] = 0x01;
    for (const auto &rate : kPollRates) {
        if (rate.hz == hz) {
            report[3] = char(rate.low);
            report[4] = char(rate.high);
            break;
        }
    }
    return report;
}

int batteryPercent(const QByteArray &report, QString *state) {
    if (report.size() < 5 || quint8(report[0]) != 0x03)
        return -1;
    const quint8 kind = quint8(report[2]);
    if (kind != 0x40 && kind != 0x41)
        return -1;
    if (state) {
        switch (quint8(report[3])) {
        case 1: *state = QStringLiteral("discharging"); break;
        case 2: *state = QStringLiteral("full"); break;
        case 3: *state = QStringLiteral("charging"); break;
        default: *state = QStringLiteral("unknown"); break;
        }
    }
    return qMin(100, quint8(report[4]) * 10);
}

// ------------------------------------------------------------------ profile

QJsonObject Profile::toJson() const {
    QJsonArray stages;
    for (int dpi : this->dpi)
        stages.append(dpi);
    QJsonObject json;
    json["name"] = name;
    json["dpi"] = stages;
    json["activeStage"] = activeStage;
    json["ripple"] = ripple;
    json["angleSnap"] = angleSnap;
    json["pollingHz"] = pollingHz;
    json["sleepMin"] = sleepMin;
    json["deepSleepMin"] = deepSleepMin;
    json["debounceMs"] = debounceMs;
    json["lightOn"] = lightOn;
    json["lightMode"] = lightMode;
    json["lightColor"] = lightColor.name();
    json["lightBrightness"] = lightBrightness;
    json["lightSpeed"] = lightSpeed;
    return json;
}

Profile Profile::fromJson(const QJsonObject &json) {
    Profile profile;
    profile.name = json.value("name").toString(profile.name);
    const auto stages = json.value("dpi").toArray();
    for (int i = 0; i < kStages && i < stages.size(); ++i)
        profile.dpi[i] = qBound(kMinDpi, int(stages.at(i).toDouble(profile.dpi.at(i))), kMaxDpi) / kDpiStep * kDpiStep;
    profile.activeStage = qBound(1, json.value("activeStage").toInt(profile.activeStage), kStages);
    profile.ripple = json.value("ripple").toBool(profile.ripple);
    profile.angleSnap = json.value("angleSnap").toBool(profile.angleSnap);
    profile.pollingHz = json.value("pollingHz").toInt(profile.pollingHz);
    profile.sleepMin = qBound(kMinSleepMin, json.value("sleepMin").toDouble(profile.sleepMin), kMaxSleepMin);
    profile.deepSleepMin = qBound(kMinDeepSleepMin, json.value("deepSleepMin").toInt(profile.deepSleepMin), kMaxDeepSleepMin);
    profile.debounceMs = qBound(kMinDebounceMs, json.value("debounceMs").toInt(profile.debounceMs), kMaxDebounceMs) / 2 * 2;
    profile.lightOn = json.value("lightOn").toBool(profile.lightOn);
    profile.lightMode = qBound(0, json.value("lightMode").toInt(profile.lightMode), 15);
    const QColor color(json.value("lightColor").toString(profile.lightColor.name()));
    if (color.isValid())
        profile.lightColor = color;
    profile.lightBrightness = qBound(1, json.value("lightBrightness").toInt(profile.lightBrightness), 8);
    profile.lightSpeed = qBound(1, json.value("lightSpeed").toInt(profile.lightSpeed), 5);
    return profile;
}

bool Profile::operator==(const Profile &other) const {
    return toJson() == other.toJson();
}

}  // namespace r1
