// Opens the approved mockup (v3.5) in headless Chromium at design scale 1 (2920 wide, the height follows the page) with the shipped fonts.
// Used by dump_layout.js and snap_mockup.js. Needs Playwright (npm) and no network.
const fs = require('fs'), path = require('path'), os = require('os');
let chromium; try { ({ chromium } = require('playwright')); } catch (e) { ({ chromium } = require('/opt/npm-tools/node_modules/playwright')); }
const ROOT = path.resolve(__dirname, '..');
const MOCK = path.join(ROOT, 'docs/mockups/JPurrr-8000-mockup-v3.5.html');
const FONTS = path.join(ROOT, 'plugin/Resources/fonts');

function fontCss() {
  const map = { Barlow: 'Barlow', BarlowCondensed: 'Barlow Condensed', JetBrainsMono: 'JetBrains Mono' };
  let css = '';
  for (const f of fs.readdirSync(FONTS)) {
    const m = f.match(/^(\w+)-(\d+)\.ttf$/); if (!m) continue;
    css += `@font-face{font-family:'${map[m[1]]}';font-weight:${m[2]};src:url(data:font/ttf;base64,${fs.readFileSync(path.join(FONTS, f)).toString('base64')}) format('truetype');}`;
  }
  return css;
}

// The mockup is patched in memory (never on disk in docs/): design scale 1 and a few internals exposed.
function patched(file) {
  let h = fs.readFileSync(file || MOCK, 'utf8');
  h = h.replace('const NAT=.72,MINS=.6;', 'const NAT=1,MINS=1;');
  h = h.replace('window.__JP={P,ORDER,DEFS,FACTORY,BANKS,TYPES,GENRES,qvals,snapshot,loadPreset};',
    'window.__JP={P,ORDER,DEFS,FACTORY,BANKS,TYPES,GENRES,qvals,snapshot,loadPreset,MPD,QBASE,QT,VMC,O1L,O2L,O1W,GPATS,PAGES,S,say,guide,settings,about,saveDlg,browser,exportDlg,importDlg,openPage,setP,showPage,fxSub,applyLook,delPreset,openDlg,closeDlg,startRename,get cur(){return cur}};');
  if (!h.includes('MPD,QBASE')) throw new Error('mockup patch point not found');
  return h;
}

async function open(opts = {}) {
  const tmp = path.join(os.tmpdir(), 'jp-mock-' + process.pid + '-' + (opts.file ? 'alt' : 'main') + '.html');
  fs.writeFileSync(tmp, patched(opts.file));
  const browser = await chromium.launch();
  const page = await browser.newPage({ viewport: { width: opts.vw || 3000, height: opts.vh || 1500 }, deviceScaleFactor: opts.dpr || 1 });
  await page.route(/fonts\.googleapis|fonts\.gstatic/, r => r.abort());
  await page.goto('file://' + tmp);
  await page.addStyleTag({ content: fontCss() });
  await page.evaluate(() => document.fonts.ready);
  await page.waitForTimeout(600);
  // freeze animation, hide page chrome, put the window at 0,0
  await page.addStyleTag({ content: '.pagebar,.zoomrow,.hd{display:none!important} body{padding:0!important;margin:0!important} #stageWrap{padding:0!important;overflow:visible!important} *{caret-color:transparent!important}' });
  await page.waitForTimeout(100);
  return { browser, page, close: async () => { await browser.close(); try { fs.unlinkSync(tmp); } catch (e) {} } };
}
module.exports = { open, ROOT, MOCK };
