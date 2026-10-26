#!/usr/bin/env python3
"""Writes states.json: a spread of parameter states that exercise every display (static, no live data)."""
import json, random
S = []
def st(name, params=None, **kw):
    d = {"name": name, "params": params or {}, "scr": 0.37, "t": 1.0}; d.update(kw); S.append(d)
st("default")
for w in range(7):
    st(f"o1w{w}", {"o1w": w, "o1c1": 30 + w * 10, "o1c2": 90 - w * 8}, scr=0.61)
    st(f"o1w{w}_param", {"o1w": w, "o1c1": 30 + w * 10, "o1c2": 90 - w * 8}, touch=["o1c1"])
for w in range(4):
    st(f"o2w{w}", {"o2w": w, "o2c1": 20 + w * 30, "o2c2": 10 + w * 25}, scr=1.9)
    st(f"o2w{w}_param", {"o2w": w, "o2c1": 20 + w * 30, "o2c2": 10 + w * 25}, touch=["o2c2"])
for w in range(4): st(f"lfo{w}", {"l1w": w})
for ft in range(3):
    for f24 in (0, 1):
        st(f"filt{ft}_{f24}", {"ft": ft, "f24": f24, "fc": 40 + ft * 30, "fr": 20 + ft * 45})
st("filt_extreme", {"fc": 0, "fr": 127, "fk": 63}); st("filt_open", {"fc": 127, "fr": 0})
st("env_min", {k: 0 for k in ["fea", "fed", "fes", "fer", "aea", "aed", "aes", "aer", "pa", "pdd", "pd"]})
st("env_max", {k: 127 for k in ["fea", "fed", "fes", "fer", "aea", "aed", "aes", "aer", "pa", "pdd", "pd"]})
st("env_mid", {"fea": 64, "fed": 30, "fes": 90, "fer": 50, "aea": 10, "aed": 80, "aes": 40, "aer": 100, "pa": 40, "pdd": 70, "pd": 64})
st("pe_neg", {"pd": -50, "pa": 20})
st("pad", {"bend": -40, "mod": 80}); st("pad2", {"bend": 60, "mod": 5})
st("rib_none", {"rib": 90, "rrel": 0}); st("rib_target", {"rib": 90, "rrel": 1}, ribbonTarget="fc"); st("rib_hold", {"rib": 30, "rhold": 1}, ribbonTarget="o1c1")
st("gate_off", {"gon": 0, "gpat": 0x8421}); st("gate_decay", {"gon": 1, "gpat": 0xAAAA, "gdc": 90, "gdep": 60}); st("gate_swell", {"gon": 1, "gpat": 0x5555, "gsw": 80, "gdep": 40})
st("gate_deep", {"gon": 1, "gpat": 0xFFFF, "gdep": 127}); st("gate_none", {"gon": 1, "gpat": 0})
st("fx_off", {"qon": 0})
st("fx_ott_on", {"qvon": 1, "qvdl": 10, "qvul": 120, "qvdm": 127, "qvum": 0, "qvdh": 64, "qvuh": 64})
st("fx_mb_solo", {"qm2s": 1, "qm3o": 0, "qm1u": 1, "qmx1": 0, "qmx2": 127, "qmx3": 127})
st("fx_mb_off", {"qmon": 0}); st("fx_mb_mid", {"qmx1": 90, "qmx2": 20, "qmx3": 60, "qm1t": 20, "qm1r": 120, "qm4t": 100, "qm4r": 5})
st("fx_drive_hi", {"qdv": 120, "qdx": 100, "qdf": 120, "qdm": 127}); st("fx_drive_lo", {"qdv": 0, "qdx": 0, "qdf": 0}); st("fx_drive_off", {"qdon": 0})
st("fx_low", {"qhp": 0, "qlm": 127}); st("fx_low2", {"qhp": 127, "qlm": 0}); st("fx_low_off", {"qlon": 0})
st("fx_eq_hi", {"qmp": 24, "qsh": 127, "qai": 24}); st("fx_eq_lo", {"qmp": -24, "qsh": 0, "qai": -24}); st("fx_eq_off", {"qeon": 0})
st("fx_trans", {"qta": 12, "qts": -12}); st("fx_trans2", {"qta": -12, "qts": 12}); st("fx_trans_off", {"qtn": 0})
st("fx_tamer", {"qrd": 127, "qrf": 0}); st("fx_tamer1", {"qrd": 100, "qrf": 1}); st("fx_tamer_off", {"qrn": 0})
for r in range(4): st(f"fx_glue{r}", {"qct": 20 + r * 30, "qcr": r})
st("fx_glue_off", {"qcn": 0})
st("fx_wide", {"qwa": 127}); st("fx_wide0", {"qwa": 0}); st("fx_wide_off", {"qwn": 0})
st("fx_pan", {"qpn": 1, "qpd": 100, "qpr": 90}); st("fx_pan2", {"qpn": 1, "qpd": 30, "qpr": 10})
st("fx_out", {"qka": 127, "qll": 127, "qlc": 30}); st("fx_out0", {"qka": 0, "qll": 0, "qlc": 0, "qko": 0})
st("fx_opto", {"qoon": 1, "qopr": 90, "qomd": 1, "qomx": 127}); st("fx_opto_off", {"qoon": 0})
random.seed(7)
d = json.load(open('../../data/params.json'))['params']
ids = [p for p in d if not p.get('nosave') and (p['id'][0] == 'q' or p['id'][0] in 'fao' or p['id'] in ('pa', 'pdd', 'pd', 'l1w', 'gpat', 'gdep', 'gdc', 'gsw'))]
for i in range(8):
    pr = {p['id']: random.randint(p['min'], p['max']) for p in ids if random.random() < 0.5}
    pr["qon"] = 1; pr["qvon"] = 1
    st(f"rand{i}", pr, scr=random.random() * 3)
json.dump(S, open('states.json', 'w'), indent=0)
print(len(S), 'states')
