/* JPurrr-8000 melody generator: uplifting trance melodies in the style of the 22 melodies of the player library.
   Source file. tools/embed_melody_gen.js puts the learned data (data/melody_gen.json) into it, writes tools/melody_gen.js and embeds that into the mockup.
   The plugin has the same algorithm in core/src/melody_gen.cpp; both give the same notes for the same settings (checked by tests/test_melody_gen.cpp).
   Nothing is copied from the 22 melodies: only their bar rhythms and chord loops are used, the notes are chosen new.
   generate({seed, key, mode, style}) -> {name, bpm, beats, ev:[[beat, length in beats, MIDI note, velocity]], info}
     seed   1 to 16777215 (the number shown as #seed)        key   0..11 (C = 0)
     mode   0 minor, 1 major, 2 either                       style 0 any, 1 anthem, 2 run, 3 roller, 4 stabs, 5 sparse            */
(function (root) {
  'use strict';
  const DATA = /*DATA*/{"note":"Learned from the 22 uplifting trance melodies (rhythms and chord loops only). Written by tools/build_melody_gen.py; do not edit by hand.","families":["anthem","run","roller","stabs","sparse"],"rhythms":{"anthem":[{"w":15,"r":[[0,6],[6,2],[8,4],[12,2],[14,2]]},{"w":5,"r":[[0,4],[4,4],[8,4],[12,2],[14,2]]},{"w":3,"r":[[0,4],[4,4],[8,6],[14,2]]},{"w":3,"r":[[0,2],[2,2],[4,4],[8,4],[12,2],[14,2]]},{"w":3,"r":[[0,2],[2,2],[4,8],[12,2],[14,2]]},{"w":3,"r":[[0,2],[2,2],[4,2],[6,2],[8,6],[14,2]]},{"w":2,"r":[[0,4],[4,4],[8,4],[12,4]]},{"w":2,"r":[[0,2],[2,2],[4,2],[6,2],[8,4]]},{"w":1,"r":[[0,6],[6,2],[8,6],[14,2]]},{"w":1,"r":[[0,6],[6,2],[8,8]]},{"w":1,"r":[[0,2],[2,6],[8,6],[14,2]]},{"w":1,"r":[[0,4],[4,2],[6,2],[8,6],[14,2]]},{"w":1,"r":[[0,2],[2,6],[8,2],[10,2],[12,4]]},{"w":1,"r":[[0,4],[4,4],[8,6]]},{"w":1,"r":[[0,2],[2,2],[4,4],[8,4],[12,4]]},{"w":1,"r":[[0,2],[2,2],[4,2],[6,2],[8,6]]}],"run":[{"w":21,"r":[[0,2],[2,2],[4,2],[6,2],[8,2],[10,2],[12,2],[14,2]]},{"w":18,"r":[[0,2],[2,2],[4,2],[6,2],[8,4],[12,2],[14,2]]},{"w":3,"r":[[0,2],[3,1],[5,2],[7,1],[9,1],[10,1],[11,1],[13,1],[14,2]]},{"w":2,"r":[[0,2],[2,2],[4,2],[6,2],[8,2],[10,2],[13,1],[14,2]]},{"w":2,"r":[[0,2],[4,2],[6,2],[9,1],[10,2],[12,2],[14,2]]},{"w":1,"r":[[2,2],[5,2],[7,2],[10,1],[11,1],[12,2],[14,2]]},{"w":1,"r":[[0,2],[3,1],[5,2],[7,1],[9,2],[11,1],[12,2],[14,2]]},{"w":1,"r":[[2,2],[4,2],[6,2],[9,1],[10,2],[12,2],[14,2]]}],"roller":[{"w":19,"r":[[0,1],[1,1],[2,1],[3,1],[4,1],[5,1],[6,1],[7,1],[8,1],[9,1],[10,1],[11,1],[12,1],[13,1],[14,1],[15,1]]},{"w":3,"r":[[0,1],[1,1],[2,1],[4,1],[5,1],[6,1],[8,1],[9,1],[10,1],[12,1],[13,1],[14,1]]},{"w":1,"r":[[0,1],[1,1],[2,1],[3,1],[4,1],[5,1],[6,1],[7,1],[8,1],[9,1],[10,1],[11,1],[12,4]]}],"stabs":[{"w":8,"r":[[0,3],[3,3],[6,2],[8,3],[11,3],[14,2]]}],"sparse":[{"w":3,"r":[[0,2],[6,2],[10,2],[12,2],[14,2]]},{"w":2,"r":[[0,6],[6,2],[8,4],[14,2]]},{"w":2,"r":[[0,2],[6,2],[10,2],[13,2]]},{"w":1,"r":[[2,2],[6,2],[10,2],[13,2]]}]},"endings":[[[0,4],[4,4],[8,4],[12,4]],[[0,2],[2,2],[4,2],[6,2],[8,4]],[[0,6],[6,2],[8,8]],[[0,4],[4,4],[8,6]],[[0,2],[2,2],[4,4],[8,4],[12,4]],[[0,1],[1,1],[2,1],[3,1],[4,1],[5,1],[6,1],[7,1],[8,1],[9,1],[10,1],[11,1],[12,4]],[[0,2],[2,2],[4,2],[6,2],[8,6]]],"progressions":{"minor":[[[0,"m"],[8,"M"],[3,"M"],[10,"M"],[0,"m"],[8,"M"],[10,"M"],[0,"m"]],[[0,"m"],[8,"M"],[3,"M"],[10,"M"],[0,"m"],[8,"M"],[3,"M"],[10,"M"]],[[0,"m"],[8,"M"],[5,"m"],[7,"M"],[0,"m"],[8,"M"],[5,"m"],[7,"M"]],[[8,"M"],[10,"M"],[0,"m"],[0,"m"],[8,"M"],[10,"M"],[0,"m"],[7,"M"]],[[0,"m"],[10,"M"],[7,"m"],[8,"M"],[0,"m"],[10,"M"],[7,"m"],[8,"M"]],[[0,"m"],[8,"M"],[3,"M"],[10,"M"]],[[8,"M"],[10,"M"],[0,"m"],[0,"m"]],[[0,"m"],[10,"M"],[8,"M"],[7,"m"]],[[0,"m"],[8,"M"],[5,"m"],[7,"m"]],[[0,"m"],[7,"m"],[8,"M"],[10,"M"]],[[0,"m"],[8,"M"],[10,"M"],[7,"m"]],[[0,"m"],[10,"M"],[8,"M"],[7,"M"]]],"major":[[[0,"M"],[7,"M"],[9,"m"],[5,"M"],[0,"M"],[7,"M"],[5,"M"],[7,"M"]]]}}/*END*/;
  const MINOR = [0, 2, 3, 5, 7, 8, 10], MAJOR = [0, 2, 4, 5, 7, 9, 11];
  const LO = 72, HI = 98, WR = 12;                    // written register (the piece is played WR semitones lower, like the 22 melodies)
  const NAMES_MIN = ['C', 'C#', 'D', 'Eb', 'E', 'F', 'F#', 'G', 'G#', 'A', 'Bb', 'B'];
  const NAMES_MAJ = ['C', 'Db', 'D', 'Eb', 'E', 'F', 'F#', 'G', 'Ab', 'A', 'Bb', 'B'];
  const STYLE_NAMES = ['Any', 'Anthem', 'Run', 'Roller', 'Stabs', 'Sparse'];
  const FAM = ['anthem', 'run', 'roller', 'stabs', 'sparse'];
  const GATE = { anthem: 92, run: 92, roller: 80, stabs: 60, sparse: 92 };   // percent of the note length
  const SHAPES = [[0, 0, 2, 0, 5, 7, 9, 2], [0, 2, 0, 3, 6, 8, 6, 2], [0, 0, 0, 0, 6, 6, 8, 0], [0, 1, 2, 3, 6, 8, 9, 3]];

  function Rng(seed) {
    let x = (seed >>> 0) || 0x9E3779B9;
    const next = () => { x = (x ^ (x << 13)) >>> 0; x = (x ^ (x >>> 17)) >>> 0; x = (x ^ (x << 5)) >>> 0; return x; };
    for (let i = 0; i < 4; i++) next();
    const r = { next, n: k => (next() >>> 8) % k, pick: w => { let t = 0; for (const v of w) t += v; let q = r.n(t); for (let i = 0; i < w.length; i++) { if (q < w[i]) return i; q -= w[i]; } return w.length - 1; } };
    return r;
  }
  const mod12 = v => ((v % 12) + 12) % 12;
  const clamp = (v, a, b) => v < a ? a : v > b ? b : v;

  function chordInfo(key, off, q) { const r = mod12(key + off); return { root: r, q, tones: [r, mod12(r + (q === 'm' ? 3 : 4)), mod12(r + 7)] }; }

  /* one try of the generator; returns the piece or null */
  function attempt(seed, key, minor, style, tag) {
    const R = Rng(seed);
    const mode = minor ? 'minor' : 'major', scale = (minor ? MINOR : MAJOR).map(v => mod12(key + v));
    /* chords: 8 bars */
    const pool = DATA.progressions[mode]; const base = pool[R.n(pool.length)];
    let prog = base.slice();
    if (prog.length === 4) { const second = prog.slice(); if (R.n(100) < 60) { const turn = minor ? [[10, 'M'], [7, 'M'], [8, 'M'], [7, 'm']] : [[7, 'M'], [5, 'M'], [9, 'm']]; second[3] = turn[R.n(turn.length)]; } prog = prog.concat(second); }
    const chords = prog.map(c => chordInfo(key, c[0], c[1]));
    /* rhythm */
    let fam;
    if (style >= 1) fam = FAM[style - 1]; else { const w = FAM.map(f => DATA.rhythms[f].reduce((a, x) => a + x.w, 0)); fam = FAM[R.pick(w)]; }
    const bank = DATA.rhythms[fam];
    const A = bank[R.pick(bank.map(x => x.w))].r; let B = A; for (let i = 0; i < 6 && B === A && bank.length > 1; i++) B = bank[R.pick(bank.map(x => x.w))].r;
    const endAny = R.n(100) < (fam === 'anthem' ? 80 : fam === 'run' ? 60 : 0); const E = endAny ? DATA.endings[R.n(DATA.endings.length)] : null;
    const tpl = [];
    for (let b = 0; b < 8; b++) { let t = (b % 4 === 3) ? B : A; if (b >= 4 && b <= 6 && B !== A && R.n(100) < 35) t = (t === A) ? B : A; if (b === 7 && E) t = E; tpl.push(t); }
    /* register and shape */
    const shape = SHAPES[R.n(SHAPES.length)], center0 = 82 + R.n(5) - 2;
    const barCenter = b => center0 + shape[b] + (R.n(3) - 1);
    const pedalType = fam === 'roller' ? R.n(2) : 0;   // 0 fixed key note or fifth, 1 root of the bar
    const pedalPc = R.n(2) ? key : mod12(key + 7);
    /* notes */
    const N = [];
    for (let b = 0; b < 8; b++) tpl[b].forEach((p, k) => N.push({ b, k, s: p[0], n: p[1], abs: b * 16 + p[0], p: -1, anchor: false }));
    const last = N[N.length - 1]; 
    const ladder = [], isCT = [];
    for (let b = 0; b < 8; b++) {
      const ct = chords[b].tones, L = [], C = [];
      for (let m = LO; m <= HI; m++) { const pc = mod12(m); const c = ct.indexOf(pc) >= 0; if (c || scale.indexOf(pc) >= 0) { L.push(m); C.push(c); } }
      ladder.push(L); isCT.push(C);
    }
    const nearest = (L, v) => { let bi = 0, bd = 1e9; for (let i = 0; i < L.length; i++) { const d = Math.abs(L[i] - v); if (d < bd) { bd = d; bi = i; } } return bi; };
    /* pass 1: anchors (beats 1 and 3, and the last note) are chord tones */
    let prev = -1; const d8 = []; for (let b = 0; b < 8; b++) d8.push(R.n(7) - 3);
    const centers = []; for (let b = 0; b < 8; b++) centers.push(barCenter(b));
    for (const o of N) {
      if (!(o.s === 0 || o.s === 8 || o === last)) continue;
      o.anchor = true; const L = ladder[o.b], C = isCT[o.b];
      const tgt = centers[o.b] + (o.s >= 8 ? d8[o.b] : 0) + (o === last ? -2 : 0);
      const cand = []; for (let i = 0; i < L.length; i++) if (C[i]) cand.push(L[i]);
      cand.sort((x, y) => Math.abs(x - tgt) - Math.abs(y - tgt) || x - y);
      let list = cand; if (prev >= 0) { const ok = cand.filter(x => Math.abs(x - prev) <= 9), ok2 = cand.filter(x => Math.abs(x - prev) <= 12); list = ok.length ? ok : ok2.length ? ok2 : cand; }
      o.p = list[Math.min(R.pick([6, 3, 1]), list.length - 1)]; prev = o.p;
    }
    /* pass 2: the other notes */
    const pRep = { anthem: 6, run: 8, roller: 0, stabs: 4, sparse: 4 }[fam];
    const copyFrom = new Array(8).fill(-1); const rel = new Array(8).fill(null);
    for (let b = 1; b < 8; b++) {
      const cand = []; if (tpl[b - 1] === tpl[b]) cand.push([b - 1, 60]); if (b >= 4 && tpl[b - 4] === tpl[b]) cand.push([b - 4, 25]);
      for (const [s, pc] of cand) if (copyFrom[b] < 0 && R.n(100) < pc) copyFrom[b] = s;
    }
    let lastP = -1;
    const anchorsAfter = i => { for (let j = i + 1; j < N.length; j++) if (N[j].anchor) return N[j]; return null; };
    const anchorBefore = i => { for (let j = i - 1; j >= 0; j--) if (N[j].anchor) return N[j]; return null; };
    let idx = 0;
    for (let b = 0; b < 8; b++) {
      const L = ladder[b]; const bar = []; rel[b] = [];
      let barAnchorIdx = -1;
      while (idx < N.length && N[idx].b === b) {
        const o = N[idx], i = idx; idx++;
        if (o.anchor) { barAnchorIdx = nearest(L, o.p); o.p = L[barAnchorIdx]; rel[b].push(null); lastP = o.p; continue; }
        let p = -1;
        if (copyFrom[b] >= 0 && barAnchorIdx >= 0 && rel[copyFrom[b]][o.k] != null) { const j = barAnchorIdx + rel[copyFrom[b]][o.k]; if (j >= 0 && j < L.length) p = L[j]; }
        if (p < 0) {
          const aB = anchorBefore(i), aA = anchorsAfter(i);
          const pa = aB ? aB.p : aA.p, pb = aA ? aA.p : pa, sa = aB ? aB.abs : aA.abs, sb = aA ? aA.abs : sa + 1;
          if (fam === 'roller' && (o.s % 2 === 1)) { p = (pedalType ? mod12(chords[b].root) : pedalPc); let best = -1, bd = 1e9; const c0 = centers[b] + 2; for (let m = LO; m <= HI; m++) if (mod12(m) === p && Math.abs(m - c0) < bd) { bd = Math.abs(m - c0); best = m; } p = best; }
          else {
            const t100 = sb > sa ? Math.round(100 * (o.abs - sa) / (sb - sa)) : 0, tgt = Math.round(pa + (pb - pa) * t100 / 100);
            let j = nearest(L, tgt);
            if (lastP >= 0 && R.n(100) < pRep && L.indexOf(lastP) >= 0) j = L.indexOf(lastP);
            else {
              j += [-2, -1, 0, 1, 2][R.pick(fam === 'roller' ? [2, 3, 2, 3, 2] : [2, 3, 2, 3, 2])];
              if (R.n(100) < 45) { let bj = j, bd = 99; for (let q = Math.max(0, j - 3); q <= Math.min(L.length - 1, j + 3); q++) if (isCT[b][q] && Math.abs(q - j) < bd) { bd = Math.abs(q - j); bj = q; } j = bj; }
              if (L[clamp(j, 0, L.length - 1)] === lastP) { const dir = (aA && aA.p !== lastP) ? (aA.p > lastP ? 1 : -1) : (R.n(2) ? 1 : -1); j += dir * (1 + R.n(2)); }
            }
            const nx = (idx < N.length) ? N[idx] : null;
            if (nx && nx.anchor && nx.b === b && R.n(100) < 60) { const aj = nearest(L, nx.p); j = aj + (lastP >= nx.p ? 1 : -1); }
            j = clamp(j, 0, L.length - 1); p = L[j];
          }
          const lim = fam === 'roller' ? 12 : 9; if (lastP >= 0 && Math.abs(p - lastP) > lim) p = L[nearest(L, lastP + (p > lastP ? lim : -lim))];
        }
        o.p = p; lastP = p; rel[b].push(barAnchorIdx >= 0 ? nearest(L, p) - barAnchorIdx : null);
      }
    }
    /* events */
    const ev = [], gate = GATE[fam];
    for (const o of N) {
      const accent = o.s === 0 || o.s === 8; let vel = accent ? 108 : (o.n >= 6 ? 100 : 90); if (o.b >= 4 && o.b <= 6) vel = Math.min(120, vel + 8);
      const g = fam === 'stabs' ? gate : (o.n === 1 ? 80 : gate);
      ev.push([o.b * 4 + o.s / 4, Math.round(o.n * g * 2.5) / 1000, o.p - WR, vel]);
    }
    return { fam, chords, minor, key, N, ev, tpl, ladder };
  }

  function validate(r) {
    const prob = [], scale = (r.minor ? MINOR : MAJOR).map(v => mod12(r.key + v)); let top = -1, topBar = -1, mn = 999, prev = -1, maxLeap = 0;
    r.N.forEach(o => {
      const ct = r.chords[o.b].tones, pc = mod12(o.p);
      if (scale.indexOf(pc) < 0 && ct.indexOf(pc) < 0) prob.push('note outside key, bar ' + (o.b + 1));
      if ((o.s === 0 || o.s === 8) && ct.indexOf(pc) < 0) prob.push('beat note not a chord tone, bar ' + (o.b + 1));
      if (o.p > top) { top = o.p; topBar = o.b + 1; } mn = Math.min(mn, o.p); if (prev >= 0) maxLeap = Math.max(maxLeap, Math.abs(o.p - prev)); prev = o.p;
    });
    if (topBar < 5 || topBar > 7) prob.push('highest note in bar ' + topBar);
    if (top - mn > 24) prob.push('range ' + (top - mn)); if (maxLeap > 12) prob.push('leap ' + maxLeap);
    if (r.N.length < 14) prob.push('too few notes');
    return prob;
  }

  function chordName(c, minorKey) { const names = minorKey ? NAMES_MIN : NAMES_MAJ; return names[c.root] + (c.q === 'm' ? 'm' : ''); }

  function generate(o) {
    o = o || {}; const seed = ((o.seed | 0) >>> 0) % 16777216 || 1, key = mod12(o.key | 0), style = clamp(o.style | 0, 0, 5);
    let r = null, tries = 0;
    for (; tries < 64; tries++) {
      const R0 = Rng((seed * 2654435761 + tries * 40503) >>> 0);
      const minor = (o.mode | 0) === 0 ? true : (o.mode | 0) === 1 ? false : R0.n(100) < 85;
      const sd = (seed ^ Math.imul(tries, 0x9E3779B1)) >>> 0;
      const t = attempt(sd, key, minor, style, tries);
      if (t && validate(t).length === 0) { r = t; break; }
    }
    if (!r) throw new Error('melody generator: no valid piece for seed ' + seed);
    const bpm = [136, 138, 140][Rng(seed + 7).n(3)];
    const cn = r.chords.map(c => chordName(c, r.minor));
    return { name: 'Generated #' + seed + ', ' + cn.join('-'), bpm, beats: 32, ev: r.ev, info: { seed, key, minor: r.minor, style: FAM.indexOf(r.fam) + 1, tries, chords: cn, notes: r.N.length } };
  }
  const api = { generate, validate, STYLE_NAMES, NAMES_MIN, NAMES_MAJ, _attempt: attempt, DATA };
  if (typeof module !== 'undefined' && module.exports) module.exports = api; else root.JPGen = api;
})(typeof window !== 'undefined' ? window : globalThis);
