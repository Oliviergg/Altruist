# Build Altruist as a WebAssembly module using Emscripten.
#
# Requirements: emcc on PATH (Homebrew: `brew install emscripten`).
# Build: `make -C wasm`
# Serve: `make -C wasm serve`  -> http://localhost:8000/
# Verify: `make -C wasm verify` (build native + wasm, diff their output)
#
# All artefacts go under build/wasm/ at the repo root, alongside Qt's
# build/qt/ from altruist.pro.

EMCC       ?= emcc
CXX_NATIVE ?= clang++

BUILD_DIR  = ../build/wasm

# Sources we can build without Qt:
#   wasm_main.cpp     - globals + emscripten exports + default parameters
#   run.cpp           - simulation helpers (Qt-coupled methods #ifdef'd out)
#   *_model.cpp       - all the simulation models
#   habitat.cpp       - point-map / territory geometry (territoriality, regality)
#   random.cpp        - RandomVariates (RNG)
#   wallenius.cpp     - non-central hypergeometric distributions used by random.cpp
SOURCES = \
	wasm_main.cpp \
	../run.cpp \
	../habitat.cpp \
	../island_model.cpp \
	../haystack_model.cpp \
	../regality_model.cpp \
	../territoriality_model.cpp \
	../epistasis_model.cpp \
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

WASM_OUT     = $(BUILD_DIR)/altruist.js
NATIVE_OUT   = $(BUILD_DIR)/test_native
INDEX_OUT    = $(BUILD_DIR)/index.html

all: $(WASM_OUT) $(INDEX_OUT)

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

$(WASM_OUT): $(SOURCES) qt_shim.h ../stdafx.h | $(BUILD_DIR)
	$(EMCC) $(CXXFLAGS) $(SOURCES) -o $(WASM_OUT) $(LDFLAGS)

$(INDEX_OUT): index.html | $(BUILD_DIR)
	cp index.html $(BUILD_DIR)/index.html

$(NATIVE_OUT): test_native.cpp $(SOURCES) qt_shim.h ../stdafx.h | $(BUILD_DIR)
	$(CXX_NATIVE) $(CXXFLAGS) test_native.cpp $(SOURCES) -o $(NATIVE_OUT)

clean:
	rm -rf $(BUILD_DIR)

serve: all
	@echo "Open http://localhost:8000/ in your browser."
	cd $(BUILD_DIR) && python3 -m http.server 8000

# --- Equivalence harness ----------------------------------------------------
# Build a native CLI (no Qt) from the same .cpp files as the WASM build,
# run both, and diff the stats. Identical output = no drift between
# toolchains.

test-native: $(NATIVE_OUT)
	$(NATIVE_OUT)

test-node: $(WASM_OUT)
	node test_node.js

verify: $(NATIVE_OUT) $(WASM_OUT)
	@$(NATIVE_OUT) > /tmp/altruist_native.out
	@node test_node.js > /tmp/altruist_wasm.out
	@if diff -u /tmp/altruist_native.out /tmp/altruist_wasm.out; then \
		echo "OK: native and WASM outputs match exactly"; \
	else \
		echo "MISMATCH between native and WASM output (see diff above)"; exit 1; \
	fi

.PHONY: all clean serve test-native test-node verify
