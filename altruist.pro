######################################################################
# Altruist qmake project file
#
# Build instructions (macOS / Linux):
#   qmake
#   make -j8
#
# Or open this file in Qt Creator.
#
# Requires Qt 6 (>= 6.4) with the Widgets module.
######################################################################

QT       += core gui widgets
CONFIG   += c++17

TARGET    = Altruist
TEMPLATE  = app

# Explicit source list. Avoid wildcards: qmake generates moc_*.cpp /
# qrc_*.cpp / ui_*.h itself, and a glob would pick those up a second
# time and produce duplicate-symbol link errors.
SOURCES = \
    altruist.cpp \
    data_file_out.cpp \
    epistasis_model.cpp \
    graphics.cpp \
    habitat.cpp \
    haystack_model.cpp \
    island_model.cpp \
    menus.cpp \
    parameter_file_io.cpp \
    parameterloop.cpp \
    random.cpp \
    regality_model.cpp \
    run.cpp \
    stdafx.cpp \
    territoriality_model.cpp \
    wallenius.cpp

HEADERS = \
    altruist.h \
    graphics.h \
    habitat.h \
    menus.h \
    parameterloop.h \
    random.h \
    stdafx.h

# Qt Designer form
FORMS    += altruist.ui

# Resource file
RESOURCES += altruist.qrc

# Precompiled header (used by Visual Studio; honoured by qmake too)
PRECOMPILED_HEADER = stdafx.h

# Quiet a few warnings that come from the existing code base
macx|unix:!macx {
    QMAKE_CXXFLAGS += -Wno-deprecated-declarations \
                      -Wno-unused-result \
                      -Wno-format-security
}

macx {
    # Sensible deployment target for recent Qt 6 releases
    QMAKE_MACOSX_DEPLOYMENT_TARGET = 11.0

    # Keep -O2 (qmake default) and add -g so Instruments / xctrace can
    # resolve symbols. Frame pointers help the call-stack sampler.
    QMAKE_CXXFLAGS_RELEASE += -g -fno-omit-frame-pointer
    QMAKE_LFLAGS_RELEASE   += -g
}
