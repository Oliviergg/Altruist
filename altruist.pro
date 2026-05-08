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

# Source files: every .cpp / .h at the project root
SOURCES  += $$files(*.cpp)
HEADERS  += $$files(*.h)

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
}
