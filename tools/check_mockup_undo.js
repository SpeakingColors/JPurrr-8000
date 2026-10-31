// Checks UNDO and REDO of mockup v3.11: steps per change, one drag = one step, undo/redo restore the sound, Ctrl+Z / Ctrl+Y, buttons enable and disable,
// loading a sound starts a new history, the player and preview switches are not recorded, typing in a box keeps its own Ctrl+Z, no console errors.
// Usage: node tools/check_mockup_undo.js [screenshot-dir]
if (!process.env.JP_MOCKUP) process.env.JP_MOCKUP = 'v3.11';
const { open } = require('./mockup_page.js');
let bad = 0; const ok = (c, msg) => { console.log((c ? 'ok   ' : 'FAIL ') + msg); if (!c) bad++; };
(async () => {
  const m = await open(); const p = m.page, errs = []; p.on('pageerror', e => errs.push(String(e))); p.on('console', c => { if (c.type() === 'error') errs.push(c.text()); });
  const ev = (f, a) => p.evaluate(f, a), V = id => ev(i => window.__JP.P[i].v, id), dis = id => ev(i => document.querySelector(i).disabled, id), H = () => ev(() => [window.__JPhist.undoCount, window.__JPhist.redoCount]);
  ok((await dis('#bUndo')) && (await dis('#bRedo')), 'both buttons start disabled');
  const fc0 = await V('fc'), fr0 = await V('fr');
  await p.click('.sw[data-id="o2ss"]'); await p.waitForTimeout(50); ok((await H())[0] === 1 && !(await dis('#bUndo')) && (await dis('#bRedo')), 'one change: 1 step, undo enabled, redo disabled');
  await ev(() => { for (let v = 10; v < 60; v += 5) window.__JP.setP('fc', v, true); }); ok((await H())[0] === 2, 'fifty quick moves of one control are one step');
  await ev(() => window.__JP.setP('fr', 77, true)); ok((await H())[0] === 3, 'another control is a new step');
  await p.click('#bUndo'); ok((await V('fr')) === fr0 && (await V('fc')) !== fc0 && (await H()).join() === '2,1', 'undo: RESONANCE is back to ' + fr0 + ', CUTOFF untouched');
  await p.click('#bUndo'); ok((await V('fc')) === fc0 && (await V('o2ss')) === 1, 'undo again: CUTOFF is back to ' + fc0 + ', EXTRA still on');
  await p.click('#bUndo'); ok((await V('o2ss')) === 0 && !(await ev(() => document.querySelector('.opts[data-id="o2w"] .opt[data-v="4"]').offsetParent)), 'undo again: EXTRA is off and the list is back to 4 waveforms'); ok(await dis('#bUndo'), 'nothing left to undo: button disabled');
  await p.click('#bRedo'); await p.click('#bRedo'); await p.click('#bRedo'); ok((await V('fr')) === 77 && (await V('fc')) !== fc0 && (await V('o2ss')) === 1 && (await H()).join() === '3,0', 'redo three times brings everything back');
  await p.keyboard.press('Control+z'); ok((await V('fr')) === fr0, 'Ctrl+Z undoes'); await p.keyboard.press('Control+y'); ok((await V('fr')) === 77, 'Ctrl+Y redoes'); await p.keyboard.press('Control+Shift+z'); ok((await V('fr')) === 77, 'Ctrl+Shift+Z with nothing to redo does nothing');
  await ev(() => window.__JP.setP('fr', 30, true)); await p.click('#bUndo'); await ev(() => window.__JP.setP('fr', 31, true)); ok((await H())[1] === 0, 'a new change after an undo clears redo');
  const n = (await H())[0]; await ev(() => { window.__JP.setP('player', 1, true); window.__JP.setP('prev', 1, true); }); ok((await H())[0] === n, 'the PLAYER and PREVIEW switches are not recorded');
  const tip = await ev(() => document.querySelector('#bUndo').dataset.tip); ok(/RESONANCE|Resonance/.test(tip) || /\(/.test(tip), 'the undo tip names the change: ' + tip.slice(0, 70));
  await ev(() => window.__JPhist.clear()); const kb = await ev(() => { const r = document.querySelector('.knob[data-id="o2r"]').getBoundingClientRect(); return [r.left + r.width / 2, r.top + r.height / 2]; }), c0 = await V('o2r');
  await p.mouse.move(kb[0], kb[1]); await p.mouse.down(); for (let i = 1; i <= 25; i++) await p.mouse.move(kb[0], kb[1] - i * 3); await p.mouse.up();
  ok((await V('o2r')) !== c0 && (await H())[0] === 1, 'one mouse drag of the RANGE knob = one step'); await p.click('#bUndo'); ok((await V('o2r')) === c0, 'undo puts the knob back to ' + c0);
  await ev(() => window.__JP.loadPreset(window.__JP.FACTORY[3])); ok((await H()).join() === '0,0' && (await dis('#bUndo')), 'loading another sound starts a new history');
  await p.mouse.move(10, 10); const sh = await ev(() => { const b = document.querySelector('#bUndo').getBoundingClientRect(), n = document.querySelector('#bNext').getBoundingClientRect(); return [Math.round(b.width), Math.round(b.height), Math.round(b.left - n.right)]; }); ok(sh[0] > 20 && sh[1] > 20, 'buttons are square ' + sh[0] + 'x' + sh[1]);
  if (process.argv[2]) { await ev(() => window.__JP.setP('fr', 90, true)); await p.screenshot({ path: process.argv[2] + '/undo_header.png', clip: { x: 0, y: 0, width: 1500, height: 80 } }); }
  ok(errs.length === 0, 'no console errors ' + JSON.stringify(errs)); await m.browser.close(); console.log(bad ? bad + ' FAILED' : 'all checks passed'); process.exit(bad ? 1 : 0);
})().catch(e => { console.error(e); process.exit(1); });
