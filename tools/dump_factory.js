// Writes data/factory.json (the factory sounds, with the MIX READY chain values of their type) from the FACTORY list of a mockup.
// The mockup is where the sounds are edited; data/params.json is NOT touched (it carries the manual's standard settings, see docs/engine-check-jp8000.md).
// Usage: node tools/dump_factory.js [mockup.html] [out.json]     (default: docs/mockups/JPurrr-8000-mockup-v3.7.html -> data/factory.json)
// Then: python3 tools/gen_params.py   (regenerates core/src/factory_gen.cpp)
const { chromium } = require('playwright'); const fs = require('fs'), path = require('path');
(async () => {
  const root = path.resolve(__dirname, '..');
  const src = path.resolve(process.argv[2] || path.join(root, 'docs/mockups/JPurrr-8000-mockup-v3.7.html'));
  const out = path.resolve(process.argv[3] || path.join(root, 'data/factory.json'));
  const b = await chromium.launch(); const p = await b.newPage({ viewport: { width: 2920, height: 1300 } });
  await p.goto('file://' + src); await p.waitForTimeout(900);
  const presets = await p.evaluate(() => { const J = window.__JP; return J.FACTORY.map(f => ({ name: f.name, type: f.type, genres: f.genres, slot: f.slot, start: f.start, v: Object.assign({}, f.type ? J.qvals(f.type) : {}, f.v) })); });
  fs.writeFileSync(out, JSON.stringify({ format: 1, presets }, null, 1) + (fs.readFileSync(out, 'utf8').endsWith('\n') ? '\n' : ''));
  console.log(presets.length, 'factory sounds ->', path.relative(root, out)); await b.close();
})();
