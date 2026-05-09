// WASM entry point for the Altruist simulation POC.
// Provides the globals and constructors normally defined in altruist.cpp,
// and exposes a small C API to JavaScript via emscripten.
#include "stdafx.h"
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#else
#define EMSCRIPTEN_KEEPALIVE __attribute__((used))
#endif

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
    // mirrors a subset of Altruist::initializeData() — only what the simulation
    // actually reads at runtime. JS may override any of these via wasm_set_*.
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
    d->murate[1][0] = 0;    d->murate[1][1] = 0;
    d->murate[2][0] = 0;    d->murate[2][1] = 0;

    d->totArea     = 1024;
    d->maxIslands  = 256;                    // 16x16 grid
    d->rowLength   = 16;
    d->numRows     = 16;
    d->minGroupSize = 2;
    d->colonySize   = 4;
    d->territorySizeMax = 64;
    d->territorySizeMin = 16;
    d->carryingCapacity[0] = 100.f;
    d->carryingCapacity[1] = 100.f;
    d->carryingCapacityStandardDeviation = 0.f;

    // fitness array: [egoist among egoists, egoist among altruists,
    //                 altruist among egoists, altruist among altruists, ...]
    d->fit[0] = 1.0f;
    d->fit[1] = 1.2f;
    d->fit[2] = 0.8f;
    d->fit[3] = 1.0f;
    for (int i = 4; i < 8; i++) d->fit[i] = 1.0f;
    for (int i = 0; i < 4; i++) d->fit2[i] = 1.f;
    d->growthRate = 2.0f;
    d->selectionModel = 0;       // fecundity selection

    d->extinctionRate[0] = 0.05f;
    d->extinctionRate[1] = 0.02f;
    d->extinctionRate[2] = 0.02f;
    d->extinctionRate[3] = 0.005f;
    d->surviv = 0.f;
    d->warIntensity = 0.5f;
    d->fitfunc = 2;              // linear
    d->groupFitCurvature = 1.f;
    d->haystackPeriod = 5;
    d->mixingPeriod   = 5;
    d->leaderAdvantage = 1.f;
    d->leaderSelection = 0.f;

    d->migrationRate[0] = 0.05f;
    d->migrationRate[1] = 0.05f;
    d->emigrationPattern    = 0;
    d->immigrationPattern   = 0;  // common pool
    d->colonizationPattern  = 0;
    d->migrationTopology    = 2;  // quadratic, 4 neighbours

    d->minimumGenerations = 0;
    d->maximumGenerations = 1000000;
    d->stopCriterion       = 0;
    d->stopCriterionDegree = 0.99f;
    d->seed = 1;
}


// ---------------------------------------------------------------------------
// Parameter metadata table.
// Used by JS to render forms and call typed setters/getters by name.
// ---------------------------------------------------------------------------

enum : int { P_INT = 1, P_FLOAT = 2, P_BOOL = 3 };

struct ParamEntry {
    const char * section;        // "Geography", "Loci", ...
    const char * name;           // matches what JS passes to wasm_set_*/wasm_get_*
    const char * label;          // human-readable label for the form
    int          type;           // P_INT / P_FLOAT / P_BOOL
    size_t       offset;         // offsetof into AltruData
    int          arraySize;      // 1 for scalar, >1 for array (2D arrays flattened)
};

