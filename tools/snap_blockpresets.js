// Reference screenshots of the mockup's FX block preset states (v3.3) for the plugin comparison.
// Usage: node tools/snap_blockpresets.js <outdir>   writes bp_chain bp_menu bp_save bp_replace bp_hint bp_rename bp_delete bp_msg .png (window only)
// The same states in the plugin: jpurrr_snapshot <outdir> bp_chain bp_menu ... (it prepares "Warm Drive" and "Crunchy" of the drive stack).
const path = require('path'), fs = require('fs');
const { open } = require('./mockup_page.js');
(async () => {
  const out = path.resolve(process.argv[2] || 'snap-bp'); fs.mkdirSync(out, { recursive: true });
  const m = await open(); const p = m.page;
  const shot = async (name) => {
    await p.mouse.move(1500, 20); await p.waitForTimeout(120);
    const box = await p.evaluate(() => { const r = document.querySelector('#win').getBoundingClientRect(); return { x: r.x, y: r.y, w: r.width, h: r.height }; });
    await p.screenshot({ path: path.join(out, name + '.png'), clip: { x: box.x, y: box.y, width: box.w, height: box.h } });
    console.log(name, box.w + 'x' + box.h);
  };
  const setDrive = (v) => p.evaluate(v => { for (const [k, x] of Object.entries(v)) __JP.setP(k, x, true); }, v);
  const WARM = { qdv: 30, qdx: 40, qdf: 10, qdm: 60 }, CRUNCH = { qdv: 55, qdx: 60, qdf: 20, qdm: 80 };
  const saveAs = async (name) => { await p.click('[data-mpsave="drive"]'); await p.fill('#mpn', name); await p.click('#scrim [data-b="0"]'); await p.waitForTimeout(60); };
  await p.evaluate(() => { __JP.showPage('fx'); __JP.fxSub(1); });
  await setDrive(CRUNCH); await saveAs('Crunchy');
  await setDrive(WARM); await saveAs('Warm Drive');
  await p.evaluate(() => { document.querySelector('#msg').textContent = ''; });
  const reset = async () => { await p.evaluate(() => { __JP.closeDlg(); document.querySelector('#mpmenu').hidden = true; document.querySelector('#msg').textContent = ''; }); await setDrive(WARM); await p.selectOption('.mpsel[data-mp="drive"]', 'u:Warm Drive'); await p.evaluate(() => { document.querySelector('#msg').textContent = ''; }); await p.mouse.move(1500, 20); };
  await reset(); await shot('bp_chain');
  await p.click('[data-mpmore="drive"]'); await shot('bp_menu'); await reset();
  await p.click('[data-mpsave="drive"]'); await shot('bp_save'); await reset();
  await p.click('[data-mpsave="drive"]'); await p.fill('#mpn', 'Warm Drive'); await p.click('#scrim [data-b="0"]'); await shot('bp_replace'); await reset();
  await p.click('[data-mpsave="drive"]'); await p.fill('#mpn', 'Default'); await p.click('#scrim [data-b="0"]'); await shot('bp_hint'); await reset();
  await p.click('[data-mpmore="drive"]'); await p.click('#mpmenu [data-a="ren"]'); await shot('bp_rename'); await reset();
  await p.click('[data-mpmore="drive"]'); await p.click('#mpmenu [data-a="del"]'); await shot('bp_delete'); await reset();
  await p.selectOption('.mpsel[data-mp="drive"]', 'Warm Push'); await p.click('[data-mpmore="drive"]'); await p.click('#mpmenu [data-a="ren"]'); await shot('bp_msg');
  const info = await p.evaluate(() => { const r = document.querySelector('#mpmenu'); r.hidden = false; const b = r.getBoundingClientRect(); r.hidden = true; return { w: b.width, h: b.height }; });
  console.log('mpmenu size', JSON.stringify(info));
  await m.close();
})();
