from pathlib import Path
from PIL import Image
import json
R=Path(__file__).resolve().parents[1];D=R/'feedback-fixes';ts=json.loads((R/'tasks.json').read_text())['tasks'];mask=Image.open(D/'M032-opening-mask.png');underlay=Image.open(D/'M032-underlay-full.png').convert('RGB');out=D/'entry-overrides'
# Water's first context is shared with the gate's closed context. Per-entry
# atlases allow a bare passage behind the moving slats without painting a
# second gate into that water/background layer.
for t in ts:
 if t['room']!='M032' or not (t['control']==1 or t['entry']==11):continue
 im=Image.open(R/'OpenAI-feedback'/t['input']).convert('RGB');p=out/(t['key']+'.png');temp=p.with_suffix('.tmp');Image.composite(underlay,im,mask).save(temp,format='PNG');temp.replace(p)
