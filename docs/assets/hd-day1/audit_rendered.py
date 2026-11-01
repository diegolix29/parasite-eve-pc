from pathlib import Path
from PIL import Image
import numpy as np,json,argparse
p=argparse.ArgumentParser();p.add_argument("--candidate",default="hd-current-tail");opts=p.parse_args()
R=Path(__file__).resolve().parent;native=R/'verify/native-current-tail';hd=R/'verify'/opts.candidate;states=json.loads((hd/'states.json').read_text());records=[]
for record in states:
 state=record['state'];tick=state['vblanks'];a=np.array(Image.open(native/f'{tick}.ppm'));b=np.array(Image.open(hd/f'{tick}.ppm'));assert a.shape==b.shape;changed=int(np.count_nonzero(a!=b));records.append({'tick':tick,'scene':state['scene'],'stage':state['story_stage'],'changed_rendered_channels':changed,'shape':list(a.shape)})
assert all(next(r for r in records if r['tick']==tick)['changed_rendered_channels']>1000 for tick in [24000,28000]);(hd/'rendered-differences.json').write_text(json.dumps({'checkpoints':records,'changed_checkpoints':sum(r['changed_rendered_channels']>0 for r in records),'native_state_vram_pcm_assertions':'parity.json','note':'Rendered differences are expected and required; no image equality or perfect-art certification is claimed.'},indent=2));print(sum(r['changed_rendered_channels']>0 for r in records),'of',len(records),'rendered checkpoints changed; early real field HD active')
