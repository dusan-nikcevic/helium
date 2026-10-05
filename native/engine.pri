DEFINES += ZEPHYR_ENGINE WAF_BUILD BOOST_BIND_GLOBAL_PLACEHOLDERS
ZEPHYR_ENGINE_ROOT = $$(ZEPHYR_ENGINE_ROOT)
isEmpty(ZEPHYR_ENGINE_ROOT): ZEPHYR_ENGINE_ROOT = $$clean_path($$PWD/..)
ZEPHYR_ENGINE_DEPS = $$clean_path($$PWD/.work/engine-deps/usr)
!exists($$ZEPHYR_ENGINE_ROOT/build/libs/ardour/libardour.so): error(Existing Ardour build is required. Set ZEPHYR_ENGINE_ROOT.)
!exists($$ZEPHYR_ENGINE_DEPS/include/boost): error(Local engine headers are required in native/.work/engine-deps.)
INCLUDEPATH += $$ZEPHYR_ENGINE_ROOT $$ZEPHYR_ENGINE_ROOT/build $$ZEPHYR_ENGINE_ROOT/libs
engine_modules = ardour pbd temporal evoral midi++2 audiographer lua zita-resampler zita-convolver ctrl-interface/control_protocol
for(engine_module, engine_modules) {
    INCLUDEPATH += $$ZEPHYR_ENGINE_ROOT/libs/$$engine_module $$ZEPHYR_ENGINE_ROOT/build/libs/$$engine_module
}
INCLUDEPATH += $$ZEPHYR_ENGINE_ROOT/libs/libltc/ltc
INCLUDEPATH += $$ZEPHYR_ENGINE_DEPS/include $$ZEPHYR_ENGINE_DEPS/include/glibmm-2.4 $$ZEPHYR_ENGINE_DEPS/lib/glibmm-2.4/include
INCLUDEPATH += $$ZEPHYR_ENGINE_DEPS/include/giomm-2.4 $$ZEPHYR_ENGINE_DEPS/lib/giomm-2.4/include
INCLUDEPATH += $$ZEPHYR_ENGINE_DEPS/include/sigc++-2.0 $$ZEPHYR_ENGINE_DEPS/lib/sigc++-2.0/include
QMAKE_CXXFLAGS += -Wno-unused-parameter
CONFIG += link_pkgconfig
PKGCONFIG += glib-2.0 gobject-2.0 gio-2.0 gthread-2.0 libxml-2.0
engine_libraries = ardour pbd temporal evoral midi++2 audiographer ptformat ctrl-interface/control_protocol
for(engine_library, engine_libraries) {
    engine_libdir = $$ZEPHYR_ENGINE_ROOT/build/libs/$$engine_library
    LIBS += -L$$engine_libdir
    QMAKE_LFLAGS += -Wl,-rpath,$$engine_libdir -Wl,-rpath-link,$$engine_libdir
}
LIBS += -L$$ZEPHYR_ENGINE_DEPS/lib -lardour -lardourcp -lpbd -ltemporal -levoral -lmidipp -laudiographer -lglibmm-2.4 -lgiomm-2.4 -lsigc-2.0
QMAKE_LFLAGS += -Wl,-rpath,$$ZEPHYR_ENGINE_DEPS/lib -Wl,-rpath-link,$$ZEPHYR_ENGINE_DEPS/lib
SOURCES += $$PWD/engine/ardourbridge.cpp $$PWD/engine/checks.cpp
HEADERS += $$PWD/engine/ardourbridge.h $$PWD/engine/checks.h

SOURCES += $$PWD/engine/commandexecutor.cpp
HEADERS += $$PWD/engine/commandexecutor.h
HEADERS += $$PWD/engine/enginesession.h
SOURCES += $$PWD/engine/commandchecks.cpp
