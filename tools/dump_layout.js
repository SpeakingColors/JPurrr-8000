// Dumps the geometry of the approved mockup (docs/mockups/JPurrr-8000-mockup-v3.5.html) to data/layout.json.
// The plugin window is generated from this file (tools/gen_layout.py), so a re-dump updates the plugin.
// Usage: node tools/dump_layout.js [out.json]      (needs Playwright, no network)
const fs = require('fs'), path = require('path');
const { open, ROOT } = require('./mockup_page.js');

// Runs inside the page. Returns {items:[...]} for the currently visible state.
function walk() {
  const win = document.querySelector('#win');
  const W0 = win.getBoundingClientRect();
  const R = r => ({ x: +(r.left - W0.left).toFixed(2), y: +(r.top - W0.top).toFixed(2), w: +r.width.toFixed(2), h: +r.height.toFixed(2) });
  const vis = el => el.getClientRects().length > 0 && !el.closest('[hidden]');
  const items = [];
  const cvs = document.createElement('canvas').getContext('2d');
  const ascentCache = {};
  const fontOf = el => {
    const cs = getComputedStyle(el);
    const fam = cs.fontFamily.split(',')[0].replace(/["']/g, '').trim();
    return { fam, size: parseFloat(cs.fontSize), weight: parseInt(cs.fontWeight), ls: cs.letterSpacing === 'normal' ? 0 : +(parseFloat(cs.letterSpacing) / parseFloat(cs.fontSize)).toFixed(4), italic: cs.fontStyle === 'italic' };
  };
  const ascent = f => { const k = f.fam + f.weight + f.size + f.italic; if (!(k in ascentCache)) { cvs.font = `${f.italic ? 'italic ' : ''}${f.weight} ${f.size}px "${f.fam}"`; const m = cvs.measureText('Hx'); ascentCache[k] = [m.fontBoundingBoxAscent, m.fontBoundingBoxDescent]; } return ascentCache[k]; };
  const colourVars = ['--lbl', '--lbl2', '--ttl', '--acc', '--acc2', '--hdrfg', '--wordmark', '--lcdfg', '--lcdoff', '--selfg', '--logo', '--led', '--ledoff', '--btnbg'];
  const probe = document.createElement('i'); win.appendChild(probe);
  const norm = c => { probe.style.color = ''; probe.style.color = c; return getComputedStyle(probe).color; };
  const varVal = v => norm(`var(${v})`);
  const tokenOf = col => { for (const v of colourVars) if (varVal(v) === col) return v.slice(2); return null; };
  const rawColour = el => getComputedStyle(el).color;

  // text nodes -> lines
  const tw = document.createTreeWalker(win, NodeFilter.SHOW_TEXT);
  const texts = [];
  for (let n; (n = tw.nextNode());) if (n.nodeValue.trim()) texts.push(n);
  for (const n of texts) {
    const el = n.parentElement;
    if (!vis(el) || el.closest('select,canvas,script,style,#scrim,#tip,#val,#pmenu,#mpmenu,svg,.dlg')) continue;
    const cs = getComputedStyle(el);
    const up = cs.textTransform === 'uppercase';
    const s = n.nodeValue;
    // group chars by line (top)
    const lines = [];
    const rg = document.createRange();
    let cur = null;
    for (let i = 0; i < s.length; i++) {
      rg.setStart(n, i); rg.setEnd(n, i + 1);
      const rs = rg.getClientRects(); if (!rs.length) continue;
      const r = rs[0]; if (r.width === 0 && s[i] === ' ') { continue; }
      if (!cur || Math.abs(r.top - cur.top) > 3) { cur = { top: r.top, bottom: r.bottom, left: r.left, right: r.right, t: '' }; lines.push(cur); }
      cur.t += s[i]; cur.left = Math.min(cur.left, r.left); cur.right = Math.max(cur.right, r.right);
    }
    if (!lines.length) continue;
    const f = fontOf(el); const [asc] = ascent(f);
    const sv = el.closest('[data-mpsave]'), inK = !!el.closest('#kbd');
    // colour token across looks is resolved later (needs look switch): keep the element for now
    const it = { k: 'text', id: el.id || (el.closest('[id^=lb_]') || {}).id || (sv ? 'mpsave_' + sv.dataset.mpsave : ''), kbd: inK, f, al: 'c', cls: el.className && el.className.baseVal === undefined ? el.className : '',
      lines: lines.map(l => ({ t: (up ? l.t.toUpperCase() : l.t).trim(), x: +(l.left - W0.left).toFixed(2), y: +(l.top - W0.top).toFixed(2), w: +(l.right - l.left).toFixed(2), base: +(l.top - W0.top + Math.round(asc)).toFixed(2) })),
      rect: R(el.getBoundingClientRect()), _el: el };
    // alignment: centre when the element box is centred text
    const ta = cs.textAlign;
    it.al = (el.closest('.opt,#pname,.lcd,.plist') ? 'l' : (el.classList.contains('lb') || el.classList.contains('note') || el.closest('.ttl') || el.classList.contains('big') || el.classList.contains('sub') || el.closest('.logo small') ? 'c' : (ta === 'center' ? 'c' : 'l')));
    items.push(it);
  }
  // clean text of ids that are dynamic placeholders
  // controls
  const q = (s, fn) => document.querySelectorAll(s).forEach(el => { if (vis(el)) fn(el); });
  const tipOf = el => { const t = el.closest('[data-tip]'); return t ? t.dataset.tip : ''; };
  q('#win .cheek', el => items.push({ k: 'cheek', side: el.classList.contains('l') ? 'l' : 'r', rect: R(el.getBoundingClientRect()) }));
  q('#hdr', el => items.push({ k: 'hdr', rect: R(el.getBoundingClientRect()) }));
  q('#ftr', el => items.push({ k: 'ftr', rect: R(el.getBoundingClientRect()) }));
  q('.brand svg', el => items.push({ k: 'cat', rect: R(el.getBoundingClientRect()), tok: 'logo' }));
  q('.logo svg', el => items.push({ k: 'cat', rect: R(el.getBoundingClientRect()), tok: 'logo' }));
  q('#hdr .hb', el => { const bs = el.closest('#pmenu'); if (bs) return; items.push({ k: 'hbtn', id: el.id, tab: el.classList.contains('tab'), sq: el.classList.contains('sq'), on: el.classList.contains('on'), t: el.textContent.trim(), rect: R(el.getBoundingClientRect()), tip: el.dataset.tip || '' }); });
  q('#bEditParts', el => items.push({ k: 'hbtn', id: el.id, tab: false, on: el.classList.contains('on'), t: el.textContent.trim(), rect: R(el.getBoundingClientRect()), tip: el.dataset.tip || '' }));   // v3.5
  q('.fxtabs .hb', el => items.push({ k: 'hbtn', id: el.id, tab: true, on: el.classList.contains('on'), t: el.textContent.trim(), rect: R(el.getBoundingClientRect()), tip: el.dataset.tip || '' }));
  q('#pname', el => items.push({ k: 'pname', rect: R(el.getBoundingClientRect()), tip: el.dataset.tip, f: fontOf(el) }));
  q('#pinfo', el => items.push({ k: 'pinfo', rect: R(el.getBoundingClientRect()), f: fontOf(el) }));
  q('#msg', el => items.push({ k: 'msg', rect: R(el.parentElement.getBoundingClientRect()), f: fontOf(el) }));
  q('.sec', el => { const ttl = el.querySelector('.ttl'), sp = el.querySelector('.ttl span');
    items.push({ k: 'sec', rect: R(el.getBoundingClientRect()), title: sp.textContent, ttl: R(ttl.getBoundingClientRect()), span: R(sp.getBoundingClientRect()), f: fontOf(ttl) }); });
  // v3.3: SAVE and "..." buttons next to each FX block preset select
  q('[data-mpsave],[data-mpmore]', el => { const more = el.hasAttribute('data-mpmore'), mp = el.dataset.mpsave || el.dataset.mpmore;
    items.push({ k: 'bbtn', mp, more, id: (more ? 'mpmore_' : 'mpsave_') + mp, t: more ? '...' : el.textContent.trim().toUpperCase(), rect: R(el.getBoundingClientRect()), f: fontOf(el), tip: el.dataset.tip || '' }); });
  // v3.3: the keyboard strip with the BEND and MOD wheels and the keys (texts are dumped by the text walker with kbd:true)
  q('#kbd', el => {
    const wheels = ['wBend', 'wMod'].map(id => { const w = document.getElementById(id), col = w.closest('.wcol'), b = w.querySelector('b'), h = w.querySelector('i');
      return { id, pitch: id === 'wBend', rect: R(w.getBoundingClientRect()), handle: R(h.getBoundingClientRect()), line: b ? R(b.getBoundingClientRect()) : null, name: R(col.querySelector('.wnm').getBoundingClientRect()),
        scale: R(col.querySelector('.wsc').getBoundingClientRect()), wn: parseFloat(getComputedStyle(w).getPropertyValue('--wn')), tip: w.dataset.tip, aria: w.getAttribute('aria-label') }; });
    const keys = [...document.querySelectorAll('#keys .wk, #keys .bk')].map(k => ({ n: +k.dataset.n, black: k.classList.contains('bk'), rect: R(k.getBoundingClientRect()) }));
    const labels = [...document.querySelectorAll('#keys .kl')].map(k => ({ n: +k.parentElement.dataset.n, t: k.textContent }));
    const knobs = [...document.querySelectorAll('#kbd .knob')].map(k => ({ id: k.dataset.id, rect: R(k.getBoundingClientRect()), tip: tipOf(k), tipRect: R((k.closest('[data-tip]') || k).getBoundingClientRect()) }));
    items.push({ k: 'kbd', rect: R(el.getBoundingClientRect()), keysRect: R(document.querySelector('#keys').getBoundingClientRect()), whls: R(document.querySelector('#whls').getBoundingClientRect()), wheels, knobs, keys, labels }); });
  q('.mbp', el => items.push({ k: 'mbp', rect: R(el.getBoundingClientRect()) }));
  q('.knob', el => { if (el.closest('#kbd')) return; items.push({ k: 'knob', id: el.dataset.id, rect: R(el.getBoundingClientRect()), tip: tipOf(el) }); });   // v3.8: BEND UP / DOWN live in the strip
  q('.sld', el => items.push({ k: 'slider', id: el.dataset.id, hh: +el.dataset.h, tk: parseFloat(el.style.getPropertyValue('--tk')), rect: R(el.getBoundingClientRect()), tip: tipOf(el) }));
  q('.sw', el => { if (el.closest('#pmenu')) return; items.push({ k: 'switch', id: el.dataset.id || el.id, mom: el.classList.contains('mom'), rect: R(el.getBoundingClientRect()), tip: tipOf(el) || el.dataset.tip || '' }); });
  q('.opts', el => {
    const opts = [...el.querySelectorAll('.opt')].map(b => { const led = b.querySelector('.led'), r = b.getBoundingClientRect(); return { v: +b.dataset.v, r: R(r), led: R(led.getBoundingClientRect()) }; });
    items.push({ k: 'opts', id: el.dataset.id, rect: R(el.getBoundingClientRect()), opts, tip: tipOf(el) });
  });
  q('select', el => { if (el.closest('.dlg')) return; const cs = getComputedStyle(el); items.push({ k: 'select', id: el.dataset.id || '', mp: el.dataset.mp || '', rect: R(el.getBoundingClientRect()), f: fontOf(el), n: el.options.length, text: el.options[el.selectedIndex] ? el.options[el.selectedIndex].text : '', tip: tipOf(el) }); });
  q('canvas.cv', el => items.push({ k: 'canvas', id: el.id, cw: +el.dataset.w, ch: +el.dataset.h, rect: R(el.getBoundingClientRect()), tip: tipOf(el) }));
  q('#lcd', el => items.push({ k: 'lcd', rect: R(el.getBoundingClientRect()), l1: R(document.querySelector('#l1').getBoundingClientRect()), l2: R(document.querySelector('#l2').getBoundingClientRect()), f: fontOf(el) }));
  q('.mbl', el => { const r = el.getBoundingClientRect(); items.push({ k: 'vtext', t: el.textContent, rect: R(r), f: fontOf(el), tok: 'lbl2' }); });
  q('.note#o1info,#osv,#kshv', el => { });
  // hover tip regions (data-tip) and knob "for" ids
  q('[data-tip]', el => { if (el.closest('#hdr') || el.closest('#kbd')) return; items.push({ k: 'tip', rect: R(el.getBoundingClientRect()), tip: el.dataset.tip, for: el.dataset.for || '' }); });
  probe.remove();
  return items.map(i => { const o = Object.assign({}, i); delete o._el; return o; });
}


// v3.5: the PARTS band (LOWER card, KEYS map, UPPER card). Runs inside the page with the band open in SPLIT. x = window x, y = relative to the top of the band
// (the band starts 8 px under the panels of the SOUND page); inner rectangles are relative to their card or map. The texts are dynamic (sound names,
// chips, key mode) and are drawn by the plugin: only their boxes and baselines are taken here.
function dumpParts() {
  const win = document.querySelector('#win'), W0 = win.getBoundingClientRect(), band = document.querySelector('#parts'), B0 = band.getBoundingClientRect();
  const rr = (r, ox, oy) => ({ x: +(r.left - W0.left - (ox || 0)).toFixed(2), y: +(r.top - B0.top - (oy || 0)).toFixed(2), w: +r.width.toFixed(2), h: +r.height.toFixed(2) });
  const cvs = document.createElement('canvas').getContext('2d');
  const asc = el => { const cs = getComputedStyle(el); const fam = cs.fontFamily.split(',')[0].replace(/["']/g, '').trim(); cvs.font = `${cs.fontWeight} ${cs.fontSize} "${fam}"`; return Math.round(cvs.measureText('Hx').fontBoundingBoxAscent); };
  const base = (el, ox, oy) => { const n = [...el.childNodes].find(n => n.nodeType === 3 && n.nodeValue.trim()); if (!n) return null; const rg = document.createRange(); rg.setStart(n, 0); rg.setEnd(n, 1); const r = rg.getClientRects()[0]; return +(r.top - B0.top - (oy || 0) + asc(el)).toFixed(2); };
  const el = (e, ox, oy) => e ? Object.assign(rr(e.getBoundingClientRect(), ox, oy), { base: base(e, ox, oy) }) : null;
  const sec = band.getBoundingClientRect(), ttl = band.querySelector('.ttl');
  const out = { sec: { rect: rr(sec), title: band.querySelector('.ttl span').textContent, ttl: rr(ttl.getBoundingClientRect()), span: rr(ttl.querySelector('span').getBoundingClientRect()) }, cards: {}, btns: [] };
  for (const k of ['l', 'u']) {
    const c = band.querySelector('.pt-card.' + k), cr = c.getBoundingClientRect(), ox = cr.left - W0.left, oy = cr.top - B0.top;
    const o = { rect: rr(cr), led: el(c.querySelector('.led'), ox, oy), title: el(c.querySelector('.pt-h b'), ox, oy), role: el(c.querySelector('.pt-role'), ox, oy), ed: el(c.querySelector('.pt-ed'), ox, oy),
      prev: el(c.querySelector('[data-a=prev]'), ox, oy), name: el(c.querySelector('.pt-name'), ox, oy), next: el(c.querySelector('[data-a=next]'), ox, oy), browse: el(c.querySelector('button[data-a=browse]'), ox, oy),
      chipsRow: el(c.querySelector('.pt-chips'), ox, oy), chip: el(c.querySelector('.pt-chip:not(.id)'), ox, oy), chipId: el(c.querySelector('.pt-chip.id'), ox, oy) };
    out.cards[k] = o;
    for (const a of ['prev', 'next', 'browse']) { const b = c.querySelector(a === 'browse' ? 'button[data-a=browse]' : '[data-a=' + a + ']'); const n = [...b.childNodes].find(n => n.nodeType === 3);
      const rg = document.createRange(); rg.setStart(n, 0); rg.setEnd(n, n.nodeValue.length); const tr = rg.getBoundingClientRect(); const cs = getComputedStyle(b);
      out.btns.push({ id: 'pt_' + a + '_' + k, a, card: k, t: b.textContent.trim(), rect: rr(b.getBoundingClientRect()), tip: b.dataset.tip, sq: b.classList.contains('sq'), line: { x: +(tr.left - W0.left).toFixed(2), w: +tr.width.toFixed(2), base: +(tr.top - B0.top + asc(b)).toFixed(2) },
        f: { size: parseFloat(cs.fontSize), weight: parseInt(cs.fontWeight), ls: cs.letterSpacing === 'normal' ? 0 : +(parseFloat(cs.letterSpacing) / parseFloat(cs.fontSize)).toFixed(4) } }); }
  }
  const m = band.querySelector('.pt-map'), mr = m.getBoundingClientRect(), mx = mr.left - W0.left, my = mr.top - B0.top;
  out.map = { rect: rr(mr), title: el(m.querySelector('.pm-h b'), mx, my), pill: el(m.querySelector('.pill'), mx, my), text: el(m.querySelector('.pm-h > span:not(.pill):not(.sp)'), mx, my), sp: el(m.querySelector('.pm-h .sp'), mx, my),
    mini: el(m.querySelector('.mini'), mx, my), oct: el(m.querySelector('.pm-oct'), mx, my), octI: el(m.querySelector('.pm-oct i'), mx, my) };
  out.tips = { card: band.querySelector('.pt-card.u').dataset.tip, map: m.dataset.tip, name: band.querySelector('.pt-name').dataset.tip };
  out.splitTip = document.querySelector('#spH').dataset.tip;
  out.h = +B0.height.toFixed(2);
  return out;
}

// v3.8: the player bar (#plr), dumped with PLAYER on. x = window x, y = relative to the top of the bar. Texts that change (piece name, group, BPM, readouts)
// are drawn by the plugin: only their boxes, fonts and baselines are taken here.
function dumpPlayer() {
  const win = document.querySelector('#win'), W0 = win.getBoundingClientRect(), bar = document.querySelector('#plr'), B0 = bar.getBoundingClientRect();
  const rr = r => ({ x: +(r.left - W0.left).toFixed(2), y: +(r.top - B0.top).toFixed(2), w: +r.width.toFixed(2), h: +r.height.toFixed(2) });
  const cvs = document.createElement('canvas').getContext('2d');
  const fontOf = el => { const cs = getComputedStyle(el); const fam = cs.fontFamily.split(',')[0].replace(/["']/g, '').trim();
    return { fam, size: parseFloat(cs.fontSize), weight: parseInt(cs.fontWeight), ls: cs.letterSpacing === 'normal' ? 0 : +(parseFloat(cs.letterSpacing) / parseFloat(cs.fontSize)).toFixed(4), italic: false }; };
  const met = el => { const cs = getComputedStyle(el); const fam = cs.fontFamily.split(',')[0].replace(/["']/g, '').trim(); cvs.font = `${cs.fontWeight} ${cs.fontSize} "${fam}"`; const m = cvs.measureText('Hx'); return [m.fontBoundingBoxAscent, m.fontBoundingBoxDescent]; };
  const textNode = el => [...el.childNodes].find(n => n.nodeType === 3 && n.nodeValue.trim());
  const base = el => { const n = textNode(el); if (!n) return null; const rg = document.createRange(); rg.setStart(n, 0); rg.setEnd(n, 1); const r = rg.getClientRects()[0]; return +(r.top - B0.top + Math.round(met(el)[0])).toFixed(2); };
  const tline = el => { const n = textNode(el); if (!n) return null; const rg = document.createRange(); rg.setStart(n, 0); rg.setEnd(n, n.nodeValue.length); const r = rg.getBoundingClientRect(); return { t: n.nodeValue.trim(), x: +(r.left - W0.left).toFixed(2), w: +r.width.toFixed(2), base: base(el) }; };
  const inputBase = el => { const [a, d] = met(el), r = el.getBoundingClientRect(); return +(r.top - B0.top + r.height / 2 + (Math.round(a) - Math.round(d)) / 2).toFixed(2); };
  const tipOf = el => { const t = el.closest('[data-tip]'); return t ? t.dataset.tip : ''; };
  const it = (id, el, o) => Object.assign({ id, rect: rr(el.getBoundingClientRect()), tip: tipOf(el), f: fontOf(el), line: tline(el) }, o || {});
  const items = [];
  const $1 = s => document.querySelector(s);
  items.push(it('title', $1('#plr .pl-t')));
  const play = $1('#plPlay'); items.push(it('play', play, { icon: rr($1('#plPlay .i-play').getBoundingClientRect()), lbl: it('lbl', $1('#plPlay span')) }));
  items.push(it('prev', $1('#plPrev'), { icon: rr($1('#plPrev svg').getBoundingClientRect()) }));
  items.push(it('sel', $1('#plSel'), { name: it('name', $1('#plName'), { base: base($1('#plName')) }), grp: it('grp', $1('#plGrp'), { base: base($1('#plGrp')) }), chev: rr($1('#plSel svg').getBoundingClientRect()), pad: parseFloat(getComputedStyle($1('#plSel')).paddingLeft) }));
  items.push(it('next', $1('#plNext'), { icon: rr($1('#plNext svg').getBoundingClientRect()) }));
  [...document.querySelectorAll('#plr .pl-sep')].forEach((e, i) => items.push(it('sep' + i, e)));
  const grp = id => $1(id).closest('.plg1');
  items.push(it('bpmG', grp('#plBpm'))); items.push(it('bpmL', grp('#plBpm').querySelector('.pl-l')));
  items.push(it('bpm', $1('#plBpm'), { base: inputBase($1('#plBpm')) }));
  items.push(it('lockG', grp('#plLock'))); items.push(it('lock', $1('#plLock'), { led: rr($1('#plLock .led').getBoundingClientRect()) })); items.push(it('lockL', grp('#plLock').querySelector('.pl-l')));
  items.push(it('trG', grp('#plTr'))); items.push(it('trL', grp('#plTr').querySelector('.pl-l')));
  items.push(it('tr', $1('#plTr'), { base: inputBase($1('#plTr')) }));
  items.push(it('load', $1('#plLoad'))); items.push(it('exp', $1('#plExp')));
  const pc = $1('#plPos'); items.push(it('posG', grp('#plPos'))); items.push(it('pos', pc, { cw: +pc.dataset.w, ch: +pc.dataset.h }));
  const pt = $1('#plPosT'), sm = pt.querySelector('small'), vn = [...pt.childNodes].find(n => n.nodeType === 3);
  const vrg = document.createRange(); vrg.setStart(vn, 0); vrg.setEnd(vn, vn.nodeValue.length); const vr = vrg.getBoundingClientRect();
  items.push(it('posT', pt, { small: it('small', sm), val: { x: +(vr.left - W0.left).toFixed(2), y: +(vr.top - B0.top).toFixed(2), w: +vr.width.toFixed(2), h: +vr.height.toFixed(2), f: fontOf(pt), base: +(vr.top - B0.top + Math.round(met(pt)[0])).toFixed(2) } }));
  const cs = getComputedStyle(bar);
  return { rect: rr(B0), h: +B0.height.toFixed(2), radius: parseFloat(cs.borderTopLeftRadius), items };
}
// the piece menu (#plMenu): constants of its CSS, dumped with the menu open
function dumpPlayerMenu() {
  __JPplayer.openMenu();
  const m = document.querySelector('#plMenu'), g = m.querySelector('.plg'), r = m.querySelector('.pli'), cm = getComputedStyle(m), cg = getComputedStyle(g), cr = getComputedStyle(r);
  const fo = (el, sel) => { const e = sel ? el.querySelector(sel) : el, cs = getComputedStyle(e); return { fam: cs.fontFamily.split(',')[0].replace(/["']/g, '').trim(), size: parseFloat(cs.fontSize), weight: parseInt(cs.fontWeight), ls: cs.letterSpacing === 'normal' ? 0 : +(parseFloat(cs.letterSpacing) / parseFloat(cs.fontSize)).toFixed(4), italic: false }; };
  const o = { w: m.getBoundingClientRect().width, border: parseFloat(cm.borderTopWidth), radius: parseFloat(cm.borderTopLeftRadius), gh: g.getBoundingClientRect().height, gpad: parseFloat(cg.paddingLeft), rh: r.getBoundingClientRect().height,
    rpadL: parseFloat(cr.paddingLeft), rpadR: parseFloat(cr.paddingRight), rgap: parseFloat(cr.columnGap), gf: fo(g, 'span'), gsf: fo(g, 'small'), rf: fo(r, 'span'), rsf: fo(r, 'small'), above: 6, minTop: 76, maxH: 640, minH: 240, left: 40 };
  __JPplayer.closeMenu();
  return o;
}

(async () => {
  const outFile = process.argv[2] || path.join(ROOT, 'data/layout.json');
  const m = await open(); const p = m.page;
  const out = { design: { w: 2920 }, looks: {}, pages: {}, dyn: {} };

  // look variables
  const varNames = ['pnl', 'pnl2', 'pnl3', 'edge', 'sbd', 'hi', 'ttl', 'lbl', 'lbl2', 'knob', 'knobhi', 'ind', 'cap', 'led', 'ledoff', 'acc', 'acc2', 'lcdbg', 'lcdfg', 'lcdoff', 'hdr', 'hdrfg', 'sel', 'selfg', 'dsp', 'dspline', 'dspfill', 'dspgrid', 'dspaccent', 'logo', 'btnbg', 'btnbd', 'wordmark'];
  const looks = ['jp', 'navy', 'soda', 'purr'];
  out.looks = await p.evaluate(({ looks, varNames }) => {
    const win = document.querySelector('#win'); const probe = document.createElement('i'); win.appendChild(probe); const res = {};
    for (const l of looks) { if (l === 'jp') win.removeAttribute('data-look'); else win.dataset.look = l;
      res[l] = {}; for (const v of varNames) { probe.style.color = ''; probe.style.background = ''; probe.style.color = `var(--${v})`; const c = getComputedStyle(probe).color; res[l][v] = c; } }
    win.removeAttribute('data-look'); probe.remove(); return res; }, { looks, varNames });

  // Walk, then a second page pass that fills the colour token of every text node (same order and filter).
  async function dumpPage(name) {
    const items = await p.evaluate(walk);
    const tokens = await p.evaluate(({ looks, varNames }) => {
      const win = document.querySelector('#win');
      const vis = el => el.getClientRects().length > 0 && !el.closest('[hidden]');
      const tw = document.createTreeWalker(win, NodeFilter.SHOW_TEXT); const els = [];
      for (let n; (n = tw.nextNode());) { if (!n.nodeValue.trim()) continue; const el = n.parentElement; if (!vis(el) || el.closest('select,canvas,script,style,#scrim,#tip,#val,#pmenu,#mpmenu,svg,.dlg')) continue; els.push(el); }
      const probe = document.createElement('i'); win.appendChild(probe);
      const per = [];
      for (const l of looks) { if (l === 'jp') win.removeAttribute('data-look'); else win.dataset.look = l;
        const vv = {}; for (const v of varNames) { probe.style.color = `var(--${v})`; vv[v] = getComputedStyle(probe).color; }
        per.push({ vv, cols: els.map(e => getComputedStyle(e).color) }); }
      win.removeAttribute('data-look'); probe.remove();
      return els.map((e, i) => { const matches = varNames.filter(v => per.every(p => p.vv[v] === p.cols[i])); return { matches, col: per[0].cols[i] }; });
    }, { looks, varNames });
    return { items, tokens };
  }

  async function capture(name) {
    const { items, tokens } = await dumpPage(name);
    return { items, tokens, texts: items.filter(i => i.k === 'text') };
  }

  const pages = {};
  async function cap(name) {
    const c = await capture(name);
    // align: tokens may be longer (text nodes with no measurable lines). Re-run alignment by count if equal, else best effort by order.
    let ti = 0;
    const texts = c.texts;
    if (c.tokens.length !== texts.length) console.error(`token/text count differs on ${name}: ${c.tokens.length} vs ${texts.length}`);
    texts.forEach((t, i) => { const tk = c.tokens[i]; if (tk) { t.tok = tk.matches.length ? (tk.matches.includes('lbl') ? 'lbl' : tk.matches[0]) : null; t.col = tk.col; } });
    pages[name] = c.items;
    // window height with the strip shown (default) and with PREVIEW off, and where the strip starts
    const hs = await p.evaluate(() => { const J = window.__JP, w = document.querySelector('#win'); const k = document.querySelector('#kbd'); const shown = w.offsetHeight, sy = k.getBoundingClientRect().top - w.getBoundingClientRect().top;
      J.setP('prev', 0, true); const bare = w.offsetHeight; J.setP('prev', 1, true); return { shown, bare, stripY: +sy.toFixed(2) }; });
    (out.pageHk = out.pageHk || {})[name] = hs.shown; (out.pageH = out.pageH || {})[name] = hs.bare; (out.stripY = out.stripY || {})[name] = hs.stripY;
  }

  // global state: dump the sound page
  await p.evaluate(() => { window.__JP.showPage('sound'); });
  await p.waitForTimeout(100);
  await cap('sound');
  await p.evaluate(() => { window.__JP.showPage('fx'); window.__JP.fxSub(1); }); await p.waitForTimeout(100);
  await cap('chain');
  await p.evaluate(() => { window.__JP.fxSub(2); }); await p.waitForTimeout(100);
  await cap('master');
  await p.evaluate(() => { window.__JP.fxSub(1); window.__JP.showPage('sound'); });

  // page sizes: measured in cap(). pageH = content only (PREVIEW off), pageHk = with the keyboard strip. Check the strip sits 8 px under the panels.
  for (const [name, items] of Object.entries(pages)) {
    let bot = 0; for (const i of items) if (i.k === 'sec' || i.k === 'mbp') bot = Math.max(bot, i.rect.y + i.rect.h);
    const ftr = items.find(i => i.k === 'ftr');
    if (Math.round(bot + 4 + ftr.rect.h) !== out.pageH[name]) console.error('page height differs from panel bottom + 4 + footer on', name, bot, out.pageH[name]);
    if (Math.abs(out.stripY[name] - (Math.ceil(bot - 0.005) + 8)) > 0.01) console.error('strip is not 8 px under the panels on', name, bot, out.stripY[name]);
  }
  // split chrome (common to all pages) from the page items
  const key = i => JSON.stringify([i.k, i.id || '', i.t || '', i.rect && i.rect.x, i.rect && i.rect.y]);
  const chromeKinds = new Set(['cheek', 'hdr', 'ftr']);
  out.chrome = pages.sound.filter(i => chromeKinds.has(i.k) || (i.rect && i.rect.y < 56 && i.k !== 'tip') || (i.k === 'text' && i.rect.y < 56)).filter(i => !(i.k === 'hbtn' && ['tfx1', 'tfx2'].includes(i.id)));
  const chromeKeys = new Set(out.chrome.map(key));
  const chromeRect = i => i.rect && (i.k === 'cheek' || i.k === 'hdr' || i.k === 'ftr');
  // footer text lines live at the bottom; treat as chrome
  const ftrY = pages.sound.find(i => i.k === 'ftr').rect.y; const isFtrText = i => i.k === 'text' && i.rect.y >= ftrY && !i.kbd;
  out.chrome = out.chrome.concat(pages.sound.filter(isFtrText));
  // the keyboard strip: same on every page except its y. Taken from the sound page, y made relative to the strip top.
  const isStrip = i => i.k === 'kbd' || i.kbd;
  const stripOf = (name, items) => items.filter(isStrip);
  const sy = out.stripY.sound;
  for (const n of ['chain', 'master']) { const a = pages.sound.find(i => i.k === 'kbd'), b = pages[n].find(i => i.k === 'kbd');
    if (a.rect.x !== b.rect.x || a.rect.w !== b.rect.w || a.rect.h !== b.rect.h || a.keys.length !== b.keys.length || stripOf('sound', pages.sound).length !== stripOf(n, pages[n]).length) console.error('strip differs between sound and', n); }
  const dyRect = r => r && Object.assign({}, r, { y: +(r.y - sy).toFixed(2) });
  const kb = pages.sound.find(i => i.k === 'kbd');
  out.strip = { rect: dyRect(kb.rect), keysRect: dyRect(kb.keysRect), whls: dyRect(kb.whls),
    wheels: kb.wheels.map(w => Object.assign({}, w, { rect: dyRect(w.rect), handle: dyRect(w.handle), line: dyRect(w.line), name: dyRect(w.name), scale: dyRect(w.scale) })),
    knobs: kb.knobs.map(k => Object.assign({}, k, { rect: dyRect(k.rect), tipRect: dyRect(k.tipRect) })),
    keys: kb.keys.map(k => Object.assign({}, k, { rect: dyRect(k.rect) })), labels: kb.labels,
    texts: pages.sound.filter(i => i.kbd).map(t => Object.assign({}, t, { rect: dyRect(t.rect), lines: t.lines.map(l => Object.assign({}, l, { y: +(l.y - sy).toFixed(2), base: +(l.base - sy).toFixed(2) })) })) };
  for (const [name, items] of Object.entries(pages)) {
    out.pages[name] = items.filter(i => !chromeKeys.has(key(i)) && !isFtrText(i) && !isStrip(i) && !(i.k === 'hdr' || i.k === 'ftr' || i.k === 'cheek'));
  }
  out.design.chromeH = out.pageHk.sound;
  // cheek heights depend on the window height: stored as x,w only (full height at runtime)
  // tip regions in the header are skipped (hbtn carries its own tip)

  // ---- data shared with the C++ side: option texts, preset menus, sound type values, parameter pages ----
  out.dyn = await p.evaluate(() => {
    const J = window.__JP;
    const mpd = {}; for (const [k, m] of Object.entries(J.MPD)) mpd[k] = { title: m.title, ids: m.ids, list: m.list.map(pr => ({ n: pr[0], v: Object.fromEntries(m.ids.map(id => [id, (id in pr[1]) ? pr[1][id] : J.DEFS[id]])) })) };
    const qv = {}; ['Lead', 'Pad', 'Bass', 'Pluck', 'Chord', 'FX'].forEach(t => qv[t] = J.qvals(t));
    const secp = {}; document.querySelectorAll('.mpsel').forEach(s => { secp[s.dataset.mp] = true; });
    const fmt = {}; J.ORDER.forEach(id => { const q = J.P[id]; if (q.fmt && q.max - q.min <= 600) { const a = []; for (let v = q.min; v <= q.max; v++) a.push(q.fmt(v)); fmt[id] = a; } });
    const factoryMeta = J.FACTORY.map(f => ({ name: f.name, type: f.type, genres: f.genres, slot: f.slot, start: f.start }));
    return { fmt, mpd, qvals: qv, o1l: J.O1L, o2l: J.O2L, o1w: J.O1W, vmc: J.VMC, pages: J.PAGES, banks: J.BANKS, types: J.TYPES, genres: J.GENRES, gpats: J.GPATS, factoryMeta,
      scope: J.ORDER.filter(id => !J.P[id].hid && ['k', 's', 'o', 'd', 'w'].includes(J.P[id].t)) };
  });
  // parameter metadata needed by the pages (labels, tips of controls without .ctl)
  out.design.h = out.pageH; out.design.hk = out.pageHk;

  // ---- dialogs (reference trees, measured with the window at scale 1) ----
  const dlgTree = () => {
    const win = document.querySelector('#win'); const W0 = win.getBoundingClientRect();
    const dlg = document.querySelector('#scrim .dlg'); if (!dlg) return null;
    const z = parseFloat(getComputedStyle(dlg).zoom) || 1;
    const out = [];
    const walkEl = (el, depth) => {
      const r = el.getBoundingClientRect(); const cs = getComputedStyle(el);
      const own = [...el.childNodes].filter(n => n.nodeType === 3 && n.nodeValue.trim()).map(n => n.nodeValue.trim()).join(' ');
      out.push({ d: depth, tag: el.tagName.toLowerCase(), cls: el.className && el.className.baseVal === undefined ? el.className : '', id: el.id, x: +(r.left - W0.left).toFixed(1), y: +(r.top - W0.top).toFixed(1), w: +r.width.toFixed(1), h: +r.height.toFixed(1), text: own.slice(0, 90), fs: cs.fontSize, ff: cs.fontFamily.split(',')[0], fw: cs.fontWeight, col: cs.color, bg: cs.backgroundColor, bd: cs.borderTopWidth + ' ' + cs.borderTopColor });
      [...el.children].forEach(c => walkEl(c, depth + 1));
    };
    walkEl(dlg, 0);
    const sc = document.querySelector('#scrim');
    return { zoom: z, scrim: sc.className, tree: out };
  };
  out.dialogs = {};
  const dlgs = { guide: () => __JP.guide(), settings: () => __JP.settings(), about: () => __JP.about(), save: () => __JP.saveDlg(), browser: () => __JP.browser(), export: () => __JP.exportDlg(), import: () => __JP.importDlg(), page1: () => __JP.openPage(1), page4: () => __JP.openPage(4), page6: () => __JP.openPage(6), page7: () => __JP.openPage(7), page8: () => __JP.openPage(8), del: () => { __JP.loadPreset(__JP.FACTORY[0]); __JP.openDlg('Delete this sound?', '<p>x</p>', [{ t: 'Delete', go: 1 }, { t: 'Close' }]); } };
  for (const [name] of Object.entries(dlgs)) {
    await p.evaluate(n => { const J = window.__JP; const f = { guide: J.guide, settings: J.settings, about: J.about, save: J.saveDlg, browser: J.browser, export: J.exportDlg, import: J.importDlg, page1: () => J.openPage(1), page4: () => J.openPage(4), page6: () => J.openPage(6), page7: () => J.openPage(7), page8: () => J.openPage(8) }[n]; if (f) f(); }, name);
    await p.waitForTimeout(80);
    const t = await p.evaluate(dlgTree);
    if (t) out.dialogs[name] = t;
    await p.evaluate(() => window.__JP.closeDlg());
  }
  // v3.5: the PARTS band. Window heights with the band open (SOUND page only): strip shown and PREVIEW off.
  await p.evaluate(() => { __JPparts.setParts(true); __JP.setP('km', 2, true); }); await p.waitForTimeout(200);
  out.parts = await p.evaluate(dumpParts);
  const hp = await p.evaluate(() => { const J = window.__JP, w = document.querySelector('#win'), k = document.querySelector('#kbd'); const shown = w.offsetHeight, sy = k.getBoundingClientRect().top - w.getBoundingClientRect().top;
    J.setP('prev', 0, true); const bare = w.offsetHeight; J.setP('prev', 1, true); return { shown, bare, stripY: +sy.toFixed(2) }; });
  out.parts.hOpen = hp.shown; out.parts.hOpenBare = hp.bare; out.parts.stripYOpen = hp.stripY;
  out.parts.top = out.stripY.sound;                       // y of the band in the window = where the strip starts when the band is closed
  await p.evaluate(() => { __JPparts.setParts(false); __JP.setP('km', 0, true); });
  // v3.8: the player bar (SOUND page, PARTS closed). Window heights with the bar: strip shown and PREVIEW off.
  await p.evaluate(() => { __JP.setP('player', 1, false); }); await p.waitForTimeout(200);
  out.player = await p.evaluate(dumpPlayer);
  out.player.menu = await p.evaluate(dumpPlayerMenu);
  out.player.playOn = await p.evaluate(() => { __JPplayer.demo(21); const pl = document.querySelector('#plPlay'), B0 = document.querySelector('#plr').getBoundingClientRect(), W0 = document.querySelector('#win').getBoundingClientRect();
    const rr = r => ({ x: +(r.left - W0.left).toFixed(2), y: +(r.top - B0.top).toFixed(2), w: +r.width.toFixed(2), h: +r.height.toFixed(2) }), sp = pl.querySelector('span'), n = [...sp.childNodes].find(n => n.nodeType === 3), rg = document.createRange(); rg.setStart(n, 0); rg.setEnd(n, n.nodeValue.length); const tr = rg.getBoundingClientRect();
    const r = { icon: rr(pl.querySelector('.i-stop').getBoundingClientRect()), lbl: rr(sp.getBoundingClientRect()), t: n.nodeValue, x: +(tr.left - W0.left).toFixed(2), w: +tr.width.toFixed(2) }; __JPplayer.stop(); return r; });
  const hq = await p.evaluate(() => { const J = window.__JP, w = document.querySelector('#win'), k = document.querySelector('#kbd'), b = document.querySelector('#plr'); const shown = w.offsetHeight, sy = k.getBoundingClientRect().top - w.getBoundingClientRect().top, by = b.getBoundingClientRect().top - w.getBoundingClientRect().top;
    J.setP('prev', 0, true); const bare = w.offsetHeight, byBare = b.getBoundingClientRect().top - w.getBoundingClientRect().top; J.setP('prev', 1, true); return { shown, bare, stripY: +sy.toFixed(2), barY: +by.toFixed(2), barYBare: +byBare.toFixed(2) }; });
  out.player.hShown = hq.shown; out.player.hBare = hq.bare; out.player.stripY = hq.stripY; out.player.barY = hq.barY; out.player.barYBare = hq.barYBare;
  out.player.extra = hq.shown - out.pageHk.sound;
  await p.evaluate(() => { __JP.setP('player', 0, false); });
  // tooltips and value bubble geometry
  out.bubbles = await p.evaluate(() => { const win = document.querySelector('#win'); const tip = document.querySelector('#tip'); tip.hidden = false; tip.innerHTML = 'Tip text<br><b>Now: 12</b> · double click resets'; const r = tip.getBoundingClientRect(); const cs = getComputedStyle(tip); tip.hidden = true; return { tipPad: cs.padding, tipZoom: cs.zoom, w: r.width, h: r.height }; });
  fs.mkdirSync(path.dirname(outFile), { recursive: true });
  fs.writeFileSync(outFile, JSON.stringify(out));
  const n = Object.fromEntries(Object.entries(out.pages).map(([k, v]) => [k, v.length]));
  console.log('layout written', outFile, 'items', JSON.stringify(n), 'chrome', out.chrome.length, 'pageH', JSON.stringify(out.pageH), 'pageHk', JSON.stringify(out.pageHk), 'stripY', JSON.stringify(out.stripY), 'size', fs.statSync(outFile).size);
  await m.close();
})().catch(e => { console.error(e); process.exit(1); });