static const ParamEntry paramTable[] = {
    // Geography & migration
    {"Geography", "maxIslands",          "Number of groups (max)",           P_INT,   offsetof(AltruData, maxIslands), 1},
    {"Geography", "totArea",             "Total habitat area",               P_INT,   offsetof(AltruData, totArea), 1},
    {"Geography", "carryingCapacity",    "Carrying capacity (egoist/altruist)", P_FLOAT, offsetof(AltruData, carryingCapacity), 2},
    {"Geography", "carryingCapacityStandardDeviation", "Capacity std. dev.", P_FLOAT, offsetof(AltruData, carryingCapacityStandardDeviation), 1},
    {"Geography", "minGroupSize",        "Minimum group size",               P_INT,   offsetof(AltruData, minGroupSize), 1},
    {"Geography", "colonySize",          "Recolonisation group size",        P_INT,   offsetof(AltruData, colonySize), 1},
    {"Geography", "territorySizeMax",    "Max territory area",               P_INT,   offsetof(AltruData, territorySizeMax), 1},
    {"Geography", "territorySizeMin",    "Min territory area",               P_INT,   offsetof(AltruData, territorySizeMin), 1},
    {"Geography", "migrationRate",       "Migration rate (normal/endogamy)", P_FLOAT, offsetof(AltruData, migrationRate), 2},
    {"Geography", "migrationTopology",   "Migration topology (0–7)",         P_INT,   offsetof(AltruData, migrationTopology), 1},
    {"Geography", "emigrationPattern",   "Emigration pattern",               P_INT,   offsetof(AltruData, emigrationPattern), 1},
    {"Geography", "immigrationPattern",  "Immigration pattern",              P_INT,   offsetof(AltruData, immigrationPattern), 1},
    {"Geography", "colonizationPattern", "Colonisation pattern",             P_INT,   offsetof(AltruData, colonizationPattern), 1},

    // Loci & mutation
    {"Loci", "locusUsed",                "Loci used (altruism/endogamy/conformity)", P_BOOL, offsetof(AltruData, locusUsed), maxLoci},
    {"Loci", "fg0",                      "Initial mutant fraction per locus",         P_FLOAT, offsetof(AltruData, fg0), maxLoci},
    {"Loci", "murate",                   "Mutation rates [locus][forward/back]",      P_FLOAT, offsetof(AltruData, murate), maxLoci * 2},

    // Individual fitness
    {"Fitness", "fit",                   "Individual fitness fit[0..7]",     P_FLOAT, offsetof(AltruData, fit), 8},
    {"Fitness", "growthRate",            "Average fertility (growth rate)",  P_FLOAT, offsetof(AltruData, growthRate), 1},
    {"Fitness", "selectionModel",        "Selection model (0=fecundity)",    P_INT,   offsetof(AltruData, selectionModel), 1},

    // Group properties
    {"Group", "extinctionRate",          "Group extinction rate (4 entries)", P_FLOAT, offsetof(AltruData, extinctionRate), 4},
    {"Group", "surviv",                  "Survival rate after extinction",    P_FLOAT, offsetof(AltruData, surviv), 1},
    {"Group", "warIntensity",            "Territorial war intensity",         P_FLOAT, offsetof(AltruData, warIntensity), 1},
    {"Group", "fitfunc",                 "Group fitness function (0–4)",      P_INT,   offsetof(AltruData, fitfunc), 1},
    {"Group", "groupFitCurvature",       "Group fitness curvature",           P_FLOAT, offsetof(AltruData, groupFitCurvature), 1},
    {"Group", "haystackPeriod",          "Haystack period",                   P_INT,   offsetof(AltruData, haystackPeriod), 1},
    {"Group", "mixingPeriod",            "Mixing period",                     P_INT,   offsetof(AltruData, mixingPeriod), 1},
    {"Group", "leaderAdvantage",         "Leader advantage (regality)",       P_FLOAT, offsetof(AltruData, leaderAdvantage), 1},
    {"Group", "leaderSelection",         "Leader selection bias (regality)",  P_FLOAT, offsetof(AltruData, leaderSelection), 1},

    // Run control
    {"Run", "seed",                      "Random seed",                       P_INT,   offsetof(AltruData, seed), 1},
    {"Run", "minimumGenerations",        "Minimum generations",               P_INT,   offsetof(AltruData, minimumGenerations), 1},
    {"Run", "maximumGenerations",        "Maximum generations",               P_INT,   offsetof(AltruData, maximumGenerations), 1},
    {"Run", "stopCriterion",             "Stop criterion (0=none)",           P_INT,   offsetof(AltruData, stopCriterion), 1},
    {"Run", "stopCriterionDegree",       "Stop criterion degree (0..1)",      P_FLOAT, offsetof(AltruData, stopCriterionDegree), 1},
};
static const int paramTableSize = (int)(sizeof(paramTable)/sizeof(paramTable[0]));

static const ParamEntry * findParam(const char * name) {
    for (int i = 0; i < paramTableSize; i++) {
        if (strcmp(paramTable[i].name, name) == 0) return &paramTable[i];
    }
    return nullptr;
}


