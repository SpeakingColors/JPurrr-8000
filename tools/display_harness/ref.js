// Renders every cv_* canvas of the mockup for a list of states, as PNGs (2x pixel density, canvas content over the display background).
// usage: node ref.js states.json outdir [mockup.html]
// The mockup file is copied to a temp file with a few test hooks (frozen animation phase, setP/poke exposed); the original is never changed.
// Needs playwright (see PW_MODULES or the scratchpad tool dir) and the @fontsource fonts used by tool/pw.js.
const fs = require('fs'), path = require('path');
const PW_DIR = process.env.PW_DIR || '/tmp/claude-0/-home-claude/2264391b-5fea-5001-8ec8-301a6c10a547/scratchpad/tool';
const { open } = require(path.join(PW_DIR, 'pw.js'));

const [statesFile, outDir, mockupArg] = process.argv.slice(2);
const mockup = path.resolve(mockupArg || path.join(__dirname, '../../docs/mockups/JPurrr-8000-mockup-v3.2.html'));
let html = fs.readFileSync(mockup, 'utf8');
const rep = (a, b) => { if (!html.includes(a)) throw new Error('hook anchor missing: ' + a.slice(0, 60)); html = html.replace(a, b); };
rep('scr=(scr+dt*(voices.size?2.2:.9))%1000;', 'scr=(window.__SCR!=null)?window.__SCR:(scr+dt*(voices.size?2.2:.9))%1000;');
rep('const t=performance.now()/1000*.35;const mid=(h+14)/2', 'const t=(window.__T!=null?window.__T:performance.now()/1000)*.35;const mid=(h+14)/2');
rep('window.__JP={P,', 'window.__JP={setRibT:(i)=>{ribT=i},setP,poke,noteOn,noteOff,voices,ensureAudio,view,P,');
const tmp = path.join(path.dirname(path.resolve(statesFile)), '_mockup_hooked.html');
fs.writeFileSync(tmp, html);

(async () => {
  const states = JSON.parse(fs.readFileSync(statesFile, 'utf8'));
  const { b, p } = await open(tmp, 1600, 900);
  await p.addStyleTag({ content: '#win{transform:none!important}' });
  for (const st of states) {
    const dir = path.join(outDir, st.name); fs.mkdirSync(dir, { recursive: true });
    await p.evaluate(({ st }) => {
      const J = window.__JP; const P = J.P;
      J.view.o1 = 0; J.view.o2 = 0;
      window.__SCR = st.scr != null ? st.scr : 0; window.__T = st.t != null ? st.t : 0;
      const f0 = J.FACTORY[0];
      const v = Object.assign({}, f0.v, st.params || {});
      // nosave params (bend, mod, rib...) survive loadPreset, so set them first
      for (const id in (st.params || {})) if (P[id] && P[id].nosave) P[id].v = st.params[id];
      J.loadPreset(Object.assign({}, f0, { v }));
      for (const id of (st.touch || [])) J.poke(id.startsWith('o2') ? 'o2' : 'o1');
      J.setRibT(st.ribbonTarget || null);
    }, { st });
    await p.waitForTimeout(250);
    const urls = await p.evaluate(() => {
      const out = {}; const bg = getComputedStyle(document.querySelector('#win')).getPropertyValue('--dsp').trim();
      document.querySelectorAll('canvas.cv').forEach(c => {
        const o = document.createElement('canvas'); o.width = c.width; o.height = c.height; const x = o.getContext('2d');
        x.fillStyle = bg; x.fillRect(0, 0, o.width, o.height); x.drawImage(c, 0, 0); out[c.id] = o.toDataURL('image/png');
      });
      return out;
    });
    for (const id in urls) fs.writeFileSync(path.join(dir, id + '.png'), Buffer.from(urls[id].split(',')[1], 'base64'));
    console.log(st.name, Object.keys(urls).length, 'canvases');
  }
  await b.close();
})();
