// Native CLI test for the Altruist WASM POC.
// Compiles the *same* simulation sources as the WASM build (no Qt) and prints
// deterministic stats. Compare against `node test_node.js` to verify the WASM
// build does not introduce numerical drift.
//
// Build & run:  make -C wasm test-native
#include "../stdafx.h"
#include <stdio.h>

extern "C" {
int wasm_init(int iModel, int seed, int gridSide);
int wasm_step(int n);
int wasm_get_max_islands();
int wasm_get_inhabited_groups();
float wasm_get_altruism_fraction();
int64_t wasm_get_total_population();
int64_t wasm_get_generation();
int wasm_get_model_count();
const char * wasm_get_model_name(int i);
int wasm_find_model(const char * name);
}

int main() {
    printf("models=%d\n", wasm_get_model_count());
    for (int i = 0; i < wasm_get_model_count(); i++) {
        printf("model[%d]=%s\n", i, wasm_get_model_name(i));
    }
    int idx = wasm_find_model("Island model");
    int rc = wasm_init(idx, 42, 16);
    printf("init rc=%d maxIslands=%d\n", rc, wasm_get_max_islands());
    for (int g = 0; g < 10; g++) {
        wasm_step(20);
        printf("gen=%lld pop=%lld inhabited=%d altruism=%.6f\n",
            (long long)wasm_get_generation(),
            (long long)wasm_get_total_population(),
            wasm_get_inhabited_groups(),
            (double)wasm_get_altruism_fraction());
    }
    return 0;
}
