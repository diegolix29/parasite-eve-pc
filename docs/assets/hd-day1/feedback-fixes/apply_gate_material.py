"""Separate the gate's metal material from scene water; native masks own geometry."""
from pathlib import Path
from PIL import Image
from scipy.ndimage import distance_transform_edt
import numpy as np,json
R=Path(__file__).resolve().parents[1];D=R/'feedback-fixes';ts=json.loads((R/'tasks.json').read_text())['tasks'];first=next(t for t in ts if t['room']=='M032' and t['entry']==0);base=Image.open(R/'OpenAI'/first['input']).convert('RGB');im=np.array(Image.open(D/'M032-metal-generated.png').convert('RGBA'));valid=im[:,:,3]>128;assert valid.any();indices=distance_transform_edt(~valid,return_distances=False,return_indices=True);padded=im[:,:,:3][indices[0],indices[1]];patch=Image.fromarray(padded).resize((372,452),Image.Resampling.LANCZOS);base.paste(patch,(140,68));out=D/'entry-overrides'
# The original sprites shorten the visible bars. Their original fixed-world
# shading does not translate; only solid metal RGB is shared, never a scene
# crop containing water. Original per-phase alpha and tile placement are intact.
for t in ts:
 if t['room']=='M032' and t['control']==0:
  p=out/(t['key']+'.png');temp=p.with_suffix('.tmp');base.save(temp,format='PNG');temp.replace(p)
(D/'gate-material-application.json').write_text(json.dumps({'source':'M032-metal-generated.png','background_not_sampled':True,'original_native_alpha_and_positions_unchanged':True,'shared_metal_material_all_opening_phases':True,'transparent_RGB_padding':'Nearest foreground metal colour; generated alpha is not used as runtime geometry.'},indent=2))
