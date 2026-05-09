// Minimal Qt stubs for the WASM/headless build.
// Only the symbols referenced by the simulation-side headers are provided.
// Qt-coupled .cpp files (altruist.cpp, menus.cpp, graphics.cpp, ...) are NOT compiled here.
#pragma once

#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <math.h>
#include <cmath>

// Constants that the models read from graphics.h. We don't include the real
// graphics.h because it pulls QGraphicsView in.
const int graphicsNone        = 0;
const int graphicsIslands     = 1;
const int graphicsTerritories = 2;
const int graphicsLimits      = 3;

// MSVC's bounds-checked sprintf is unavailable on emcc/Clang/GCC.
// Map to snprintf using sizeof at the call site (works because all current
// call sites pass a stack array as the destination buffer).
#ifndef _MSC_VER
#define sprintf_s(buf, ...) snprintf((buf), sizeof(buf), __VA_ARGS__)
#endif

#define Q_OBJECT
#define signals public
#define slots
#define emit
#define Q_SLOTS
#define Q_SIGNALS public

struct QString {
    QString() = default;
    QString(const char*) {}
    bool isEmpty() const { return true; }
    QString operator+(const char*) const { return *this; }
};
struct QObject { virtual ~QObject() = default; };
struct QWidget : public QObject {};
struct QMainWindow : public QWidget {};
struct QThread {};
struct QFile {};
struct QElapsedTimer {};
struct QLineEdit {};
struct QAction {};
struct QMenu { QMenu(const char* = nullptr) {} };

namespace Ui { struct AltruistClass {}; }
