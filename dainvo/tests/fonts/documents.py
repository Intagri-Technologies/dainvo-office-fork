from pathlib import Path
import shutil
from docx import Document
from docx.shared import Inches,Pt
from docx.oxml import OxmlElement
from docx.oxml.ns import qn
import argparse
p=argparse.ArgumentParser();p.add_argument('--workdir',type=Path,required=True);root=p.parse_args().workdir
fonts=root/'Reference.app/Contents/Resources/fonts/truetype'
for p in (root/'reference-only-aptos').glob('*.ttf'): shutil.copy2(p,fonts/p.name)
for p in (root/'upstream').rglob('*.ttf'): shutil.copy2(p,fonts/p.name)
inputs=root/'documents';inputs.mkdir(exist_ok=True)
text=('Dainvo Office compatibility report — The quick brown fox jumps over the lazy dog. '
      'Our office reviewed the financial figures, efficient workflows and difficult cases. '
      'Invoice 2026-11-12: $1,234.56, €789.00 and £42.99 (25%). '
      'Résumé, naïve, façade, café, Ångström, Straße, São Paulo and Łódź. '
      'AVATAR To WA Yo: punctuation, “quoted text”, apostrophe’s and 0123456789. ')
def font(run,family,bold=False,italic=False,size=11):
 run.font.name=family;run.bold=bold;run.italic=italic;run.font.size=Pt(size)
 props=run._element.get_or_add_rPr()
 rf=props.rFonts
 for k in ['ascii','hAnsi','eastAsia','cs']: rf.set(qn('w:'+k),family)
 # Enable kerning just as a normal office paragraph would for matching typography.
 kern=OxmlElement('w:kern');kern.set(qn('w:val'),'0');props.append(kern)
for suffix in ['', ' Display',' Narrow',' Serif']:
 for kind in ['Aptos','Intos']:
  family=kind+suffix;doc=Document();sec=doc.sections[0]
  sec.page_width=Inches(8.5);sec.page_height=Inches(11)
  sec.top_margin=sec.bottom_margin=sec.left_margin=sec.right_margin=Inches(.75)
  for n in range(32):
   p=doc.add_paragraph();p.paragraph_format.space_after=Pt(6)
   for b,i in [(False,False),(True,False),(False,True),(True,True)]:
    font(p.add_run(text),family,b,i)
  table=doc.add_table(rows=5,cols=3)
  for n,row in enumerate(table.rows):
   for j,c in enumerate(row.cells):font(c.paragraphs[0].add_run(f'Row {n+1}: total ${1234.56*(j+1):,.2f}'),family,n==0)
  doc.save(inputs/(family.replace(' ','')+'.docx'))
print('Created eight multi-page DOCX test documents with four font styles each.')
