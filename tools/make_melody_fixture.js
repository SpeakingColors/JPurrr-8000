// Writes tests/data/melody_gen_fixture.txt from tools/melody_gen.js, so tests/test_melody_gen.cpp can check that the C++
// generator gives exactly the same notes as the mockup's JavaScript. One line per case:
//   seed key mode style | bpm minor styleUsed tries | name | beatQ:lenMs:note:vel ...
// Run:  node tools/make_melody_fixture.js      (after tools/embed_melody_gen.js refreshed tools/melody_gen.js)
const fs = require('fs'), path = require('path');
const G = require('./melody_gen.js');
const out = [];
const cases = [];
for (let i = 0; i < 48; i++) cases.push({ seed: (i * 7919 + 1) % 16777216 + 1, key: i % 12, mode: i % 3, style: i % 6 });
cases.push({ seed: 1, key: 0, mode: 0, style: 0 }, { seed: 16777215, key: 11, mode: 2, style: 5 }, { seed: 4821, key: 9, mode: 0, style: 1 });
for (const c of cases) {
  const g = G.generate(c);
  const ev = g.ev.map(e => `${Math.round(e[0] * 4)}:${Math.round(e[1] * 1000)}:${e[2]}:${e[3]}`).join(' ');
  out.push(`${c.seed} ${c.key} ${c.mode} ${c.style} | ${g.bpm} ${g.info.minor ? 1 : 0} ${g.info.style} ${g.info.tries} | ${g.name} | ${ev}`);
}
const f = path.join(__dirname, '../tests/data/melody_gen_fixture.txt');
fs.writeFileSync(f, out.join('\n') + '\n');
console.log('wrote', path.relative(path.join(__dirname, '..'), f), out.length, 'cases');
