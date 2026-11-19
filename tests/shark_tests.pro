QT += core testlib

CONFIG += c++17 console testcase
CONFIG -= app_bundle

TARGET = shark_tests
TEMPLATE = app

INCLUDEPATH += ../../src

HEADERS += \
    ../../src/r1.h

SOURCES += \
    shark_tests.cpp \
    ../../src/r1.cpp

LIBS += -lusb-1.0
