// Plays notes in the mockup (WebAudio) and dumps the canvases while sound runs, to compare the live overlays by eye.
// usage: node ref_live.js outdir [note ...]   (uses the hooked copy written by ref.js; run ref.js once first)
const fs = require('fs'), path = require('path');
const PW_DIR = process.env.PW_DIR || '/tmp/claude-0/-home-claude/2264391b-5fea-5001-8ec8-301a6c10a547/scratchpad/tool';
const { open } = require(path.join(PW_DIR, 'pw.js'));
const [outDir, ...notes] = process.argv.slice(2);
const tmp = path.join(__dirname, '_mockup_hooked.html');
(async () => {
  const { b, p } = await open(tmp, 1600, 900);
  await p.addStyleTag({ content: '#win{transform:none!important}' });
  fs.mkdirSync(outDir, { recursive: true });
  await p.evaluate(async (notes) => {
    const J = window.__JP; J.ensureAudio(); await new Promise(r => setTimeout(r, 300));
    (notes.length ? notes : [48, 55, 60, 64]).forEach(n => J.noteOn(+n));
  }, notes);
  for (let k = 0; k < 3; k++) {
    await p.waitForTimeout(700);
    const urls = await p.evaluate(() => {
      const out = {}; const bg = getComputedStyle(document.querySelector('#win')).getPropertyValue('--dsp').trim();
      document.querySelectorAll('canvas.cv').forEach(c => { const o = document.createElement('canvas'); o.width = c.width; o.height = c.height; const x = o.getContext('2d'); x.fillStyle = bg; x.fillRect(0, 0, o.width, o.height); x.drawImage(c, 0, 0); out[c.id] = o.toDataURL('image/png'); });
      return out;
    });
    for (const id in urls) fs.writeFileSync(path.join(outDir, `${id}_${k}.png`), Buffer.from(urls[id].split(',')[1], 'base64'));
  }
  await b.close();
})();
