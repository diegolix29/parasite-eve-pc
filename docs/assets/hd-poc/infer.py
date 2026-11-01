"""Offline full-context inference. No per-tile inference or game dependencies."""
from pathlib import Path
import argparse,hashlib,json,time
import torch,numpy as np
from PIL import Image
from spandrel import ModelLoader
p=argparse.ArgumentParser();p.add_argument('weight');p.add_argument('input');p.add_argument('output');p.add_argument('--device',default='mps');p.add_argument('--limit',type=int,default=0);p.add_argument('--tile',type=int,default=0);p.add_argument('--half',action='store_true');a=p.parse_args()
root=Path(a.output);root.mkdir(parents=True,exist_ok=True);torch.set_num_threads(4)
model=ModelLoader().load_from_file(a.weight).to(a.device).eval();assert model.scale==4
if a.half:model=model.half()
files=sorted(Path(a.input).glob('*.png'));files=files[:a.limit] if a.limit else files
start=time.monotonic();peak=0;records=[]
for n,path in enumerate(files):
 dest=root/path.name
 if dest.exists():continue
 im=np.array(Image.open(path).convert('RGB'));t=torch.from_numpy(im.astype('float32')/255).permute(2,0,1).unsqueeze(0).to(a.device)
 if a.half:t=t.half()
 with torch.inference_mode():
  if a.tile:
   # Large-context overlapping inference; only retain each central tile.
   _,_,h,w=t.shape;out=torch.empty((1,3,h*4,w*4),device='cpu');pad=32
   for y in range(0,h,a.tile):
    for x in range(0,w,a.tile):
     x0,y0=max(0,x-pad),max(0,y-pad);x1,y1=min(w,x+a.tile+pad),min(h,y+a.tile+pad)
     patch=model(t[:,:,y0:y1,x0:x1]).float().cpu()
     ex,ey=min(w,x+a.tile),min(h,y+a.tile)
     out[:,:,y*4:ey*4,x*4:ex*4]=patch[:,:,(y-y0)*4:(ey-y0)*4,(x-x0)*4:(ex-x0)*4]
  else:out=model(t).float().cpu()
  out=out.squeeze(0).clamp(0,1).permute(1,2,0).numpy()
 Image.fromarray(np.rint(out*255).astype('uint8')).save(dest)
 records.append({'input':path.name,'input_sha256':hashlib.sha256(path.read_bytes()).hexdigest(),'output_sha256':hashlib.sha256(dest.read_bytes()).hexdigest()})
 print(n+1,len(files),path.name,round(time.monotonic()-start,1),flush=True)
 if a.device=='cuda':peak=max(peak,torch.cuda.max_memory_allocated())
(root/'inference.json').write_text(json.dumps({'model':Path(a.weight).name,'weights_sha256':hashlib.sha256(Path(a.weight).read_bytes()).hexdigest(),'device':a.device,'torch':torch.__version__,'tile':a.tile,'half':a.half,'elapsed':time.monotonic()-start,'peak_allocated_bytes':peak,'files':records},indent=2))
