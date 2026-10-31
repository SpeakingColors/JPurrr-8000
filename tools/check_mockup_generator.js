// Checks the melody generator of mockup v3.10 in headless Chromium: the library is untouched, GEN opens a panel without changing the window height,
// NEW MELODY plays a generated piece, key / mode / style / seed rewrite it, the same settings give the same notes, 8 generated pieces are kept,
// EXPORT MIDI works on them, Esc and the PLAYER switch close the panel, no console errors.  Usage: node tools/check_mockup_generator.js
if (!process.env.JP_MOCKUP) process.env.JP_MOCKUP = 'v3.10';
const fs = require('fs'), path = require('path');
const { open } = require('./mockup_page.js');
let bad = 0; const ok = (c, msg) => { console.log((c ? 'ok   ' : 'FAIL ') + msg); if (!c) bad++; };
const lib = f => (fs.readFileSync(path.join(__dirname, '../docs/mockups/JPurrr-8000-mockup-' + f + '.html'), 'utf8').match(/<script type="application\/json" id="plLib">([\s\S]*?)<\/script>/) || [])[1];
(async () => {
  ok(lib('v3.10') === lib('v3.9') && !!lib('v3.9'), 'the 162 library pieces (22 melodies, backings, riffs, demos) are byte for byte the same as in v3.9');
  const m = await open(); const p = m.page, errs = []; p.on('pageerror', e => errs.push(String(e))); p.on('console', c => { if (c.type() === 'error') errs.push(c.text()); });
  const ev = (f, a) => p.evaluate(f, a), J = () => ev(() => ({ n: window.__JPplayer.piece.name, ev: window.__JPplayer.piece.ev, playing: window.__JPplayer.playing, count: window.__JPplayer.count, open: window.__JPplayer.genOpen }));
  await ev(() => window.__JP.setP('player', 1, false)); await p.waitForTimeout(200);
  const H = () => ev(() => Math.round(document.querySelector('#win').getBoundingClientRect().height)); const h0 = await H();
  await p.click('#plGen'); await p.waitForTimeout(150); ok((await J()).open && (await H()) === h0, 'GEN opens the panel and the window keeps its height ' + h0);
  await p.click('#pgNew'); await p.waitForTimeout(200); let a = await J(); ok(/^Generated #\d+, /.test(a.n) && a.playing && a.count === 163, 'NEW MELODY writes and plays "' + a.n + '"');
  const seed = await ev(() => window.__JPplayer.gen.seed);
  await p.click('#pgStyle .chip[data-v="3"]'); await p.click('#pgKey .chip[data-v="9"]'); await p.click('#pgMode .chip[data-v="0"]'); a = await J();
  ok(/-Am?|A-|Am/.test(a.n) && (await ev(() => window.__JPplayer.gen.style)) === 3, 'KEY A, MINOR, ROLLER rewrite the piece: ' + a.n);
  const first = JSON.stringify(a.ev); await p.click('#pgKey .chip[data-v="2"]'); await p.click('#pgKey .chip[data-v="9"]'); ok(JSON.stringify((await J()).ev) === first, 'same seed, key, mode, style: same notes');
  await ev(() => window.__JPplayer.setGen({ seed: 4821, key: 9, mode: 0, style: 1 })); a = await J(); ok(a.n.startsWith('Generated #4821, Am-'), 'seed 4821 gives ' + a.n);
  const ok2 = a.ev.every(e => e[2] >= 36 && e[2] <= 96 && e[3] > 0 && e[1] > 0) && a.ev.length > 14; ok(ok2, a.ev.length + ' notes in a playable range');
  for (let i = 0; i < 12; i++) await p.click('#pgNew'); a = await J(); ok(a.count === 162 + 8, 'only the last 8 generated pieces are kept (' + (a.count - 162) + ')');
  await ev(() => window.__JPplayer.openMenu()); const t = await ev(() => document.querySelector('#plMenu').textContent); ok(/Generated melodies/.test(t), 'the piece list has a "Generated melodies" group'); await ev(() => window.__JPplayer.closeMenu());
  const mid = await ev(() => Array.from(window.__JPplayer.buildMidi()).length); ok(mid > 200, 'EXPORT MIDI builds a file for a generated piece (' + mid + ' bytes)');
  await p.keyboard.press('Escape'); ok(!(await J()).open, 'Esc closes the panel');
  await p.click('#plGen'); await ev(() => window.__JP.setP('player', 0, false)); ok(!(await J()).open, 'PLAYER switch off closes the panel');
  ok(errs.length === 0, 'no console errors ' + JSON.stringify(errs)); await m.browser.close(); console.log(bad ? bad + ' FAILED' : 'all checks passed'); process.exit(bad ? 1 : 0);
})().catch(e => { console.error(e); process.exit(1); });
