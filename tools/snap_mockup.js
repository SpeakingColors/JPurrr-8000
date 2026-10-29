// Reference screenshots of the mockup at design scale 1 for the plugin comparison.
// Usage: node tools/snap_mockup.js <outdir>      writes sound.png chain.png master.png (strip shown), *_nokbd.png (PREVIEW off), guide.png ... (window only)
const path = require('path'), fs = require('fs');
const { open } = require('./mockup_page.js');
(async () => {
  const out = path.resolve(process.argv[2] || 'snap-mock'); fs.mkdirSync(out, { recursive: true });
  const m = await open(); const p = m.page;
  // v3.3: the window height follows the page and the PREVIEW switch (strip shown by default), so no resizing is needed
  const shot = async (name) => {
    await p.waitForTimeout(80);
    const box = await p.evaluate(() => { const r = document.querySelector('#win').getBoundingClientRect(); return { x: r.x, y: r.y, w: r.width, h: r.height }; });
    await p.screenshot({ path: path.join(out, name + '.png'), clip: { x: box.x, y: box.y, width: box.w, height: box.h } });
    console.log(name, box.w + 'x' + box.h);
  };
  const prev = v => p.evaluate(v => window.__JP.setP('prev', v, true), v);
  const J = () => p.evaluate(() => 0);
  const ev = (f, ...a) => p.evaluate(f, ...a);
  await ev(() => window.__JP.showPage('sound')); await shot('sound');
  await ev(() => { window.__JP.showPage('fx'); window.__JP.fxSub(1); }); await p.waitForTimeout(100); await shot('chain');
  await ev(() => window.__JP.fxSub(2)); await p.waitForTimeout(100); await shot('master');
  // strip hidden (PREVIEW off): the old content heights 931 / 961 / 1099
  await prev(0);
  await ev(() => window.__JP.showPage('sound')); await shot('sound_nokbd');
  await ev(() => { window.__JP.showPage('fx'); window.__JP.fxSub(1); }); await shot('chain_nokbd');
  await ev(() => window.__JP.fxSub(2)); await shot('master_nokbd');
  await prev(1);
  await ev(() => { window.__JP.fxSub(1); window.__JP.showPage('sound'); });
  for (const [n, f] of Object.entries({ guide: () => __JP.guide(), settings: () => __JP.settings(), about: () => __JP.about(), save: () => __JP.saveDlg(), browser: () => __JP.browser(), export: () => __JP.exportDlg(), import: () => __JP.importDlg(), page1: () => __JP.openPage(1), page2: () => __JP.openPage(2), page4: () => __JP.openPage(4), page6: () => __JP.openPage(6), page7: () => __JP.openPage(7), page8: () => __JP.openPage(8) })) {
    await p.evaluate(f); await p.waitForTimeout(100); await shot(n); await p.evaluate(() => window.__JP.closeDlg());
  }
  // preset menu
  await p.click('#bMenu'); await p.waitForTimeout(80); await shot('menu'); await p.mouse.click(1500, 600);
  // look variants
  for (const l of ['navy', 'soda', 'purr']) { await p.evaluate(l => { window.__JP.S.look = l; window.__JP.applyLook(); }, l); await shot('look_' + l); }
  // tooltip + value bubble
  await p.evaluate(() => { window.__JP.S.look = 'jp'; window.__JP.applyLook(); });
  await m.close();
})();
