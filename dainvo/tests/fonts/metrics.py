import json,pathlib,statistics
from fontTools.ttLib import TTFont
import uharfbuzz as hb
import argparse
p=argparse.ArgumentParser();p.add_argument('--workdir',type=pathlib.Path,required=True);root=p.parse_args().workdir
samples=[
'The quick brown fox jumps over the lazy dog. Pack my box with five dozen liquor jugs.',
'AVATAR To WA Yo fi fl ffi ffl office efficient affinity difficult flowers',
'Invoice 2026-11-12: $1,234.56 / €789.00 / £42.99 — 0123456789 (25%)',
'Résumé naïve façade café Ångström Straße über déjà vu São Paulo Łódź',
'Ελληνικά: Αθήνα κόσμος άλφα βήτα Γειά σου',
'Кириллица: Съешь ещё этих мягких французских булок, да выпей чаю.',
'“Quoted text”, apostrophe’s, hyphen-ated, en–dash, em—dash; [brackets] {braces}.',
'ABCDEFGHIJKLMNOPQRSTUVWXYZ abcdefghijklmnopqrstuvwxyz 0123456789',
'A\u0301 e\u0308 o\u0302 n\u0303 fi fl ffi ffl a/b + c=d → ← ∑ √ ∞',
]
results=[]
for ip in sorted((root/'upstream/intos').glob('*.ttf')):
 ap=root/'reference-only-aptos'/ip.name.replace('IntosDisplay','Aptos-Display').replace('IntosNarrow','Aptos-Narrow').replace('IntosSerif','Aptos-Serif').replace('Intos','Aptos').replace('-Regular','').replace('-Semibold','-SemiBold').replace('BoldItalic','Bold-Italic').replace('SemiBoldItalic','SemiBold-Italic')
 a,i=TTFont(ap),TTFont(ip)
 ac,ic=a.getBestCmap(),i.getBestCmap(); common=set(ac)&set(ic)
 au,iu=a['head'].unitsPerEm,i['head'].unitsPerEm
 diffs={c:(i['hmtx'][ic[c]][0]/iu-a['hmtx'][ac[c]][0]/au) for c in common}
 missing=sorted(set(ac)-set(ic))
 def shape(p,s):
  f=hb.Font(hb.Face(p.read_bytes())); b=hb.Buffer(); b.add_str(s); b.guess_segment_properties(); hb.shape(f,b)
  return sum(g.x_advance for g in b.glyph_positions)/f.face.upem
 shaped=[]
 for s in samples:
  if all(ord(c) in common for c in s):
   aw,iw=shape(ap,s),shape(ip,s); shaped.append({'text':s,'aptos_em':aw,'intos_em':iw,'delta_em':iw-aw})
 def metrics(f):
  u=f['head'].unitsPerEm
  return {k:getattr(f[table],name)/u for k,table,name in [('ascender','hhea','ascent'),('descender','hhea','descent'),('line_gap','hhea','lineGap'),('typo_ascender','OS/2','sTypoAscender'),('typo_descender','OS/2','sTypoDescender'),('typo_line_gap','OS/2','sTypoLineGap')]}
 r={'file':ip.name,'reference':ap.name,'family':i['name'].getDebugName(1),'subfamily':i['name'].getDebugName(2),'common_characters':len(common),'different_advances':sum(abs(d)>1e-8 for d in diffs.values()),'max_advance_delta_em':max(map(abs,diffs.values())),'missing_reference_characters':len(missing),'missing_ascii':[c for c in missing if 32<=c<127],'metrics_equal':metrics(a)==metrics(i),'aptos_metrics':metrics(a),'intos_metrics':metrics(i),'shaped_samples':shaped}
 results.append(r)
 print(ip.name,r['family'],r['subfamily'],'common',len(common),'advance diffs',r['different_advances'],'max',round(r['max_advance_delta_em'],4),'missing',len(missing),'metrics',r['metrics_equal'],'shape max',round(max(abs(t['delta_em']) for t in shaped),4))
(root/'metrics.json').write_text(json.dumps(results,ensure_ascii=False,indent=2)+'\n')
