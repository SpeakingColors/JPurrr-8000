// Writes tools/melody_gen.js (the generator with the learned data from data/melody_gen.json filled in) and embeds it in a mockup page.
// Usage: node tools/embed_melody_gen.js [mockup.html]       (default docs/mockups/JPurrr-8000-mockup-v3.9.html; the page needs the markers <!--melody-gen-begin--> and <!--melody-gen-end-->, otherwise only tools/melody_gen.js is written)
const fs = require('fs'), path = require('path');
const ROOT = path.resolve(__dirname, '..');
const data = fs.readFileSync(path.join(ROOT, 'data/melody_gen.json'), 'utf8').trim();
const src = fs.readFileSync(path.join(__dirname, 'melody_gen.src.js'), 'utf8');
const out = src.replace(/\/\*DATA\*\/[\s\S]*?\/\*END\*\//, () => '/*DATA*/' + data + '/*END*/');
fs.writeFileSync(path.join(__dirname, 'melody_gen.js'), out);
console.log('wrote tools/melody_gen.js (' + (out.length / 1024).toFixed(1) + ' KB)');
const file = path.resolve(process.argv[2] || path.join(ROOT, 'docs/mockups/JPurrr-8000-mockup-v3.9.html'));
if (fs.existsSync(file)) {
  const html = fs.readFileSync(file, 'utf8'), re = /<!--melody-gen-begin-->[\s\S]*?<!--melody-gen-end-->/;
  if (re.test(html)) { fs.writeFileSync(file, html.replace(re, () => '<!--melody-gen-begin-->\n<script>' + out.replace(/<\/script/gi, '<\\/script') + '</script>\n<!--melody-gen-end-->')); console.log('embedded into ' + path.relative(ROOT, file)); }
}
