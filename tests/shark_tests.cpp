// The protocol is the part that can silently corrupt a mouse, so its byte
// layouts are pinned here against values the firmware itself accepted.

#include <QtTest>

#include "r1.h"

class SharkTests : public QObject {
    Q_OBJECT

private slots:
    void dpiEncodingRoundTrips();
    void dpiReportCarriesEveryStage();
    void dpiReportChecksumFollowsTheFirmware();
    void highDpiStagesSetTheDoublingMask();
    void timingReportKeepsSleepAndDebounce();
    void pollingReportCarriesTheRateCode();
    void batteryReportsAreParsed();
    void profileSurvivesJson();
    void profileClampsOutOfRangeValues();
};

void SharkTests::dpiEncodingRoundTrips() {
    // The six values straight out of the firmware's 100-DPI step table.
    QCOMPARE(r1::dpiByte(100), 0x02);
    QCOMPARE(r1::dpiByte(200), 0x04);
    QCOMPARE(r1::dpiByte(300), 0x06);
    QCOMPARE(r1::dpiByte(400), 0x09);
    QCOMPARE(r1::dpiByte(500), 0x0b);
    QCOMPARE(r1::dpiByte(600), 0x0e);
    QCOMPARE(r1::dpiByte(800), 0x12);
    QCOMPARE(r1::dpiByte(1600), r1::dpiByte(1600));
    QVERIFY(r1::dpiByte(1600) > r1::dpiByte(800));
    QVERIFY(r1::dpiByte(18000) != r1::dpiByte(16000));
    // Anything outside the sensor's range is clamped, never wrapped.
    QCOMPARE(r1::dpiByte(10), r1::dpiByte(100));
    QCOMPARE(r1::dpiByte(99999), r1::dpiByte(18000));
}

void SharkTests::dpiReportCarriesEveryStage() {
    r1::Profile profile;
    profile.dpi = {400, 800, 1600, 3200, 6400, 12000};
    profile.activeStage = 3;
    const QByteArray report = r1::dpiReport(profile);

    QCOMPARE(report.size(), 56);
    QCOMPARE(quint8(report[0]), quint8(0x04));
    QCOMPARE(quint8(report[1]), quint8(0x38));
    for (int i = 0; i < r1::kStages; ++i)
        QCOMPARE(quint8(report[8 + i]), quint8(r1::dpiByte(profile.dpi.at(i))));
    QCOMPARE(quint8(report[24]), quint8(3));
    // Bytes the app does not model are passed through untouched.
    QCOMPARE(quint8(report[25]), quint8(0xff));
    QCOMPARE(quint8(report[49]), quint8(0x02));
}

void SharkTests::dpiReportChecksumFollowsTheFirmware() {
    r1::Profile profile;
    profile.dpi = {400, 800, 1600, 3200, 6400, 12000};
    profile.activeStage = 1;
    profile.ripple = false;
    profile.angleSnap = false;

    int checksum = 0x0d75;
    for (int i = 0; i < r1::kStages; ++i) {
        const int dpi = profile.dpi.at(i);
        checksum += r1::dpiByte(dpi);
        checksum += (dpi >= 10100 && dpi <= 12000) ? 1 : 0;
    }
    const QByteArray report = r1::dpiReport(profile);
    QCOMPARE(quint8(report[50]), quint8((checksum >> 8) & 0xff));
    QCOMPARE(quint8(report[51]), quint8(checksum & 0xff));

    // Each toggle adds one to the running sum.
    r1::Profile toggled = profile;
    toggled.ripple = true;
    const QByteArray second = r1::dpiReport(toggled);
    const int secondSum = (quint8(second[50]) << 8) | quint8(second[51]);
    QCOMPARE(secondSum, checksum + 1);
}

void SharkTests::highDpiStagesSetTheDoublingMask() {
    r1::Profile profile;
    profile.dpi = {400, 800, 1600, 3200, 6400, 18000};
    const QByteArray report = r1::dpiReport(profile);
    QCOMPARE(quint8(report[6]), quint8(1 << 5));
    QCOMPARE(quint8(report[7]), quint8(1 << 5));

    // 10100..12000 uses the marker byte instead of the mask bit.
    r1::Profile middle;
    middle.dpi = {400, 800, 1600, 3200, 6400, 12000};
    const QByteArray marker = r1::dpiReport(middle);
    QCOMPARE(quint8(marker[6]), quint8(0));
    QCOMPARE(quint8(marker[21]), quint8(1));
}

