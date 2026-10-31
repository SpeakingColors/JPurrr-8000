// Checks the EXT IN panel of mockup v3.11: the button in the top bar opens it, the OSC 2 SOURCE chips and the EXT switch of OSC 2 are the same setting,
// the level box works and is not saved with the sound, Esc and a click outside close it, undo covers the EXT switch, no console errors.
// Usage: node tools/check_mockup_extin.js [screenshot-dir]
if (!process.env.JP_MOCKUP) process.env.JP_MOCKUP = 'v3.11';
const { open } = require('./mockup_page.js');
let bad = 0; const ok = (c, msg) => { console.log((c ? 'ok   ' : 'FAIL ') + msg); if (!c) bad++; };
(async () => {
  const m = await open(); const p = m.page, errs = []; p.on('pageerror', e => errs.push(String(e))); p.on('console', c => { if (c.type() === 'error') errs.push(c.text()); });
  const ev = (f, a) => p.evaluate(f, a), V = id => ev(i => window.__JP.P[i].v, id), isOpen = () => ev(() => window.__JPext.isOpen);
  const H = () => ev(() => Math.round(document.querySelector('#win').getBoundingClientRect().height)); const h0 = await H();
  ok(!(await isOpen()) && await ev(() => !!document.querySelector('#bExt').offsetParent), 'EXT IN button is in the top bar, panel closed');
  await p.click('#bExt'); await p.waitForTimeout(150); ok(await isOpen(), 'click opens the panel'); ok((await H()) === h0, 'window height unchanged');
  const box = await ev(() => { const r = document.querySelector('#extP').getBoundingClientRect(), w = document.querySelector('#win').getBoundingClientRect(); return [r.left >= w.left, r.right <= w.right, r.top > 0]; }); ok(box.every(Boolean), 'panel is inside the window');
  await p.click('#exSrc .chip[data-v="1"]'); ok((await V('o2ext')) === 1, 'EXTERNAL chip sets EXT on');
  ok(await ev(() => document.querySelector('#bExt').classList.contains('on')), 'the button LED lights while EXT is on');
  await p.keyboard.press('Escape'); ok(!(await isOpen()), 'Esc closes the panel');
  await p.click('.sw[data-id="o2ext"]'); ok((await V('o2ext')) === 0, 'the EXT switch of OSC 2 turns it off again'); await p.click('#bExt'); ok(await ev(() => document.querySelector('#exSrc .chip[data-v="0"]').classList.contains('on')), 'the chips follow the OSC 2 switch');
  await p.click('#exSrc .chip[data-v="1"]'); await p.click('#bUndo'); ok((await V('o2ext')) === 0, 'undo takes EXT back off');
  await p.click('#bExt'); await ev(() => document.querySelector('#exLvl').focus()); await p.keyboard.press('Control+A'); await p.keyboard.type('+6'); await p.keyboard.press('Enter'); ok((await ev(() => document.querySelector('#exLvl').value)) === '+6 dB', 'LEVEL accepts +6 dB');
  const snap = await ev(() => JSON.stringify(window.__JP.snapshot())); ok(!/exg/.test(snap), 'the level is not part of the sound');
  ok(await ev(() => document.querySelector('#exIn .chip[data-v="0"]').classList.contains('on')), 'SOURCE starts on SIDECHAIN');
  await p.click('#exIn .chip[data-v="1"]'); ok(await ev(() => document.querySelector('#exIn .chip[data-v="1"]').classList.contains('on')), 'MAIN INPUT can be chosen');
  ok(!/exin/.test(await ev(() => JSON.stringify(window.__JP.snapshot()))), 'the source is not part of the sound');
  await p.click('#exIn .chip[data-v="0"]');
  await p.mouse.click(400, 600); ok(!(await isOpen()), 'a click outside closes the panel');
  await p.click('#bExt'); if (process.argv[2]) await p.screenshot({ path: process.argv[2] + '/extin.png', clip: { x: 0, y: 0, width: 1500, height: 520 } });
  ok(errs.length === 0, 'no console errors ' + JSON.stringify(errs)); await m.browser.close(); console.log(bad ? bad + ' FAILED' : 'all checks passed'); process.exit(bad ? 1 : 0);
})().catch(e => { console.error(e); process.exit(1); });
