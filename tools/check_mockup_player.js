// Checks the built-in MIDI player of mockup v3.8 in headless Chromium: window heights per page, PLAY / STOP through the preview synth, LOCK, BPM and TRANSPOSE boxes,
// the piece menu, loading a MIDI file, no console errors. Usage: node tools/check_mockup_player.js     (exit code 1 when a check fails)
if (!process.env.JP_MOCKUP) process.env.JP_MOCKUP = 'v3.8';
const { open } = require('./mockup_page.js');
let bad = 0; const ok = (c, msg) => { console.log((c ? 'ok   ' : 'FAIL ') + msg); if (!c) bad++; };
function tinyMidi() { const ev = []; const vl = v => { const o = [v & 127]; while ((v >>= 7)) o.unshift((v & 127) | 128); return o; }; const t = (d, ...b) => ev.push(...vl(d), ...b);
  t(0, 0xFF, 0x51, 3, 0x09, 0x89, 0x68); [60, 63, 67, 72].forEach(n => { t(0, 0x90, n, 96); t(240, 0x80, n, 0); }); t(0, 0xFF, 0x2F, 0);
  const trk = Buffer.from(ev), hd = Buffer.from([0x4D, 0x54, 0x68, 0x64, 0, 0, 0, 6, 0, 0, 0, 1, 0x01, 0xE0]), th = Buffer.alloc(8); th.write('MTrk'); th.writeUInt32BE(trk.length, 4); return Buffer.concat([hd, th, trk]); }
