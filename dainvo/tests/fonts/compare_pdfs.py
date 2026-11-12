from pathlib import Path
import pymupdf as fitz,json
import argparse
p=argparse.ArgumentParser();p.add_argument('--workdir',type=Path,required=True);root=p.parse_args().workdir;results=[]
def extract(path):
 d=fitz.open(path);pages=[];fonts=set()
 for p in d:
  lines=[]
  for b in p.get_text('rawdict')['blocks']:
   for l in b.get('lines',[]):
    chars=[]
    for s in l['spans']:
     fonts.add(s['font'])
     chars.extend((c['c'],c['origin']) for c in s['chars'])
    lines.append(chars)
  pages.append(lines)
 return pages,fonts
for suffix in ['', 'Display','Narrow','Serif']:
 a,af=extract(root/'pdfs'/('Aptos'+suffix+'.pdf'));i,inf=extract(root/'pdfs'/('Intos'+suffix+'.pdf'))
 at=[[ ''.join(c for c,o in l) for l in p] for p in a]; it=[[ ''.join(c for c,o in l) for l in p] for p in i]
 equal=at==it
 deltas=[abs(x-y) for ap,ip in zip(a,i) for al,il in zip(ap,ip) for (ac,ao),(ic,io) in zip(al,il) for x,y in zip(ao,io)] if equal else []
 r={'family':'Intos'+suffix,'aptos_pages':len(a),'intos_pages':len(i),'identical_line_breaks':equal,'max_character_origin_delta_pt':max(deltas) if deltas else None,'reference_fonts':sorted(af),'candidate_fonts':sorted(inf)}
 results.append(r);print(json.dumps(r))
(root/'layout-results.json').write_text(json.dumps(results,indent=2)+'\n')
