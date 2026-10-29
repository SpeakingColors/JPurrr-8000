// Reference screenshots of the mockup's keyboard strip states, for the plugin comparison (plugin: jpurrr_snapshot <dir> kbd_...).
// Usage: node tools/snap_mockup_kbd.js <outdir> [state...]
// State names: kbd_<sound|chain|master>_n60_n64_b40_m90_dragb_focusm  (nNN = key held, bNN = BEND value -64..63, mNN = MOD 0..127,
// dragb / dragm = that wheel looks dragged, focusb / focusm = that wheel has keyboard focus).
const path = require('path'), fs = require('fs');
const { open } = require('./mockup_page.js');
const DEFAULT = ['kbd_sound_n60_n61_n64_n66_n72', 'kbd_sound_b40_m90', 'kbd_sound_b63_m127', 'kbd_chain_n48_n55_n59_b-30_m64', 'kbd_master_n36_n96_dragb_b-64_m20',
  'kbd_sound_focusm_m40', 'kbd_sound_focusb', 'kbd_sound_dragm_m100', 'kbd_sound_b-64'];
(async () => {
  const out = path.resolve(process.argv[2] || 'snap-mock-kbd'); fs.mkdirSync(out, { recursive: true });
  const states = process.argv.length > 3 ? process.argv.slice(3) : DEFAULT;
  const m = await open(); const p = m.page;
  for (const s of states) {
    const t = s.split('_').filter(Boolean);
    const spec = { page: t[1], notes: [], b: 0, m: 0, drag: '', focus: '' };
    for (const k of t.slice(2)) {
      if (/^n\d+$/.test(k)) spec.notes.push(+k.slice(1)); else if (/^b-?\d+$/.test(k)) spec.b = +k.slice(1); else if (/^m\d+$/.test(k)) spec.m = +k.slice(1);
      else if (k === 'dragb' || k === 'dragm') spec.drag = k; else if (k === 'focusb' || k === 'focusm') spec.focus = k;
    }
    await p.evaluate(sp => {
      const J = window.__JP;
      if (sp.page === 'sound') J.showPage('sound'); else { J.showPage('fx'); J.fxSub(sp.page === 'chain' ? 1 : 2); }
      sp.notes.forEach(n => document.querySelector('#keys [data-n="' + n + '"]').classList.add('on'));
      J.setP('bend', sp.b, false); J.setP('mod', sp.m, false);
      if (sp.drag) document.querySelector(sp.drag === 'dragb' ? '#wBend' : '#wMod').classList.add('drag');
    }, spec);
    if (spec.focus) { await p.keyboard.press('Shift'); await p.evaluate(f => document.querySelector(f === 'focusb' ? '#wBend' : '#wMod').focus(), spec.focus); }
    await p.waitForTimeout(350);
    const box = await p.evaluate(() => { const r = document.querySelector('#win').getBoundingClientRect(); return { x: r.x, y: r.y, w: r.width, h: r.height }; });
    await p.screenshot({ path: path.join(out, s + '.png'), clip: { x: box.x, y: box.y, width: box.w, height: box.h } });
    console.log(s, box.w + 'x' + box.h);
    await p.evaluate(() => {
      document.querySelectorAll('#keys .on').forEach(e => e.classList.remove('on'));
      document.querySelectorAll('.whl.drag').forEach(e => e.classList.remove('drag'));
      const J = window.__JP; J.setP('bend', 0, false); J.setP('mod', 0, false); if (document.activeElement) document.activeElement.blur();
    });
  }
  await m.close();
})();
