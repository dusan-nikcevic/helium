QT += quick quickcontrols2 testlib
CONFIG += c++17
TEMPLATE = app
TARGET = zephyr-native
SOURCES += main.cpp session.cpp waveform.cpp tests/selftest.cpp
HEADERS += session.h waveform.h tests/selftest.h
DEFINES += ZEPHYR_SOURCE_DIR=\\\"$$PWD\\\"
engine: include(engine.pri)
