// Embeds the player library (data/player_library.json) into a mockup page as a compact JSON block, so the HTML file stays self-contained.
// Usage: node tools/embed_player_library.js [mockup.html]       (default docs/mockups/JPurrr-8000-mockup-v3.8.html)
// The block sits between the lines <!--player-lib-begin--> and <!--player-lib-end--> of the page and is replaced on every run.
// Compact format: {"f":1,"g":[{"n":"group name","p":[["piece name",bpm,loopBeats,[beat,len,note,vel, beat,len,note,vel, ...]], ...]}, ...]}
const fs = require('fs'), path = require('path');
const ROOT = path.resolve(__dirname, '..');
const file = path.resolve(process.argv[2] || path.join(ROOT, 'docs/mockups/JPurrr-8000-mockup-v3.8.html'));
const lib = JSON.parse(fs.readFileSync(path.join(ROOT, 'data/player_library.json'), 'utf8'));
const r = v => Math.round(v * 1000) / 1000;
const out = { f: 1, g: lib.groups.map(g => ({ n: g.name, p: g.items.map(it => [it.name, it.bpm, it.beats, [].concat(...it.ev.map(e => [r(e[0]), r(e[1]), e[2], e[3]]))]) })) };
const json = JSON.stringify(out).replace(/</g, '\\u003c');   // safe inside a script tag
const html = fs.readFileSync(file, 'utf8');
const re = /<!--player-lib-begin-->[\s\S]*?<!--player-lib-end-->/;
if (!re.test(html)) throw new Error('markers <!--player-lib-begin--> / <!--player-lib-end--> not found in ' + file);
const block = '<!--player-lib-begin-->\n<script type="application/json" id="plLib">' + json + '</script>\n<!--player-lib-end-->';
fs.writeFileSync(file, html.replace(re, () => block));
const n = lib.groups.reduce((a, g) => a + g.items.length, 0);
console.log('embedded ' + lib.groups.length + ' groups, ' + n + ' pieces, ' + (json.length / 1024).toFixed(0) + ' KB into ' + path.relative(ROOT, file));
