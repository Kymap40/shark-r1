QT += core gui qml quick quickcontrols2

CONFIG += c++17 release
TARGET = shark
TEMPLATE = app

HEADERS += \
    src/backend.h \
    src/device.h \
    src/r1.h \
    src/theme.h \
    src/translator.h

SOURCES += \
    src/main.cpp \
    src/backend.cpp \
    src/device.cpp \
    src/r1.cpp \
    src/theme.cpp \
    src/translator.cpp

RESOURCES += src/resources.qrc

LIBS += -lusb-1.0
