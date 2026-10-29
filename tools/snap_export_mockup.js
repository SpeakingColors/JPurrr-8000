// Reference screenshots of the Export dialog of the mockup (v3.6) for the plugin comparison.
// Usage: JP_MOCKUP=v3.6 node tools/snap_export_mockup.js <outdir>
// States (same names as the plugin snapshot): export (JSON, this sound), export_8080, export_8000, export_slot_8080 (slot B27), export_bank_8080, export_bank_8000, export_bank_json, export_warn_8000 (a sound with warnings)
const path = require('path'), fs = require('fs');
const { open } = require('./mockup_page.js');
(async () => {
  const out = path.resolve(process.argv[2] || 'snap-export-mock'); fs.mkdirSync(out, { recursive: true });
  const m = await open(); const p = m.page;
  const shot = async (name) => {
    await p.waitForTimeout(100);
    const box = await p.evaluate(() => { const r = document.querySelector('#win').getBoundingClientRect(); return { x: r.x, y: r.y, w: r.width, h: r.height }; });
    await p.screenshot({ path: path.join(out, name + '.png'), clip: { x: box.x, y: box.y, width: box.w, height: box.h } });
    console.log(name, box.w + 'x' + box.h);
  };
  // the choices are set in the mockup settings (what the dialog remembers), the others at their defaults: JSON, edit buffer, device 17
  const show = async (name, o = {}) => {
    await p.evaluate(o => { const S = window.__JP.S; S.xfmt = o.f || 'json'; S.xdev = o.d || 17; S.xdest = o.s || 'edit'; }, o);
    await p.evaluate(() => window.__JP.exportDlg());
    await p.waitForTimeout(80);
    if (o.w) await p.selectOption('#xw', o.w);
    await shot(name);
    await p.evaluate(() => window.__JP.closeDlg());
  };
  await show('export');
  await show('export_8080', { f: '8080' });
  await show('export_8000', { f: '8000' });
  await show('export_slot_8080', { f: '8080', s: 'B27', d: 20 });
  await show('export_bank_8080', { f: '8080', w: 'b' });
  await show('export_bank_8000', { f: '8000', w: 'b' });
  await show('export_bank_json', { f: 'json', w: 'b' });
  // a sound with the things the SysEx formats cannot keep: FEEDBACK OSC, DISTORTION, delay type, OSC2 WIDE
  await p.evaluate(() => { const J = window.__JP; J.setP('o1w', 3, true); J.setP('xt', 12, true); J.setP('dty', 2, true); J.setP('o2wide', 1, true); });
  await show('export_warn_8000', { f: '8000' });
  await m.close();
})();
