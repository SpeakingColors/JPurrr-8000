// Checks the melody generator: every seed, key, mode and style gives a valid piece (same rules as the 22 melodies), and prints statistics.
// Usage: node tools/test_melody_gen.js [count]
const G = require('./melody_gen.js');
const N = +process.argv[2] || 2000; let bad = 0, tries = 0, maxTries = 0, notes = 0; const ivs = {}, fam = {}, lens = {};
let rep = 0, leaps = 0, octs = 0, steps = 0, moves = 0;
for (let i = 1; i <= N; i++) {
  const o = { seed: i * 7919 % 16777216 + 1, key: i % 12, mode: i % 3, style: i % 6 };
  let g; try { g = G.generate(o); } catch (e) { bad++; console.log('FAIL', JSON.stringify(o), e.message); continue; }
  tries += g.info.tries; maxTries = Math.max(maxTries, g.info.tries); notes += g.ev.length; fam[g.info.style] = (fam[g.info.style] || 0) + 1;
  const again = G.generate(o); if (JSON.stringify(again) !== JSON.stringify(g)) { bad++; console.log('NOT DETERMINISTIC', JSON.stringify(o)); }
  if (g.beats !== 32 || g.ev.some(e => !(e[1] > 0) || e[0] < 0 || e[0] >= 32 || e[2] < 0 || e[2] > 127 || e[3] < 1 || e[3] > 127)) { bad++; console.log('BAD EVENT', JSON.stringify(o)); }
  for (let k = 1; k < g.ev.length; k++) { const d = g.ev[k][2] - g.ev[k - 1][2], a = Math.abs(d); moves++; if (!a) rep++; else if (a <= 2) steps++; if (a >= 5) leaps++; if (a === 12) octs++; }
  g.ev.forEach(e => { const l = Math.round(e[1] * 4 * 100) / 100; lens[l] = (lens[l] || 0) + 1; });
}
console.log('pieces ' + N + ', problems ' + bad + ', average tries ' + (tries / N).toFixed(2) + ', most tries ' + maxTries + ', notes per piece ' + (notes / N).toFixed(1));
console.log('styles made', JSON.stringify(fam));
console.log('moves: repeat ' + (100 * rep / moves).toFixed(0) + '%, step 1-2 semitones ' + (100 * steps / moves).toFixed(0) + '%, leap 5+ ' + (100 * leaps / moves).toFixed(0) + '%, octave ' + (100 * octs / moves).toFixed(0) + '%');
process.exit(bad ? 1 : 0);
