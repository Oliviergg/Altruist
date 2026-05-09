# Build Altruist as a WebAssembly module using Emscripten.
# Requirements:
#   - emsdk activated in your shell (`source emsdk_env.sh`).
#   - run from the repository root: `make -C wasm`
# Output:
#   wasm/altruist.js, wasm/altruist.wasm
# Run:
#   `make -C wasm serve` and open http://localhost:8000/wasm/

EMCC ?= emcc

# Sources we can build without Qt:
#   wasm_main.cpp  - globals + emscripten exports + default parameters
#   run.cpp        - simulation helpers (Qt-coupled methods are #ifdef'd out via -DWASM_BUILD)
#   island_model.cpp - the model we expose
#   random.cpp     - RandomVariates (RNG)
#   wallenius.cpp  - non-central hypergeometric distributions used by random.cpp
SOURCES = \
	wasm_main.cpp \
	../run.cpp \
	../island_model.cpp \
	../random.cpp \
	../wallenius.cpp

CXXFLAGS = -O2 -std=c++17 -DWASM_BUILD -I. -I.. \
	-Wno-deprecated-declarations -Wno-narrowing

LDFLAGS = \
	-s ALLOW_MEMORY_GROWTH=1 \
	-s INITIAL_MEMORY=33554432 \
	-s ENVIRONMENT=web,node \
	-s MODULARIZE=1 \
	-s EXPORT_NAME=AltruistModule \
	-s 'EXPORTED_RUNTIME_METHODS=["cwrap","ccall","HEAPU8","HEAP32","HEAPF32"]'

OUT = altruist.js

all: $(OUT)

$(OUT): $(SOURCES) qt_shim.h ../stdafx.h
	$(EMCC) $(CXXFLAGS) $(SOURCES) -o $(OUT) $(LDFLAGS)

clean:
	rm -f altruist.js altruist.wasm

serve:
	@echo "Open http://localhost:8000/wasm/ in your browser."
	cd .. && python3 -m http.server 8000

.PHONY: all clean serve
