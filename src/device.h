// The USB side of the mouse: one vendor interface, feature reports out, an
// interrupt endpoint in. Nothing here knows what the bytes mean.

#pragma once

#include <QByteArray>
#include <QString>

class Device {
public:
    enum class State { Closed, NoMouse, Open, Error };

    static constexpr quint16 kVendor = 0x1d57;
    static constexpr quint16 kDongle = 0xfa60;
    static constexpr quint16 kWired = 0xfa61;
    static constexpr int kConfigInterface = 2;
    static constexpr int kEventEndpoint = 0x83;

    ~Device();

    State state() const { return m_state; }
    bool isWired() const { return m_wired; }
    QString lastError() const { return m_error; }

    // Finds the mouse and takes the vendor interface. Detaching the kernel
    // driver is what the vendor software effectively does on Windows; the
    // mouse's own input interfaces are untouched.
    bool open();

    // Gives the interface back, so plugging another tool in keeps working.
    void close();

    // Sends one feature report and waits for the firmware's acknowledgement.
    // The dongle drops writes now and then, so this retries and only reports
    // success on a real ack.
    bool send(const QByteArray &report, QString *error = nullptr);

    // Reads whatever the mouse pushes: battery level and its charging state.
    // -1 when nothing useful arrives.
    int battery(QString *state = nullptr);

private:
    bool ackFor(quint8 reportId, QString *error);
    void *m_handle = nullptr;
    bool m_detached = false;
    bool m_claimed = false;
    State m_state = State::Closed;
    bool m_wired = false;
    QString m_error;
};
