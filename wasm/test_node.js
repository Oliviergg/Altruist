// Run the WASM build under Node.js and print the same stats lines as
// test_native.cpp. Compare with `diff` to verify WASM == native.
//
// Run:  make -C wasm test-node
const fs = require('fs');
const path = require('path');
const here = path.resolve(__dirname, '..', 'build', 'wasm');
process.chdir(here);

const wasmBytes = fs.readFileSync(path.join(here, 'altruist.wasm'));
const AltruistModule = require(path.join(here, 'altruist.js'));

(async () => {
  const Module = await AltruistModule({
    instantiateWasm: (imports, cb) =>
      WebAssembly.instantiate(wasmBytes, imports).then(r => cb(r.instance, r.module)) || {},
  });
  const fn = (n, ret, args) => Module.cwrap(n, ret, args);
  const init = fn('wasm_init', 'number', ['number','number','number']);
  const step = fn('wasm_step', 'number', ['number']);
  const cnt  = fn('wasm_get_model_count', 'number', []);
  const name = fn('wasm_get_model_name', 'string', ['number']);
  const find = fn('wasm_find_model', 'number', ['string']);
  const maxI = fn('wasm_get_max_islands', 'number', []);
  const inh  = fn('wasm_get_inhabited_groups', 'number', []);
  const frac = fn('wasm_get_altruism_fraction', 'number', []);
  const pop  = fn('wasm_get_total_population', 'number', []);
  const gen  = fn('wasm_get_generation', 'number', []);

  console.log(`models=${cnt()}`);
  for (let i = 0; i < cnt(); i++) console.log(`model[${i}]=${name(i)}`);
  const idx = find('Island model');
  const rc = init(idx, 42, 16);
  console.log(`init rc=${rc} maxIslands=${maxI()}`);
  for (let g = 0; g < 10; g++) {
    step(20);
    console.log(`gen=${gen()} pop=${pop()} inhabited=${inh()} altruism=${frac().toFixed(6)}`);
  }
})().catch(e => { console.error(e); process.exit(1); });
