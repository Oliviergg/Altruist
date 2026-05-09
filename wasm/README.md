# Altruist — POC WebAssembly

Compile the C++ simulation core to WebAssembly and run it from a static HTML
page. No npm, no Node — just `emcc` and `python3 -m http.server`.

## What is built

| C++ file | Purpose | Built for WASM? |
|---|---|---|
| `island_model.cpp` | One concrete simulation model | yes |
| `random.cpp` + `wallenius.cpp` | RNG and probability distributions | yes |
| `run.cpp` | Per-generation helpers (`statisticsInit0/1`, `findNeighbors`, `combineGenes`, …) | yes (Qt-coupled `Altruist::*`/`Worker::*` methods are skipped via `#ifdef WASM_BUILD`) |
| `wasm/wasm_main.cpp` | Globals (`models`, `errors`), `Construct`, default parameters, `extern "C"` exports | yes |
| `altruist.cpp`, `menus.cpp`, `graphics.cpp`, `parameterloop.cpp`, `data_file_out.cpp`, `parameter_file_io.cpp` | Qt UI / file I/O | **no** |

`wasm/qt_shim.h` provides empty stubs for `QString`, `QObject`, `QMainWindow`,
`Q_OBJECT`, `signals`, `emit`, etc., so that `altruist.h` and friends compile
cleanly without Qt installed. The original `stdafx.h` was edited so that under
`-DWASM_BUILD` it pulls in `qt_shim.h` instead of QtWidgets.

`run.cpp` was edited to wrap the `Altruist::*` and `Worker::*` method bodies in
`#ifndef WASM_BUILD … #endif`. The pure helpers below that block are kept and
linked into the WASM binary.

## Build

Install [emsdk](https://emscripten.org/docs/getting_started/downloads.html)
then activate it in your shell:

```sh
source /path/to/emsdk/emsdk_env.sh
make -C wasm
```

Outputs: `wasm/altruist.js` and `wasm/altruist.wasm`.

## Run

```sh
make -C wasm serve   # starts http://localhost:8000
```

Then open <http://localhost:8000/wasm/>.

## Limitations of the POC

- Only the `Island` model is wired up. Adding the others is mostly a matter of
  appending their `.cpp` files to `SOURCES` in the Makefile and pre-allocating
  any required `extraBuffer[]` slots.
- No parameter sweeps (`parameterloop.cpp` is skipped — its `emit
  resultReadySignal` calls require Qt's MOC).
- No file I/O (data dumps and `.altru` parameter files are skipped — they use
  `QFile`/`QString` heavily).
- Default parameters are hard-coded in `wasm_main.cpp::setReasonableDefaults`.
  The full GUI sets these via dialog boxes (`menus.cpp`); reproducing the same
  defaults exactly would require porting the `Altruist::initializeData()`
  body into `wasm_main.cpp`.

## To go further

1. Expose more parameters to JS (`wasm_set_param(name, value)` style) so the
   HTML UI can replace the Qt dialogs.
2. Compile each model and let JS pick by index (already supported by
   `wasm_get_model_count`/`wasm_get_model_name`).
3. Reimplement parameter sweeps in JS rather than porting `parameterloop.cpp`,
   since most of its complexity is its threading and signal/slot model.
