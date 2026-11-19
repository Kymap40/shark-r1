// The Attack Shark R1 protocol: three HID feature reports, no read-back.
//
// The mouse never reports its own settings, so everything it stores lives in a
// profile on disk. What it does report is the battery, as an unsolicited
// interrupt report, and a one-byte acknowledgement for every write.

#pragma once

#include <QByteArray>
#include <QColor>
#include <QJsonObject>
#include <QList>
#include <QString>

namespace r1 {

constexpr int kStages = 6;
constexpr int kMinDpi = 100;
constexpr int kMaxDpi = 18000;
constexpr int kDpiStep = 100;
constexpr int kMinDebounceMs = 4;
constexpr int kMaxDebounceMs = 50;
constexpr double kMinSleepMin = 0.5;
constexpr double kMaxSleepMin = 30.0;
constexpr int kMinDeepSleepMin = 1;
constexpr int kMaxDeepSleepMin = 60;

// The four polling rates the firmware accepts, as the little-endian pair the
// report carries.
struct PollRate {
    int hz;
    quint8 low;
    quint8 high;
};
const inline QList<PollRate> kPollRates{{125, 0x08, 0xf7}, {250, 0x04, 0xfb}, {500, 0x02, 0xfd}, {1000, 0x01, 0xfe}};

// One complete, writable configuration of the mouse.
struct Profile {
    QString name = QStringLiteral("Default");
    QList<int> dpi{400, 800, 1600, 3200, 6400, 12000};
    int activeStage = 2;  // 1-6
    bool ripple = false;
    bool angleSnap = false;
    int pollingHz = 1000;

    double sleepMin = 6.0;   // idle time before light sleep
    int deepSleepMin = 12;   // idle time before deep sleep
    int debounceMs = 4;      // click response time

    // Lighting: the same report 5 nibbles the X11 family uses. Left off until
    // a real R1 confirms the encoding (see README).
    bool lightOn = false;
    int lightMode = 0;  // 0 off, 1 static, 2 breathing, 3 neon, 4 breathing-cycle
    QColor lightColor{255, 0, 0};
    int lightBrightness = 8;  // 1-8
    int lightSpeed = 3;       // 1-5

    QJsonObject toJson() const;
    static Profile fromJson(const QJsonObject &);
    bool operator==(const Profile &) const;
    bool operator!=(const Profile &other) const { return !(*this == other); }
};

// DPI <-> the two payload bytes the sensor wants. The firmware's table is a
// 100-DPI step table; above 10000 the sensor doubles a halved value, and
// values in 10100..12000 set the "y = 1" marker byte instead.
int dpiByte(int dpi);
QByteArray dpiReport(const Profile &);
QByteArray timingReport(const Profile &);
QByteArray pollingReport(int hz);

// Battery from an unsolicited 5-byte report: percent, or -1 when the report is
// not a battery report.
int batteryPercent(const QByteArray &report, QString *state = nullptr);

}  // namespace r1
