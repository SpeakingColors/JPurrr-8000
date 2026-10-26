// Reference screenshots of the mockup at design scale 1 for the plugin comparison.
// Usage: node tools/snap_mockup.js <outdir>      writes sound.png chain.png master.png guide.png ... (window only, no keyboard)
const path = require('path'), fs = require('fs');
const { open } = require('./mockup_page.js');
(async () => {
  const out = path.resolve(process.argv[2] || 'snap-mock'); fs.mkdirSync(out, { recursive: true });
  const m = await open(); const p = m.page;
  // the preview keyboard is not part of the plugin: hide it and cut the window at the page height
  await p.addStyleTag({ content: '#kbd{visibility:hidden!important}' });
  const H = { sound: 931, chain: 961, master: 1099 };
  const shot = async (name, page) => {
    await p.evaluate(h => { document.querySelector('#win').style.height = h + 'px'; document.querySelector('#holder').style.height = h + 'px'; }, H[page]);
    await p.waitForTimeout(80);
    const box = await p.evaluate(() => { const r = document.querySelector('#win').getBoundingClientRect(); return { x: r.x, y: r.y, w: r.width, h: r.height }; });
    await p.screenshot({ path: path.join(out, name + '.png'), clip: { x: box.x, y: box.y, width: box.w, height: box.h } });
    console.log(name);
  };
  const J = () => p.evaluate(() => 0);
  const ev = (f, ...a) => p.evaluate(f, ...a);
  await ev(() => window.__JP.showPage('sound')); await shot('sound', 'sound');
  await ev(() => { window.__JP.showPage('fx'); window.__JP.fxSub(1); }); await p.waitForTimeout(100); await shot('chain', 'chain');
  await ev(() => window.__JP.fxSub(2)); await p.waitForTimeout(100); await shot('master', 'master');
  await ev(() => { window.__JP.fxSub(1); window.__JP.showPage('sound'); });
  for (const [n, f] of Object.entries({ guide: () => __JP.guide(), settings: () => __JP.settings(), about: () => __JP.about(), save: () => __JP.saveDlg(), browser: () => __JP.browser(), export: () => __JP.exportDlg(), import: () => __JP.importDlg(), page1: () => __JP.openPage(1), page2: () => __JP.openPage(2), page4: () => __JP.openPage(4), page6: () => __JP.openPage(6), page7: () => __JP.openPage(7), page8: () => __JP.openPage(8) })) {
    await p.evaluate(f); await p.waitForTimeout(100); await shot(n, 'sound'); await p.evaluate(() => window.__JP.closeDlg());
  }
  // preset menu
  await p.click('#bMenu'); await p.waitForTimeout(80); await shot('menu', 'sound'); await p.mouse.click(1500, 600);
  // look variants
  for (const l of ['navy', 'soda', 'purr']) { await p.evaluate(l => { window.__JP.S.look = l; window.__JP.applyLook(); }, l); await shot('look_' + l, 'sound'); }
  // tooltip + value bubble
  await p.evaluate(() => { window.__JP.S.look = 'jp'; window.__JP.applyLook(); });
  await m.close();
})();
