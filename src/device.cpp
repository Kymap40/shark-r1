#include "device.h"

#include "r1.h"

#include <libusb-1.0/libusb.h>

#include <QElapsedTimer>
#include <QThread>

namespace {

libusb_context *context() {
    static libusb_context *ctx = nullptr;
    if (!ctx)
        libusb_init(&ctx);
    return ctx;
}

QString libusbError(int code) {
    return QString::fromLatin1(libusb_error_name(code));
}

}  // namespace

Device::~Device() {
    close();
}

bool Device::open() {
    close();
    libusb_device **list = nullptr;
    const ssize_t count = libusb_get_device_list(context(), &list);
    if (count < 0) {
        m_state = State::Error;
        m_error = QStringLiteral("libusb: %1").arg(libusbError(int(count)));
        return false;
    }

    libusb_device *found = nullptr;
    bool wired = false;
    for (ssize_t i = 0; i < count && !found; ++i) {
        libusb_device_descriptor descriptor{};
        if (libusb_get_device_descriptor(list[i], &descriptor) != 0)
            continue;
        if (descriptor.idVendor != kVendor)
            continue;
        if (descriptor.idProduct == kDongle) {
            found = list[i];
            wired = false;
        } else if (descriptor.idProduct == kWired) {
            found = list[i];
            wired = true;
        }
    }

    if (!found) {
        libusb_free_device_list(list, 1);
        m_state = State::NoMouse;
        m_error = QStringLiteral("Attack Shark R1 not connected");
        return false;
    }

    libusb_device_handle *handle = nullptr;
    const int opened = libusb_open(found, &handle);
    libusb_free_device_list(list, 1);
    if (opened != 0) {
        m_state = State::Error;
        m_error = opened == LIBUSB_ERROR_ACCESS
                      ? QStringLiteral("no permission to open the mouse (install the udev rule)")
                      : QStringLiteral("cannot open the mouse: %1").arg(libusbError(opened));
        return false;
    }
    m_handle = handle;
    m_wired = wired;

    if (libusb_kernel_driver_active(handle, kConfigInterface) == 1) {
        if (libusb_detach_kernel_driver(handle, kConfigInterface) == 0)
            m_detached = true;
    }

    const int claimed = libusb_claim_interface(handle, kConfigInterface);
    if (claimed != 0) {
        m_state = State::Error;
        m_error = claimed == LIBUSB_ERROR_BUSY
                      ? QStringLiteral("the config interface is in use by another program")
                      : QStringLiteral("cannot claim the config interface: %1").arg(libusbError(claimed));
        close();
        return false;
    }
    m_claimed = true;
    m_state = State::Open;
    m_error.clear();
    return true;
}

void Device::close() {
    if (!m_handle)
        return;
    auto *handle = static_cast<libusb_device_handle *>(m_handle);
    if (m_claimed)
        libusb_release_interface(handle, kConfigInterface);
    libusb_close(handle);
    m_handle = nullptr;
    m_claimed = false;
    m_detached = false;
    m_state = State::Closed;
}

bool Device::ackFor(quint8 reportId, QString *error) {
    auto *handle = static_cast<libusb_device_handle *>(m_handle);
    QElapsedTimer timer;
    timer.start();
    QByteArray buffer(64, '\0');
    while (timer.elapsed() < 1500) {
        int transferred = 0;
        const int rc = libusb_interrupt_transfer(handle, kEventEndpoint,
                                                 reinterpret_cast<unsigned char *>(buffer.data()),
                                                 buffer.size(), &transferred, 150);
        if (rc != 0 || transferred < 5)
            continue;
        if (quint8(buffer[0]) != 0x03 || quint8(buffer[4]) != reportId)
            continue;  // a battery or status report, not our ack
        if (quint8(buffer[2]) != 0x50) {
            if (error)
                *error = QStringLiteral("the mouse rejected the report (status 0x%1)")
                             .arg(quint8(buffer[2]), 2, 16, QLatin1Char('0'));
            return false;
        }
        return true;
    }
    if (error)
        *error = QStringLiteral("the mouse did not answer -- wake it by moving it and try again");
    return false;
}

bool Device::send(const QByteArray &report, QString *error) {
    if (m_state != State::Open) {
        if (!open()) {
            if (error)
                *error = m_error;
            return false;
        }
    }
    auto *handle = static_cast<libusb_device_handle *>(m_handle);
    const quint8 reportId = quint8(report.at(0));
    QString last;
    for (int attempt = 0; attempt < 4; ++attempt) {
        if (attempt)
            QThread::msleep(150 * attempt);
        const int rc = libusb_control_transfer(handle, 0x21, 0x09, quint16(0x0300 | reportId),
                                               kConfigInterface,
                                               reinterpret_cast<unsigned char *>(const_cast<char *>(report.constData())),
                                               quint16(report.size()), 1000);
        if (rc < 0) {
            last = QStringLiteral("USB write failed: %1").arg(libusbError(rc));
            continue;
        }
        QString ackError;
        if (ackFor(reportId, &ackError))
            return true;
        last = ackError;
    }
    if (error)
        *error = last;
    return false;
}

int Device::battery(QString *state) {
    if (m_state != State::Open && !open())
        return -1;
    auto *handle = static_cast<libusb_device_handle *>(m_handle);
    QElapsedTimer timer;
    timer.start();
    QByteArray buffer(64, '\0');
    while (timer.elapsed() < 3000) {
        int transferred = 0;
        const int rc = libusb_interrupt_transfer(handle, kEventEndpoint,
                                                 reinterpret_cast<unsigned char *>(buffer.data()),
                                                 buffer.size(), &transferred, 200);
        if (rc != 0 || transferred < 5)
            continue;
        const int percent = r1::batteryPercent(buffer.left(transferred), state);
        if (percent >= 0)
            return percent;
    }
    return -1;
}
