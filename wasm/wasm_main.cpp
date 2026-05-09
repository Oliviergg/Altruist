// WASM entry point for the Altruist simulation POC.
// Provides the globals and constructors normally defined in altruist.cpp,
// and exposes a small C API to JavaScript via emscripten.
#include "stdafx.h"
#include <emscripten.h>
#include <stdlib.h>
#include <string.h>

ModelDescriptorList models;
ErrorReporter errors;

void ErrorReporter::reportError(const char * text) {
    if (num++ == 0) strncpy(errorText, text, 1024);
}
void ErrorReporter::clear() {
    num = 0;
    errorText[0] = '?'; errorText[1] = 0;
}

ModelDescriptorList::ModelDescriptorList() {}

void ModelDescriptorList::addModel(ModelDescriptor const & m) {
    static int n = 0;
    if (n >= maxModels) return;
    models[n++] = m;
    nModels = n;
    sorted = false;
}
uint32_t ModelDescriptorList::getNum() { return nModels; }
ModelDescriptor * ModelDescriptorList::getModel(uint32_t i) {
    if (i >= nModels) return nullptr;
    return models + i;
}
void ModelDescriptorList::sort() {
    if (sorted) return;
    for (int32_t i = 0; i < (int32_t)nModels; i++) {
        for (int32_t j = 0; j < (int32_t)nModels - i - 1; j++) {
            ModelDescriptor * p1 = models + j;
            ModelDescriptor * p2 = models + j + 1;
            if (strcasecmp(p1->name, p2->name) > 0) {
                ModelDescriptor t = *p1; *p1 = *p2; *p2 = t;
            }
        }
    }
    sorted = true;
}

Construct::Construct(ModelDescriptor const & m) { models.addModel(m); }


// One static AltruData and its random generator.
static AltruData g_d;
static RandomVariates g_rng(1);

static void setReasonableDefaults(AltruData * d) {
    // mirrors a subset of Altruist::initializeData() — only what the simulation actually reads.
    memset(d, 0, sizeof(*d));
    d->ran = &g_rng;

    d->nLoci = 3;
    d->locusUsed[0] = true;   // altruism
    d->locusUsed[1] = false;  // endogamy
    d->locusUsed[2] = false;  // conformity
    d->dominance[0] = recessive;
    d->dominance[1] = recessive;
    d->dominance[2] = recessive;
    d->fg0[0] = 0.5f; d->fg0[1] = 0.f; d->fg0[2] = 0.f;
    d->murate[0][0] = 1e-4; d->murate[0][1] = 1e-4;
    d->murate[1][0] = 0; d->murate[1][1] = 0;
    d->murate[2][0] = 0; d->murate[2][1] = 0;

    d->maxIslands = 64;                      // 8x8 grid
    d->rowLength = 8;
    d->numRows = 8;
    d->minGroupSize = 2;
    d->colonySize = 4;
    d->carryingCapacity[0] = 100.f;
    d->carryingCapacity[1] = 100.f;
    d->carryingCapacityStandardDeviation = 0.f;

    // fitness array: [egoist among egoists, egoist among altruists, altruist among egoists, altruist among altruists]
    d->fit[0] = 1.0f;
    d->fit[1] = 1.2f;   // egoists among altruists do better -> selection pressure against altruists
    d->fit[2] = 0.8f;   // altruists among egoists do worse
    d->fit[3] = 1.0f;
    d->fit2[0] = d->fit2[1] = d->fit2[2] = d->fit2[3] = 1.f;
    d->growthRate = 2.0f;
    d->selectionModel = 0; // fecundity selection

    d->extinctionRate[0] = 0.05f; // small egoist
    d->extinctionRate[1] = 0.02f; // big egoist
    d->extinctionRate[2] = 0.02f; // small altruist
    d->extinctionRate[3] = 0.005f;// big altruist
    d->surviv = 0.f;
    d->fitfunc = 2;        // linear
    d->groupFitCurvature = 1.f;

    d->migrationRate[0] = 0.05f;
    d->migrationRate[1] = 0.05f;
    d->emigrationPattern = 0;
    d->immigrationPattern = 0; // common pool
    d->colonizationPattern = 0;
    d->migrationTopology = 2;  // quadratic, 4 neighbors

    d->minimumGenerations = 0;
    d->maximumGenerations = 1000000;
    d->stopCriterion = 0;
    d->seed = 1;
}

