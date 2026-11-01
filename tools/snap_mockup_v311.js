// Reference screenshots of the v3.10 / v3.11 features for the plugin comparison: OSC 2 EXTRA, the EXT IN panel, the melody generator panel, undo and redo buttons.
// Usage: JP_MOCKUP=v3.11 node tools/snap_mockup_v311.js <outdir>      Writes v311_extra.png v311_extin.png v311_gen.png v311_undo.png v311_redo.png
if (!process.env.JP_MOCKUP) process.env.JP_MOCKUP = 'v3.11';
const path = require('path'), fs = require('fs');
const { open } = require('./mockup_page.js');
(async () => {
  const out = path.resolve(process.argv[2] || 'snap-mock-v311'); fs.mkdirSync(out, { recursive: true });
  const m = await open(); const p = m.page;
  const errs = []; p.on('pageerror', e => errs.push(String(e))); p.on('console', c => { if (c.type() === 'error') errs.push(c.text()); });
  const ev = (f, a) => p.evaluate(f, a);
  const shot = async (name) => {
    if (name !== 'v311_redo') await ev(() => { document.querySelector('#msg').textContent = ''; }); await p.waitForTimeout(150);
    const box = await ev(() => { const r = document.querySelector('#win').getBoundingClientRect(); return { x: r.x, y: r.y, w: r.width, h: r.height }; });
    await p.screenshot({ path: path.join(out, name + '.png'), clip: { x: box.x, y: box.y, width: box.w, height: box.h } });
    console.log(name, box.w + 'x' + box.h);
  };
  const reset = async () => { await ev(() => { const J = window.__JP; __JPext.close(); try { __JPplayer.closeGen(); } catch (e) {} J.setP('o2w', 0, false); J.setP('o2ss', 0, false); J.setP('o2ext', 0, false); J.setP('player', 0, false); __JPhist.clear(); }); };
  await ev(() => window.__JP.showPage('sound')); await reset();
  await ev(() => { const J = window.__JP; J.setP('o2ss', 1, false); }); await shot('v311_extra'); await ev(() => { window.__JP.setP('o2w', 4, false); }); await shot('v311_extra_ss'); await reset();
    await ev(() => { window.__JP.setP('o2ext', 1, false); __JPext.open(); }); await shot('v311_extin'); await reset();
  await ev(() => { const J = window.__JP; J.setP('player', 1, false); const pl = window.__JPplayer; pl.select(1); pl.setGen({ seed: 12345, key: 9, mode: 0, style: 1 }); pl.demo(21); pl.openGen(); }); await shot('v311_gen'); await reset();
  await ev(() => { window.__JP.setP('o2r', 5, true); }); await shot('v311_undo');
  await ev(() => { __JPhist.undo(); }); await shot('v311_redo'); await ev(() => { window.__JP.setP('o2r', 0, false); __JPhist.clear(); });
  if (errs.length) { console.log('ERRORS', errs); process.exitCode = 1; }
  await m.close();
})();
