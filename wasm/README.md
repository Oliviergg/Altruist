# Altruist — POC WebAssembly

Compile the C++ simulation core to WebAssembly and run it from a static HTML
page. No npm, no Node — just `emcc` (and `python3` to serve the page).

## What is built

| C++ file | Purpose | Built for WASM? |
|---|---|---|
| `island_model.cpp`, `haystack_model.cpp`, `regality_model.cpp`, `territoriality_model.cpp`, `epistasis_model.cpp` | All five simulation models | yes |
| `random.cpp` + `wallenius.cpp` | RNG and probability distributions | yes |
| `habitat.cpp` | Point-map / floating territories (used by territoriality, regality) | yes |
| `run.cpp` | Per-generation helpers (`statisticsInit0/1`, `findNeighbors`, `combineGenes`, …) | yes (Qt-coupled `Altruist::*`/`Worker::*` methods skipped via `#ifdef WASM_BUILD`) |
| `wasm/wasm_main.cpp` | Globals (`models`, `errors`), `Construct`, default parameters, parameter table, `extern "C"` exports | yes |
| `altruist.cpp`, `menus.cpp`, `graphics.cpp`, `parameterloop.cpp`, `data_file_out.cpp`, `parameter_file_io.cpp` | Qt UI / file I/O / parameter sweeps | **no** |

`wasm/qt_shim.h` provides empty stubs for `QString`, `QObject`, `QMainWindow`,
`Q_OBJECT`, `signals`, `emit`, the few `graphicsType*` constants the models
read, and friends, so `altruist.h` and the model `.cpp` files compile without
Qt installed. Two surgical edits keep the original codebase intact:

- `stdafx.h` gates the QtWidgets includes behind `!WASM_BUILD` and pulls in
  `qt_shim.h` instead.
- `altruist.h` does the same for the `<QObject>` / `<QFile>` / `<QElapsedTimer>`
  / `<QtWidgets/QMainWindow>` includes added on `main` for moc self-sufficiency.
- `run.cpp` wraps its `Altruist::*` / `Worker::*` method bodies in
  `#ifndef WASM_BUILD … #endif`. The pure simulation helpers below that block
  are kept and linked into the WASM binary.

## Build

Install Emscripten (`brew install emscripten` on macOS, `apt install emscripten`
on Debian/Ubuntu, or [emsdk](https://emscripten.org/docs/getting_started/downloads.html)).

```sh
make -C wasm
```

All artefacts land in **`build/wasm/`** at the repo root:
`altruist.js`, `altruist.wasm`, `index.html` (copied from sources), and
`test_native` (the equivalence-check binary).

The Qt build (`qmake && make` from the repo root) drops its outputs in
**`build/qt/`** by the same convention.

## Run

```sh
make -C wasm serve   # serves build/wasm/ on http://localhost:8000
```

Then open <http://localhost:8000/>.

The page provides:

- A **model picker** listing all five models (Island / Haystack / Regality /
  Territoriality / Epistasis).
- A **parameter panel** — geography, loci, fitness, group properties, run
  control. The form is generated from the C++ side via a metadata table
  (`paramTable[]` in `wasm_main.cpp`), so adding a new editable field is one
  line of code.
- **Init / Step / Run / Pause** buttons.
- A **Canvas** rendering the group grid coloured by the altruism gene fraction.

## Verify (WASM ≡ native)

To prove the WASM toolchain doesn't introduce numerical drift, the same
sources are compiled twice — once with `clang++` (native) and once with
`emcc` (WASM) — and their stats output is diffed:

```sh
make -C wasm verify
# -> "OK: native and WASM outputs match exactly"
```

## C API exposed to JavaScript

| Function | Notes |
|---|---|
| `wasm_get_model_count()`, `wasm_get_model_name(i)`, `wasm_get_model_description(i)`, `wasm_find_model(name)` | Model registry |
| `wasm_param_count()`, `wasm_param_section(i)`, `wasm_param_name(i)`, `wasm_param_label(i)`, `wasm_param_type(i)`, `wasm_param_array_size(i)` | Drive the dynamic form |
| `wasm_set_int / set_float / set_bool(name, idx, value)` and `wasm_get_*` | Read / write any registered parameter |
| `wasm_reset_defaults()` | Restore the demo defaults |
| `wasm_init_model(iModel, gridSide)` | Initialise simulation with the *current* parameters (gridSide=0 keeps whatever `maxIslands` is set to) |
| `wasm_init(iModel, seed, gridSide)` | Convenience: reset + set seed + init |
| `wasm_step(n)` | Advance n generations |
| `wasm_get_max_islands / row_length / generation / inhabited_groups / altruism_fraction / total_population / run_state` | Live readouts |
| `wasm_get_groups_ptr / group_size / pop_offset / altruism_offset` | Direct heap access for the Canvas renderer |

## Limitations

- No parameter sweeps (`parameterloop.cpp` is skipped — its `emit
  resultReadySignal` calls would need Qt's MOC).
- No file I/O (`.altru` save/load and data-file dumps need `QFile`/`QString`).
- A single shared `RandomVariates` / `AltruData` instance — no concurrent
  simulations.
- The form lists every entry from `paramTable[]`. Editing a parameter only
  takes effect on the next **Init / Reset** click (mirrors the original Qt
  dialog flow).
