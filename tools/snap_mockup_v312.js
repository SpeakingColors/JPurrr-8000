// Reference screenshots of the v3.12 feature for the plugin comparison: the SUPER SAW TYPE row (SW 2 chosen; dimmed when OSC 1 is not SUPER SAW).
// Usage: JP_MOCKUP=v3.12 node tools/snap_mockup_v312.js <outdir>      Writes v312_sw.png v312_sw_dim.png
if (!process.env.JP_MOCKUP) process.env.JP_MOCKUP = 'v3.12';
const path = require('path'), fs = require('fs');
const { open } = require('./mockup_page.js');
(async () => {
  const out = path.resolve(process.argv[2] || 'snap-mock-v312'); fs.mkdirSync(out, { recursive: true });
  const m = await open(); const p = m.page; const ev = (f, a) => p.evaluate(f, a);
  const shot = async (name) => {
    await ev(() => { document.querySelector('#msg').textContent = ''; }); await p.waitForTimeout(250);
    const box = await ev(() => { const r = document.querySelector('#win').getBoundingClientRect(); return { x: r.x, y: r.y, width: r.width, height: r.height }; });
    await p.screenshot({ path: path.join(out, name + '.png'), clip: { x: box.x, y: box.y, width: box.width, height: box.height } }); console.log(name, box.width + 'x' + box.height);
  };
  await ev(() => { const J = window.__JP; J.showPage('sound'); J.setP('o1w', 0, false); J.setP('o1sw', 1, false); }); await shot('v312_sw');
  await ev(() => { window.__JP.setP('o1w', 1, false); }); await shot('v312_sw_dim');
  await ev(() => { window.__JP.setP('o1w', 0, false); window.__JP.setP('o1sw', 0, false); });
  await m.close();
})();
