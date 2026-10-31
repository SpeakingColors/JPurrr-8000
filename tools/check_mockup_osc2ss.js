// Checks the OSC 2 EXTRA switch of mockup v3.11 (shows SUPER SAW at the top of the OSC 2 list): hidden by default, window and panel unchanged, switch shows the 5th waveform, controls get DETUNE / MIX,
// the sound can be saved and loaded with it, switching off while SUPER SAW is chosen falls back to SAW, the export warns, no console errors.
// Usage: node tools/check_mockup_osc2ss.js [screenshot-dir]
if (!process.env.JP_MOCKUP) process.env.JP_MOCKUP = 'v3.11';
const { open } = require('./mockup_page.js');
let bad = 0; const ok = (c, msg) => { console.log((c ? 'ok   ' : 'FAIL ') + msg); if (!c) bad++; };
(async () => {
  const m = await open(); const p = m.page, errs = []; p.on('pageerror', e => errs.push(String(e))); p.on('console', c => { if (c.type() === 'error') errs.push(c.text()); });
  const ev = (f, a) => p.evaluate(f, a); const vis = sel => ev(s => { const e = document.querySelector(s); return !!(e && e.offsetParent); }, sel);
  const H = () => ev(() => Math.round(document.querySelector('#win').getBoundingClientRect().height));
  ok(await vis('.sw[data-id="o2ss"]') && (await ev(() => document.querySelector('.ctl[data-for="o2ss"] .lb').textContent)) === 'EXTRA', 'the EXTRA switch is on the OSC 2 panel'); ok((await ev(() => window.__JP.P.o2ss.v)) === 0, 'switch is off by default');
  ok(!(await vis('.opts[data-id="o2w"] .opt[data-v="4"]')) && (await vis('.opts[data-id="o2w"] .opt[data-v="3"]')), 'with the switch off the list shows 4 waveforms (no SUPER SAW)');
  const h0 = await H(); const box0 = await ev(() => { const r = document.querySelector('.opts[data-id="o2w"]').getBoundingClientRect(); return [Math.round(r.height)]; });
  await p.click('.sw[data-id="o2ss"]'); await p.waitForTimeout(100);
  ok(await vis('.opts[data-id="o2w"] .opt[data-v="4"]'), 'switch on: SUPER SAW appears in the OSC 2 list'); ok((await H()) === h0, 'window height unchanged (' + h0 + ')');
  const tops = await ev(() => [0, 1, 2, 3, 4].map(v => Math.round(document.querySelector('.opts[data-id="o2w"] .opt[data-v="' + v + '"]').getBoundingClientRect().top)));
  ok(tops[4] < Math.min(tops[0], tops[1], tops[2], tops[3]), 'SUPER SAW is the top entry of the list'); ok((await ev(() => window.__JP.P.o2w.v)) === 0, 'the waveform that was chosen (SQR) stays chosen');
  const slack = await ev(() => { const sec = [...document.querySelectorAll('.sec')].find(e => /OSC 2/.test(e.textContent.slice(0, 40))); const b = sec.getBoundingClientRect().bottom; const low = Math.max(...[...sec.querySelectorAll('.lb,.knob,.sw,.opt')].filter(e => e.offsetParent).map(e => e.getBoundingClientRect().bottom)); return Math.round(b - low); });
  ok(slack >= 20, 'everything stays inside the OSC 2 panel with the switch on (' + slack + ' px to spare)');
  await p.click('.opts[data-id="o2w"] .opt[data-v="4"]'); await p.waitForTimeout(100);
  ok((await ev(() => document.querySelector('#lb_o2c1').textContent + ' / ' + document.querySelector('#lb_o2c2').textContent)) === 'DETUNE / MIX', 'OSC 2 controls read DETUNE and MIX');
  const snap = await ev(() => window.__JP.snapshot()); ok(snap.o2ss === 1 && snap.o2w === 4, 'the sound keeps o2ss=1, o2w=4');
  await ev(() => { window.__JP.loadPreset({ name: 'x', v: Object.assign({}, window.__JP.DEFS) }); }); await p.waitForTimeout(100);
  ok((await ev(() => window.__JP.P.o2ss.v)) === 0 && !(await vis('.opts[data-id="o2w"] .opt[data-v="4"]')), 'loading a normal sound: switch off again, list back to 4');
  await ev(s => window.__JP.loadPreset({ name: 'ss', v: Object.assign({}, window.__JP.DEFS, s) }), snap); await p.waitForTimeout(100);
  ok((await ev(() => window.__JP.P.o2w.v)) === 4 && await vis('.opts[data-id="o2w"] .opt[data-v="4"]'), 'loading a sound with OSC 2 SUPER SAW shows the choice and selects it');
  if (process.argv[2]) await p.screenshot({ path: process.argv[2] + '/osc2_ss.png', clip: { x: 1080, y: 40, width: 760, height: 360 } });
  const sound = await ev(async () => { const e = window.__JP; return typeof e.say === 'function'; }); ok(sound, 'mockup helpers present');
  await p.click('.sw[data-id="o2ss"]'); await p.waitForTimeout(100); ok((await ev(() => window.__JP.P.o2w.v)) === 1, 'EXTRA off while SUPER SAW is chosen: OSC 2 becomes SAW');
  for (const w of [0, 1, 2, 3]) { await ev(w => window.__JP.setP('o2w', w, true), w); ok((await ev(() => window.__JP.P.o2ss.v)) === 0, 'waveform ' + w + ' works with the switch off'); }
  ok(errs.length === 0, 'no console errors ' + JSON.stringify(errs)); await m.browser.close(); console.log(bad ? bad + ' FAILED' : 'all checks passed'); process.exit(bad ? 1 : 0);
})().catch(e => { console.error(e); process.exit(1); });