extern "C" {

// ---- Model registry --------------------------------------------------------

EMSCRIPTEN_KEEPALIVE int wasm_get_model_count() { models.sort(); return (int)models.getNum(); }
EMSCRIPTEN_KEEPALIVE const char * wasm_get_model_name(int i) {
    models.sort();
    ModelDescriptor * m = models.getModel((uint32_t)i);
    return m ? m->name : nullptr;
}
EMSCRIPTEN_KEEPALIVE const char * wasm_get_model_description(int i) {
    models.sort();
    ModelDescriptor * m = models.getModel((uint32_t)i);
    return m ? m->description : nullptr;
}
EMSCRIPTEN_KEEPALIVE int wasm_find_model(const char * name) {
    models.sort();
    for (uint32_t i = 0; i < models.getNum(); i++) {
        if (strcmp(models.getModel(i)->name, name) == 0) return (int)i;
    }
    return -1;
}

// ---- Parameter table introspection ----------------------------------------

EMSCRIPTEN_KEEPALIVE int          wasm_param_count()          { return paramTableSize; }
EMSCRIPTEN_KEEPALIVE const char * wasm_param_section(int i)   { return (i>=0&&i<paramTableSize)?paramTable[i].section:nullptr; }
EMSCRIPTEN_KEEPALIVE const char * wasm_param_name(int i)      { return (i>=0&&i<paramTableSize)?paramTable[i].name:nullptr; }
EMSCRIPTEN_KEEPALIVE const char * wasm_param_label(int i)     { return (i>=0&&i<paramTableSize)?paramTable[i].label:nullptr; }
EMSCRIPTEN_KEEPALIVE int          wasm_param_type(int i)      { return (i>=0&&i<paramTableSize)?paramTable[i].type:0; }
EMSCRIPTEN_KEEPALIVE int          wasm_param_array_size(int i){ return (i>=0&&i<paramTableSize)?paramTable[i].arraySize:0; }

// ---- Typed setters / getters by parameter name ----------------------------
// `idx` is ignored for scalars; for arrays it is the element index.
// All return -1 / NaN-equivalent on unknown name or out-of-range idx.

EMSCRIPTEN_KEEPALIVE int wasm_set_int(const char * name, int idx, int value) {
    const ParamEntry * p = findParam(name);
    if (!p || (p->type != P_INT) || idx < 0 || idx >= p->arraySize) return -1;
    *(int32_t*)((int8_t*)&g_d + p->offset + idx * sizeof(int32_t)) = value;
    return 0;
}
EMSCRIPTEN_KEEPALIVE int wasm_get_int(const char * name, int idx) {
    const ParamEntry * p = findParam(name);
    if (!p || (p->type != P_INT) || idx < 0 || idx >= p->arraySize) return 0;
    return *(int32_t*)((int8_t*)&g_d + p->offset + idx * sizeof(int32_t));
}
EMSCRIPTEN_KEEPALIVE int wasm_set_float(const char * name, int idx, float value) {
    const ParamEntry * p = findParam(name);
    if (!p || (p->type != P_FLOAT) || idx < 0 || idx >= p->arraySize) return -1;
    *(float*)((int8_t*)&g_d + p->offset + idx * sizeof(float)) = value;
    return 0;
}
EMSCRIPTEN_KEEPALIVE float wasm_get_float(const char * name, int idx) {
    const ParamEntry * p = findParam(name);
    if (!p || (p->type != P_FLOAT) || idx < 0 || idx >= p->arraySize) return 0.f;
    return *(float*)((int8_t*)&g_d + p->offset + idx * sizeof(float));
}
EMSCRIPTEN_KEEPALIVE int wasm_set_bool(const char * name, int idx, int value) {
    const ParamEntry * p = findParam(name);
    if (!p || (p->type != P_BOOL) || idx < 0 || idx >= p->arraySize) return -1;
    *((bool*)((int8_t*)&g_d + p->offset) + idx) = (value != 0);
    return 0;
}
EMSCRIPTEN_KEEPALIVE int wasm_get_bool(const char * name, int idx) {
    const ParamEntry * p = findParam(name);
    if (!p || (p->type != P_BOOL) || idx < 0 || idx >= p->arraySize) return 0;
    return *((bool*)((int8_t*)&g_d + p->offset) + idx) ? 1 : 0;
}

// ---- Init / step / state --------------------------------------------------

// Reset all parameters to the demo defaults. Called once on page load
// so the form can populate itself.
EMSCRIPTEN_KEEPALIVE void wasm_reset_defaults() {
    setReasonableDefaults(&g_d);
}

// Run model init + state_start using the *current* AltruData.
// JS should set parameters via wasm_set_* before calling this.
EMSCRIPTEN_KEEPALIVE int wasm_init_model(int iModel, int gridSide) {
    models.sort();
    if (gridSide > 0) {
        g_d.maxIslands = gridSide * gridSide;
        g_d.rowLength  = gridSide;
        g_d.numRows    = gridSide;
    }
    g_rng.randomInit(g_d.seed);
    g_d.iModel = iModel;
    g_d.currentModel = models.getModel((uint32_t)iModel);
    if (g_d.currentModel == nullptr) return -1;

    g_d.currentModel->initFunction(&g_d, model_initialize);

    g_d.groupStructureSize = g_d.modelGroupStructureSize;
    int64_t bytes = (int64_t)(g_d.maxIslands + 1) * g_d.groupStructureSize;
    if (g_d.groupData) { delete[] g_d.groupData; g_d.groupData = nullptr; }
    g_d.groupData = new int8_t[bytes]();
    g_d.bufferSize = bytes;

    g_d.runState   = state_start;
    g_d.generations = 0;
    g_d.currentModel->generationFunction(&g_d, state_start);
    return 0;
}

// Convenience: reset defaults, override seed + grid, then init the model.
// Used by the test harness so its output stays deterministic.
EMSCRIPTEN_KEEPALIVE int wasm_init(int iModel, int seed, int gridSide) {
    setReasonableDefaults(&g_d);
    if (seed != 0) g_d.seed = seed;
    return wasm_init_model(iModel, gridSide);
}

EMSCRIPTEN_KEEPALIVE int wasm_step(int n) {
    if (g_d.currentModel == nullptr) return -1;
    for (int i = 0; i < n && g_d.runState == state_run; i++) {
        g_d.currentModel->generationFunction(&g_d, state_run);
        g_d.generations++;
    }
    return (int)g_d.generations;
}

// ---- Live state readouts --------------------------------------------------

EMSCRIPTEN_KEEPALIVE int      wasm_get_max_islands()      { return g_d.maxIslands; }
EMSCRIPTEN_KEEPALIVE int      wasm_get_row_length()       { return g_d.rowLength; }
EMSCRIPTEN_KEEPALIVE int      wasm_get_group_size()       { return g_d.groupStructureSize; }
EMSCRIPTEN_KEEPALIVE int8_t * wasm_get_groups_ptr()       { return g_d.groupData; }
EMSCRIPTEN_KEEPALIVE int64_t  wasm_get_generation()       { return g_d.generations; }
EMSCRIPTEN_KEEPALIVE int      wasm_get_run_state()        { return g_d.runState; }
EMSCRIPTEN_KEEPALIVE int      wasm_get_inhabited_groups() { return g_d.inhabitedGroups; }
EMSCRIPTEN_KEEPALIVE int      wasm_get_altruist_groups()  { return g_d.altruistGroups; }
EMSCRIPTEN_KEEPALIVE float    wasm_get_altruism_fraction(){ return g_d.geneFraction[0]; }
EMSCRIPTEN_KEEPALIVE int64_t  wasm_get_total_population() { return g_d.totalPopulation; }
EMSCRIPTEN_KEEPALIVE int      wasm_get_pop_offset()       { return g_d.modelPopOffset; }

EMSCRIPTEN_KEEPALIVE int wasm_get_altruism_offset() {
    const GroupFieldDescriptor * f = g_d.currentModel ? g_d.currentModel->groupFields : nullptr;
    if (!f) return -1;
    for (; f->type != 0; f++) {
        if (f->type == 4 && f->graphics == 10) return f->offset;
    }
    return -1;
}

}  // extern "C"
