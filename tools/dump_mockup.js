const {chromium}=require('playwright');const fs=require('fs');
(async()=>{const b=await chromium.launch();const p=await b.newPage({viewport:{width:2920,height:1300}});
await p.goto('file://'+require('path').resolve(process.argv[3]||'docs/mockups/JPurrr-8000-mockup-v3.3.html'));await p.waitForTimeout(900);
const d=await p.evaluate(()=>{const J=window.__JP;const params=J.ORDER.map(id=>{const q=J.P[id];return {id,t:q.t,label:q.label,min:q.min,max:q.max,def:q.def,opts:q.opts||null,nosave:!!q.nosave,mom:!!q.mom,tip:q.tip}});
const presets=J.FACTORY.map(f=>{const v=Object.assign({},f.type?J.qvals(f.type):{},f.v);return {name:f.name,type:f.type,genres:f.genres,slot:f.slot,start:f.start,v}});
return {params,presets,types:J.TYPES,genres:J.GENRES,banks:J.BANKS}});
fs.writeFileSync(process.argv[2],JSON.stringify(d,null,1));console.log(d.params.length,'params',d.presets.length,'presets');await b.close();})();