void SharkTests::timingReportKeepsSleepAndDebounce() {
    r1::Profile profile;
    profile.sleepMin = 6.0;
    profile.deepSleepMin = 12;
    profile.debounceMs = 4;
    const QByteArray report = r1::timingReport(profile);

    QCOMPARE(report.size(), 15);
    QCOMPARE(quint8(report[0]), quint8(0x05));
    QCOMPARE(quint8(report[1]), quint8(0x0f));
    QCOMPARE(quint8(report[9]), quint8(12));  // minutes are stored doubled
    QCOMPARE(quint8(report[10]), quint8(2));  // milliseconds are halved
    QCOMPARE(quint8(report[4]) & 0xf0, quint8(12 & 0xf0));
    QCOMPARE(quint8(report[5]) >> 4, quint8(12 & 0x0f));
    QCOMPARE(quint8(report[11]), quint8(0x01));
    // The checksum byte only depends on the fields the firmware folds in.
    const int expected = ((((12 & 0x0f) + ((12 & 0xf0) >> 4)) << 4) + 0x0a + 12 + 2) & 0xff;
    QCOMPARE(quint8(report[12]), quint8(expected));
}

void SharkTests::pollingReportCarriesTheRateCode() {
    struct Case {
        int hz;
        quint8 low;
        quint8 high;
    };
    const Case cases[] = {{125, 0x08, 0xf7}, {250, 0x04, 0xfb}, {500, 0x02, 0xfd}, {1000, 0x01, 0xfe}};
    for (const auto &c : cases) {
        const QByteArray report = r1::pollingReport(c.hz);
        QCOMPARE(report.size(), 9);
        QCOMPARE(quint8(report[0]), quint8(0x06));
        QCOMPARE(quint8(report[3]), c.low);
        QCOMPARE(quint8(report[4]), c.high);
    }
}

void SharkTests::batteryReportsAreParsed() {
    QString state;
    QCOMPARE(r1::batteryPercent(QByteArray::fromHex("031040010a"), &state), 100);
    QCOMPARE(state, QStringLiteral("discharging"));
    QCOMPARE(r1::batteryPercent(QByteArray::fromHex("0310400305"), &state), 50);
    QCOMPARE(state, QStringLiteral("charging"));
    QCOMPARE(r1::batteryPercent(QByteArray::fromHex("0310500006")), -1);  // an ack, not a level
    QCOMPARE(r1::batteryPercent(QByteArray()), -1);
}

void SharkTests::profileSurvivesJson() {
    r1::Profile profile;
    profile.name = QStringLiteral("Gaming");
    profile.dpi = {400, 800, 1200, 2400, 6400, 16000};
    profile.activeStage = 4;
    profile.pollingHz = 500;
    profile.ripple = true;
    profile.angleSnap = true;
    profile.sleepMin = 2.5;
    profile.deepSleepMin = 20;
    profile.debounceMs = 8;

    const r1::Profile back = r1::Profile::fromJson(profile.toJson());
    QVERIFY(back == profile);
    QCOMPARE(back.name, QStringLiteral("Gaming"));
    QCOMPARE(back.dpi.at(3), 2400);
}

void SharkTests::profileClampsOutOfRangeValues() {
    QJsonObject json;
    json["activeStage"] = 99;
    json["sleepMin"] = 0.1;
    json["deepSleepMin"] = 900;
    json["debounceMs"] = 7;
    const r1::Profile profile = r1::Profile::fromJson(json);
    QCOMPARE(profile.activeStage, 6);
    QCOMPARE(profile.sleepMin, 0.5);
    QCOMPARE(profile.deepSleepMin, 60);
    QCOMPARE(profile.debounceMs, 6);  // rounded down to the even step the firmware takes
}

QTEST_APPLESS_MAIN(SharkTests)
#include "shark_tests.moc"