extern "C" {

EMSCRIPTEN_KEEPALIVE int wasm_get_model_count() { return (int)models.getNum(); }
EMSCRIPTEN_KEEPALIVE const char * wasm_get_model_name(int i) {
    ModelDescriptor * m = models.getModel((uint32_t)i);
    return m ? m->name : nullptr;
}

// Initialize the simulation with model index `iModel` (after sort).
// Returns 0 on success, -1 if model not found.
EMSCRIPTEN_KEEPALIVE int wasm_init(int iModel, int seed, int gridSide) {
    models.sort();
    setReasonableDefaults(&g_d);
    if (gridSide > 0) {
        g_d.maxIslands = gridSide * gridSide;
        g_d.rowLength = gridSide;
        g_d.numRows = gridSide;
    }
    if (seed != 0) g_d.seed = seed;
    g_rng.randomInit(g_d.seed);

    g_d.iModel = iModel;
    g_d.currentModel = models.getModel((uint32_t)iModel);
    if (g_d.currentModel == nullptr) return -1;

    // Run model init (state == model_initialize)
    g_d.currentModel->initFunction(&g_d, model_initialize);

    // Allocate group buffer
    g_d.groupStructureSize = g_d.modelGroupStructureSize;
    int64_t bytes = (int64_t)(g_d.maxIslands + 1) * g_d.groupStructureSize;
    if (g_d.groupData) { delete[] g_d.groupData; g_d.groupData = nullptr; }
    g_d.groupData = new int8_t[bytes]();
    g_d.bufferSize = bytes;

    // Run state_start to populate initial groups
    g_d.runState = state_start;
    g_d.generations = 0;
    g_d.currentModel->generationFunction(&g_d, state_start);
    return 0;
}

// Advance by `n` generations; returns the new generation count.
// Stops early if simulation reaches a stop condition.
EMSCRIPTEN_KEEPALIVE int wasm_step(int n) {
    if (g_d.currentModel == nullptr) return -1;
    for (int i = 0; i < n && g_d.runState == state_run; i++) {
        g_d.currentModel->generationFunction(&g_d, state_run);
        g_d.generations++;
    }
    return (int)g_d.generations;
}

EMSCRIPTEN_KEEPALIVE int wasm_get_max_islands()      { return g_d.maxIslands; }
EMSCRIPTEN_KEEPALIVE int wasm_get_row_length()       { return g_d.rowLength; }
EMSCRIPTEN_KEEPALIVE int wasm_get_group_size()       { return g_d.groupStructureSize; }
EMSCRIPTEN_KEEPALIVE int8_t * wasm_get_groups_ptr()  { return g_d.groupData; }
EMSCRIPTEN_KEEPALIVE int64_t wasm_get_generation()   { return g_d.generations; }
EMSCRIPTEN_KEEPALIVE int wasm_get_run_state()        { return g_d.runState; }
EMSCRIPTEN_KEEPALIVE int wasm_get_inhabited_groups() { return g_d.inhabitedGroups; }
EMSCRIPTEN_KEEPALIVE int wasm_get_altruist_groups()  { return g_d.altruistGroups; }
EMSCRIPTEN_KEEPALIVE float wasm_get_altruism_fraction() { return g_d.geneFraction[0]; }
EMSCRIPTEN_KEEPALIVE int64_t wasm_get_total_population() { return g_d.totalPopulation; }

// Group field offsets, so JS can decode IslandGroup from groupData without binding details.
// Returns offset (in bytes) to the population (`nn`) field.
EMSCRIPTEN_KEEPALIVE int wasm_get_pop_offset() { return g_d.modelPopOffset; }

// Return offset of the altruism mutant gene count, found via the model's GroupFieldDescriptor list.
// Returns -1 if not found.
EMSCRIPTEN_KEEPALIVE int wasm_get_altruism_offset() {
    const GroupFieldDescriptor * f = g_d.currentModel ? g_d.currentModel->groupFields : nullptr;
    if (!f) return -1;
    for (; f->type != 0; f++) {
        if (f->type == 4 && f->graphics == 10) return f->offset;  // mutant gene at primary locus
    }
    return -1;
}

}  // extern "C"