(async () => {
  const m = await open(); const p = m.page; const errs = [];
  p.on('pageerror', e => errs.push(String(e))); p.on('console', c => { if (c.type() === 'error') errs.push(c.text()); });
  const ev = (f, a) => p.evaluate(f, a);
  const H = () => ev(() => Math.round(document.querySelector('#win').getBoundingClientRect().height));
  const page = async n => { if (n === 'sound') await ev(() => window.__JP.showPage('sound')); else await ev(n2 => { window.__JP.showPage('fx'); window.__JP.fxSub(n2 === 'chain' ? 1 : 2); }, n); await p.waitForTimeout(60); };
  // 1. heights: v3.7 numbers (strip on 1105/1135/1273, off 931/961/1099, parts +175) plus 48 with the player bar
  const base = { sound: [1105, 931], chain: [1135, 961], master: [1273, 1099] };
  ok(!(await ev(() => document.querySelector('#plr').offsetParent)), 'player bar hidden by default (PLAYER off)');
  for (const pv of [1, 0]) { await ev(v => window.__JP.setP('prev', v, true), pv);
    for (const n of Object.keys(base)) { await page(n); const h0 = await H(); await ev(() => window.__JP.setP('player', 1, false)); const h1 = await H(); await ev(() => window.__JP.setP('player', 0, false)); const h2 = await H();
      console.log('     ' + n + ' PREVIEW ' + (pv ? 'on ' : 'off') + ': off ' + h0 + '  PLAYER on ' + h1 + '  (+' + (h1 - h0) + ')');
      ok(h0 === base[n][pv ? 0 : 1] && h1 === h0 + 48 && h2 === h0, n + ' PREVIEW ' + pv + ': ' + h0 + ' -> ' + h1 + ' -> back ' + h2); } }
  await ev(() => window.__JP.setP('prev', 1, true)); await page('sound');
  await ev(() => { window.__JP.setP('ps', 1, false); window.__JP.setP('km', 2, false); window.__JPparts.setParts(true); }); const hp0 = await H();
  await ev(() => window.__JP.setP('player', 1, false)); const hp1 = await H(); ok(hp0 === 1280 && hp1 === 1328, 'PARTS band + player bar: ' + hp0 + ' -> ' + hp1);
  const r = await ev(() => { const g = id => document.querySelector(id).getBoundingClientRect(); const a = g('#parts'), b = g('#plr'), k = g('#kbd'); return [Math.round(a.bottom), Math.round(b.top), Math.round(b.bottom), Math.round(k.top)]; });
  ok(r[1] - r[0] === 8 && r[3] - r[2] === 8, 'order PARTS, player bar, strip with 8 px gaps ' + r.join(','));
  await ev(() => { window.__JPparts.setParts(false); window.__JP.setP('km', 0, false); });
  // with PREVIEW off the bar sits at the bottom of the content
  await ev(() => window.__JP.setP('prev', 0, true)); const bt = await ev(() => { const b = document.querySelector('#plr').getBoundingClientRect(), f = document.querySelector('#ftr').getBoundingClientRect(); return Math.round(f.top - b.bottom); }); ok(bt >= 0 && bt <= 12, 'PREVIEW off: bar sits right above the footer (' + bt + ' px)'); await ev(() => window.__JP.setP('prev', 1, true));
  // 2. play through the preview synth
  await p.click('#plPlay'); await p.waitForTimeout(700);
  let s = await ev(() => ({ on: window.__JPplayer.stats.on, off: window.__JPplayer.stats.off, held: window.__JPplayer.held.length, keys: document.querySelectorAll('#keys .on').length, playing: window.__JPplayer.playing, btn: document.querySelector('#plPlay').textContent.trim() }));
  console.log('     playing ' + JSON.stringify(s));
  ok(s.playing && s.on > 0 && s.btn === 'STOP', 'PLAY starts the piece: ' + s.on + ' note-ons through noteOn, button says STOP');
  ok(s.keys > 0, 'the strip keys light up: ' + s.keys + ' keys on (same noteOn path as the strip)');
  await p.waitForTimeout(300); await p.click('#plPlay'); await p.waitForTimeout(150);
  s = await ev(() => ({ held: window.__JPplayer.held.length, keys: document.querySelectorAll('#keys .on').length, playing: window.__JPplayer.playing, btn: document.querySelector('#plPlay').textContent.trim(), on: window.__JPplayer.stats.on, off: window.__JPplayer.stats.off }));
  ok(!s.playing && s.held === 0 && s.keys === 0 && s.btn === 'PLAY', 'STOP releases everything (held ' + s.held + ', keys on ' + s.keys + ')');
  const on1 = s.on; await p.waitForTimeout(500); ok((await ev(() => window.__JPplayer.stats.on)) === on1, 'no more notes after STOP');
  // loop: at 300 BPM the 16 beat piece laps in 3.2 s
  await ev(() => { window.__JPplayer.select(0); window.__JPplayer.setBpm(300); window.__JPplayer.setLock(false); window.__JPplayer.play(); }); await p.waitForTimeout(4200);
  const lp = await ev(() => ({ on: window.__JPplayer.stats.on, n: window.__JPplayer.piece.ev.length })); await ev(() => window.__JPplayer.stop());
  ok(lp.on - on1 > lp.n, 'the piece loops (' + (lp.on - on1) + ' notes played, ' + lp.n + ' in one lap)');
  // notes end by their length: at 60 BPM a note of 0.46 beat is released after about 0.46 s
  await ev(() => { window.__JPplayer.setLock(false); window.__JPplayer.select(0); window.__JPplayer.setBpm(60); window.__JPplayer.play(); }); await p.waitForTimeout(200);
  const h1 = await ev(() => window.__JPplayer.held.length); await p.waitForTimeout(700); const h2 = await ev(() => ({ held: window.__JPplayer.held.length, off: window.__JPplayer.stats.off }));
  ok(h1 >= 1 && h2.off > 0, 'notes end by their length (held ' + h1 + ' at 0.2 s, ' + h2.off + ' note-offs by 0.9 s)'); await ev(() => window.__JPplayer.stop());
  // 3. LOCK
  await ev(() => { window.__JPplayer.setLock(false); window.__JPplayer.select(0); }); let b0 = await ev(() => window.__JPplayer.bpm);
  await ev(() => window.__JPplayer.select(2)); const b1 = await ev(() => window.__JPplayer.bpm); await ev(() => window.__JPplayer.select(18 + 11 + 1)); const b2 = await ev(() => window.__JPplayer.bpm);
  ok(b0 === 138 && b1 === 140 && b2 === (await ev(() => Math.round(window.__JPplayer.piece.bpm))), 'LOCK off: each piece sets its own BPM (' + [b0, b1, b2] + ')');
  await p.click('#plLockG'); await ev(() => window.__JPplayer.setBpm(155)); await p.click('#plNext'); await p.click('#plNext'); const b3 = await ev(() => [window.__JPplayer.bpm, window.__JPplayer.piece.bpm, window.__JPplayer.lock]);
  ok(b3[2] && b3[0] === 155 && b3[1] !== 155, 'LOCK on: NEXT keeps your BPM (155, piece has ' + b3[1] + ')'); await p.click('#plLockG');
  const b4 = await ev(() => { window.__JPplayer.next(); return [window.__JPplayer.bpm, window.__JPplayer.piece.bpm]; }); ok(b4[0] === Math.round(b4[1]), 'LOCK off again: the piece BPM is back (' + b4 + ')');
  // 4. number boxes
  const bx = async sel => { const r = await ev(s => { const b = document.querySelector(s).getBoundingClientRect(); return [b.x + b.width / 2, b.y + b.height / 2]; }, sel); return r; };
  let [x, y] = await bx('#plBpm'); await p.mouse.move(x, y); await ev(() => window.__JPplayer.setBpm(120)); await p.mouse.wheel(0, -100); await p.waitForTimeout(80); let v = await ev(() => window.__JPplayer.bpm); ok(v === 121, 'BPM wheel up: 121 (' + v + ')');
  await p.mouse.down(); await p.mouse.move(x, y - 40, { steps: 8 }); await p.mouse.up(); v = await ev(() => window.__JPplayer.bpm); ok(v > 121 + 10, 'BPM drag up raises it (' + v + ')');
  await p.click('#plBpm'); await p.keyboard.type('333'); await p.keyboard.press('Enter'); v = await ev(() => window.__JPplayer.bpm); ok(v === 300, 'BPM typed 333 is limited to 300 (' + v + ')');
  await p.click('#plBpm'); await p.keyboard.type('10'); await p.keyboard.press('Enter'); v = await ev(() => window.__JPplayer.bpm); ok(v === 20, 'BPM typed 10 is limited to 20 (' + v + ')');
  await p.dblclick('#plBpm'); v = await ev(() => [window.__JPplayer.bpm, Math.round(window.__JPplayer.piece.bpm)]); ok(v[0] === v[1], 'BPM double click: the tempo of the piece (' + v + ')');
  [x, y] = await bx('#plTr'); await p.mouse.move(x, y); await p.mouse.wheel(0, -100); await p.mouse.wheel(0, -100); v = await ev(() => window.__JPplayer.transpose); ok(v === 2, 'TRANSPOSE wheel up twice: +2 (' + v + ')');
  await p.click('#plTr'); await p.keyboard.type('-99'); await p.keyboard.press('Enter'); v = await ev(() => document.querySelector('#plTr').value + '/' + window.__JPplayer.transpose); ok(v === '-24/-24', 'TRANSPOSE typed -99 is limited to -24 (' + v + ')');
  await p.dblclick('#plTr'); v = await ev(() => window.__JPplayer.transpose); ok(v === 0, 'TRANSPOSE double click resets to 0');
  await ev(() => { window.__JPplayer.select(0); window.__JPplayer.setTranspose(12); window.__JPplayer.setBpm(300); window.__JPplayer.play(); }); await p.waitForTimeout(100);
  const tn = await ev(() => [window.__JPplayer.stats.last, window.__JPplayer.piece.ev[0][2]]); await ev(() => { window.__JPplayer.stop(); window.__JPplayer.setTranspose(0); });
  ok(tn[0] >= tn[1] + 12 - 12 && tn[0] - 12 >= 24, 'TRANSPOSE +12 plays an octave higher (last note ' + tn[0] + ', first note of the piece ' + tn[1] + ')');
  // 5. piece menu
  await p.click('#plSel'); await p.waitForTimeout(100); const mn = await ev(() => ({ open: window.__JPplayer.menuOpen, groups: document.querySelectorAll('#plMenu .plg').length, rows: document.querySelectorAll('#plMenu .pli').length, first: document.querySelector('#plMenu .plg span').textContent, second: document.querySelectorAll('#plMenu .plg span')[1].textContent }));
  ok(mn.open && mn.groups === 13 && mn.rows === 118 && /trance/i.test(mn.first) && /trance/i.test(mn.second), 'menu: ' + mn.groups + ' groups, ' + mn.rows + ' pieces, first two groups "' + mn.first + '", "' + mn.second + '"');
  await p.click('#plMenu .pli[data-i="30"]'); v = await ev(() => [window.__JPplayer.idx, window.__JPplayer.menuOpen]); ok(v[0] === 30 && !v[1], 'clicking a row chooses the piece and closes the menu');
  await p.click('#plSel'); await p.keyboard.press('ArrowDown'); await p.keyboard.press('Enter'); v = await ev(() => [window.__JPplayer.idx, window.__JPplayer.menuOpen]); ok(v[0] === 31 && !v[1], 'keyboard: arrow down and Enter chooses the next piece');
  await p.click('#plSel'); await p.keyboard.press('Escape'); ok(!(await ev(() => window.__JPplayer.menuOpen)), 'Escape closes the menu');
  await p.click('#plSel'); await p.mouse.click(300, 200); ok(!(await ev(() => window.__JPplayer.menuOpen)), 'a click outside closes the menu');
  // 6. load a MIDI file
  await ev(() => { window.__JPplayer.setLock(false); }); await p.setInputFiles('#plFile', { name: 'my_arpeggio.mid', mimeType: 'audio/midi', buffer: tinyMidi() }); await p.waitForTimeout(250);
  v = await ev(() => ({ name: document.querySelector('#plName').textContent, bpm: window.__JPplayer.bpm, beats: window.__JPplayer.piece.beats, n: window.__JPplayer.piece.ev.length, grp: document.querySelector('#plGrp').textContent }));
  ok(v.name === 'MIDI: my_arpeggio (96)' && v.bpm === 96 && v.n === 4 && v.beats === 4, 'LOAD MIDI FILE: "' + v.name + '", BPM ' + v.bpm + ', ' + v.n + ' notes, loop ' + v.beats + ' beats');
  await p.setInputFiles('#plFile', { name: 'bad.mid', mimeType: 'audio/midi', buffer: Buffer.from('not a midi file at all') }); await p.waitForTimeout(150);
  ok(/could not be read/.test(await ev(() => document.querySelector('#msg').textContent)), 'a broken file gives a plain message');
  // 7. PLAYER off stops the player
  await ev(() => { window.__JP.cur.dirty = false; window.__JPplayer.play(); }); await p.waitForTimeout(100); await p.click('[data-id="player"]'); await p.waitForTimeout(100);
  v = await ev(() => ({ playing: window.__JPplayer.playing, held: window.__JPplayer.held.length, keys: document.querySelectorAll('#keys .on').length, hidden: !document.querySelector('#plr').offsetParent, dirty: window.__JP.cur.dirty }));
  ok(!v.playing && !v.held && !v.keys && v.hidden, 'PLAYER switch off hides the bar and stops the player');
  ok(!v.dirty, 'switching PLAYER does not mark the preset as changed');
  await ev(() => { window.__JP.setP('player', 1, false); }); await p.keyboard.down('KeyA'); await p.waitForTimeout(60); v = await ev(() => document.querySelectorAll('#keys .on').length); await p.keyboard.up('KeyA'); ok(v === 1, 'computer key A still plays the strip with the player bar open');
  ok(errs.length === 0, 'no console errors' + (errs.length ? ': ' + errs.join(' | ') : ''));
  await m.close(); console.log(bad ? bad + ' check(s) FAILED' : 'all checks passed'); process.exitCode = bad ? 1 : 0;
})();
