QT += quick quickcontrols2 testlib network
CONFIG += c++17
TEMPLATE = app
TARGET = zephyr-native
SOURCES += main.cpp session.cpp waveform.cpp aiclient.cpp aiplanner.cpp mixhistory.cpp fixturecommands.cpp tests/selftest.cpp tests/aiclientcheck.cpp tests/aiplannercheck.cpp tests/mixhistorycheck.cpp
HEADERS += session.h waveform.h aiclient.h aiplanner.h mixhistory.h fixturecommands.h tests/selftest.h tests/aiclientcheck.h tests/aiplannercheck.h tests/mixhistorycheck.h
DEFINES += ZEPHYR_SOURCE_DIR=\\\"$$PWD\\\"
engine: include(engine.pri)
