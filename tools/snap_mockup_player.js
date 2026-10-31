// Reference screenshots of the mockup's built-in MIDI player (v3.8), at design scale 1, for the plugin comparison.
// Usage: JP_MOCKUP=v3.8 node tools/snap_mockup_player.js <outdir>      (the default file of mockup_page.js is v3.5, so JP_MOCKUP is set here when it is not)
// Writes sound_player.png chain_player.png master_player.png (PLAYER and PREVIEW on, the piece playing: playhead at beat 21),
//   sound_player_menu.png (piece list open), sound_player_nokbd.png (PREVIEW off: the bar sits at the bottom), sound_parts_split_player.png (PARTS band + player bar),
//   and chain_player_nokbd.png master_player_nokbd.png sound_player_stopped.png sound_player_lock.png sound_player_midi.png (a loaded MIDI file, name and BPM shown).
if (!process.env.JP_MOCKUP) process.env.JP_MOCKUP = 'v3.8';
const path = require('path'), fs = require('fs');
const { open } = require('./mockup_page.js');
// A tiny standard MIDI file (format 0, 480 ticks, 96 BPM, a C minor arpeggio over two bars) for the "load a file" screenshot.
function tinyMidi() {
  const ev = []; const t = (d, ...b) => ev.push(...vl(d), ...b);
  function vl(v) { const o = [v & 127]; while ((v >>= 7)) o.unshift((v & 127) | 128); return o; }
  t(0, 0xFF, 0x51, 3, 0x09, 0x89, 0x68);                      // 625000 us per beat = 96 BPM
  [60, 63, 67, 72, 67, 63, 60, 63].forEach(n => { t(0, 0x90, n, 96); t(240, 0x80, n, 0); });
  t(0, 0xFF, 0x2F, 0);
  const trk = Buffer.from(ev), hd = Buffer.from([0x4D, 0x54, 0x68, 0x64, 0, 0, 0, 6, 0, 0, 0, 1, 0x01, 0xE0]);
  const th = Buffer.alloc(8); th.write('MTrk'); th.writeUInt32BE(trk.length, 4);
  return Buffer.concat([hd, th, trk]);
}
(async () => {
  const out = path.resolve(process.argv[2] || 'snap-mock-player'); fs.mkdirSync(out, { recursive: true });
  const m = await open(); const p = m.page;
  const errs = []; p.on('pageerror', e => errs.push(String(e))); p.on('console', c => { if (c.type() === 'error') errs.push(c.text()); });
  const shot = async (name) => {
    await p.waitForTimeout(120);
    const box = await p.evaluate(() => { const r = document.querySelector('#win').getBoundingClientRect(); return { x: r.x, y: r.y, w: r.width, h: r.height }; });
    await p.screenshot({ path: path.join(out, name + '.png'), clip: { x: box.x, y: box.y, width: box.w, height: box.h } });
    console.log(name, box.w + 'x' + box.h);
  };
  const ev = (f, a) => p.evaluate(f, a);
  const page = async n => { if (n === 'sound') await ev(() => window.__JP.showPage('sound')); else await ev(n2 => { window.__JP.showPage('fx'); window.__JP.fxSub(n2 === 'chain' ? 1 : 2); }, n); await p.waitForTimeout(80); };
  const demo = (idx, beat) => ev(([i, b]) => { const pl = window.__JPplayer; pl.select(i); pl.demo(b); }, [idx, beat]);
  await ev(() => { window.__JP.setP('player', 1, false); });
  await demo(1, 21);                                          // "Soaring anthem (E minor)", playing, playhead in bar 6
  for (const n of ['sound', 'chain', 'master']) { await page(n); await shot(n + '_player'); }
  await page('sound');
  await ev(() => window.__JPplayer.openMenu()); await shot('sound_player_menu'); await ev(() => window.__JPplayer.closeMenu());
  await ev(() => window.__JPplayer.stop()); await shot('sound_player_stopped');
  await demo(1, 21);
  await ev(() => { window.__JPplayer.setLock(true); window.__JPplayer.setBpm(150); window.__JPplayer.setTranspose(-5); }); await shot('sound_player_lock');
  await ev(() => { window.__JPplayer.setLock(false); window.__JPplayer.setTranspose(0); window.__JPplayer.select(1); window.__JPplayer.demo(21); });
  await p.setInputFiles('#plFile', { name: 'my_arpeggio.mid', mimeType: 'audio/midi', buffer: tinyMidi() }); await p.waitForTimeout(200);
  await ev(() => window.__JPplayer.demo(5)); await shot('sound_player_midi');
  await demo(1, 21);
  await ev(() => window.__JP.setP('prev', 0, true));
  await shot('sound_player_nokbd'); await page('chain'); await shot('chain_player_nokbd'); await page('master'); await shot('master_player_nokbd');
  await ev(() => window.__JP.setP('prev', 1, true)); await page('sound');
  // PARTS band + player bar together (SPLIT, strip shown)
  await ev(() => { const J = window.__JP; J.setP('ps', 1, false); J.setP('km', 2, false); window.__JPparts.setParts(true); }); await shot('sound_parts_split_player');
  await ev(() => { const J = window.__JP; window.__JPparts.setParts(false); J.setP('km', 0, false); J.setP('ps', 1, false); });
  if (errs.length) { console.log('ERRORS', errs); process.exitCode = 1; }
  await m.close();
})();
