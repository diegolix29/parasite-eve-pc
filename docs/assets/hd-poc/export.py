"""Export original contextual layers, immutable keys and native texel coverage.
Private disc-derived study assets. Original runtime geometry is never rewritten.
"""
from pathlib import Path
import argparse,json,struct,importlib.util,hashlib
import numpy as np
from PIL import Image
ROOT=Path(__file__).resolve().parents[3]
p=argparse.ArgumentParser();p.add_argument('--out',type=Path,default=Path(__file__).resolve().parent);p.add_argument('--rooms',default='M001,M002,M003,M004,M005,M006,M009,M010,M011,M012,M013,M020,M030');a=p.parse_args();OUT=a.out.resolve();OUT.mkdir(exist_ok=True,parents=True)
ws=ROOT/'docs/assets/widescreen'
spec=importlib.util.spec_from_file_location('survey',ws/'survey.py');survey=importlib.util.module_from_spec(spec);spec.loader.exec_module(survey)
u32,u16=survey.u32,survey.u16
scope={'__file__':str(ws/'backgrounds.py')};s=(ws/'backgrounds.py').read_text().split("for room in report['rooms']:")[0].replace('survey.scan()','');exec(compile(s,str(ws/'backgrounds.py'),'exec'),scope);upload=scope['upload']
disc=survey.Disc('/Users/hasik/Projects/parasite-pc/disc/Parasite Eve (Disc 1).bin');img=disc.find('PE.IMG');mask64=(1<<64)-1
for d in ['inputs','refs','active-inputs','coverage']:(OUT/d).mkdir(exist_ok=True)
def fnv(h,b):
 for v in b:h=((h^v)*1099511628211)&mask64
 return h
def tile_rgb(p):return np.stack([(p&31)*255//31,((p>>5)&31)*255//31,((p>>10)&31)*255//31],axis=-1).astype('uint8')
def compose(tiles,visible,w,h):
 dest=np.zeros((h,w),dtype=np.uint16)
 for t in sorted((t for t in tiles if t['entry'] in visible),key=lambda t:(t['depth'],t['ordinal']),reverse=True):
  x,y=t['x'],t['y'];x0,y0=max(0,x),max(0,y);x1,y1=min(w,x+16),min(h,y+16)
  if x1<=x0 or y1<=y0:continue
  pix=t['pixels'][y0-y:y1-y,x0-x:x1-x].copy();d=dest[y0:y1,x0:x1];opaque=pix!=0
  if t['semi']:
   stp=(pix&0x8000)!=0
   for sh in [0,5,10]:
    fg=(pix>>sh)&31;bg=(d>>sh)&31;mode=t['mode']
    if mode==0:c=(fg.astype(int)+bg)//2
    elif mode==1:c=np.minimum(31,fg.astype(int)+bg)
    elif mode==2:c=np.maximum(0,bg.astype(int)-fg)
    else:c=np.minimum(31,bg.astype(int)+fg//4)
    pix=(pix&~np.uint16(31<<sh))|(np.where(stp,c,fg).astype(np.uint16)<<sh)
  d[opaque]=pix[opaque]
 return tile_rgb(dest)
def save_image(array,folder):
 digest=hashlib.sha256(struct.pack('<II',array.shape[1],array.shape[0])+array.tobytes()).hexdigest()[:24] if folder=='active-inputs' else hashlib.sha256(array.tobytes()).hexdigest()[:24]
 name=digest+'.png';path=OUT/folder/name
 if not path.exists():Image.fromarray(array).save(path)
 return name
rooms=json.loads((ws/'catalog.json').read_text())['rooms'];tasks=[];roomdata=[];views=[];selected=set(a.rooms.split(','))
for room in rooms:
 if room['disc']!=1 or room['id'] not in selected:continue
 if not room['views']:roomdata.append({'room':room['id'],'status':'No serialized 2D background views','entries':[],'controls':[]});continue
 start,scratch,tex,sectors=[room[k] for k in ['start_sector','scratch_sectors','texture_sectors','room_sectors']]
 blob=disc.read(img,(start+scratch+tex)*2048,sectors*2048);di=u32(blob,4);rp=u32(blob,di+0x1c)&0x3fffff;off=u32(blob,rp+4)&0xffffff;length=u32(blob,rp)&0xffffff;g=blob[off:off+length];ep=u32(g,20);count=u16(g,6)
 vram=np.zeros((512,1024),dtype='<u2');upload(vram,disc.read(img,start*2048,scratch*2048));upload(vram,disc.read(img,(start+scratch)*2048,tex*2048));vb=vram.view('uint8').reshape(512,2048)
 entries=[];tiles=[];gh=14695981039346656037
 for e in range(count):
  m=ep+e*56;n=u16(g,m+38);pp=m+u32(g,m+40);uv=m+u32(g,m+44);group=g[m+36];sx,sy=struct.unpack_from('<hh',g,m+12)
  for data in [bytes([group]),struct.pack('<H',n),g[m+8:m+12],g[pp:pp+n*4],g[uv:uv+n*8]]:gh=fnv(gh,data)
  entries.append({'index':e,'group':group,'flags':g[m],'tiles':n,'key':None,'scroll_modulus':list(struct.unpack_from('<HH',g,m+4)),'source_origin':[sx,sy]})
  for t in range(n):
   pos=u32(g,pp+t*4);word=u32(g,uv+t*8);u,v=g[uv+t*8+4],g[uv+t*8+3];tx,ty=(word>>10)&0x3c0,(word>>7)&256;cx,cy=(word>>5)&0x3f0,word&511
   indices=vb[(ty+np.arange(v,v+16)%256)%512][:,(tx*2+np.arange(u,u+16)%256)%2048];pixels=vram[cy,(cx+indices.astype(np.uint16))%1024]
   tiles.append({'entry':e,'tile':t,'x':sx+(pos>>22),'y':sy+((pos>>12)&1023),'depth':((u32(g,m)>>8)&4095)+(pos&4095),'ordinal':e*100000+t,'pixels':pixels,'semi':bool(u32(g,uv+t*8+4)&0x10000000),'mode':(word>>22)&3})
 controls=[];initial={e['index'] for e in entries if e['flags']&2}
 for ci in range(u16(g,4)):
  cp=u32(g,16)+ci*16;sp=cp+struct.unpack_from('<i',g,cp+12)[0];slots=[{'entry':g[sp+j*2],'duration':struct.unpack_from('b',g,sp+j*2+1)[0]} for j in range(u32(g,cp)>>8)]
  if slots:initial.add(slots[0]['entry'])
  controls.append({'id':ci,'group':g[cp+4],'flags':g[cp],'position':struct.unpack_from('<i',g,cp+4)[0]>>8,'step':struct.unpack_from('<h',g,cp+8)[0],'elapsed':u16(g,cp+10),'slots':slots})
 groupdata={}
 for view in room['views']:
  group=view['group'];raw=[t for t in tiles if entries[t['entry']]['group']==group]
  if not raw:continue
  ox=min(0,min(t['x'] for t in raw));oy=min(0,min(t['y'] for t in raw));w=max(view['width'],max(t['x']+16 for t in raw))-ox;h=max(view['height'],max(t['y']+16 for t in raw))-oy;w=(w+15)//16*16;h=(h+15)//16*16;assert 0<w<=2048 and 0<h<=2048
  translated=[dict(t,x=t['x']-ox,y=t['y']-oy) for t in raw];coverage=np.zeros((h,w),bool)
  for t in translated:coverage[t['y']:t['y']+16,t['x']:t['x']+16]|=t['pixels']!=0
  ys,xs=np.where(coverage)
  if not len(xs):continue
  bounds=[int(xs.min()),int(ys.min()),int(xs.max()+1),int(ys.max()+1)];x0,y0,x1,y1=bounds
  base=compose(translated,{i for i in initial if entries[i]['group']==group},w,h);canonical=save_image(base[y0:y1,x0:x1],'active-inputs');canvas=save_image(base,'inputs');Image.fromarray(coverage.astype('uint8')*255).save(OUT/'coverage'/f"{room['id']}-g{group:02d}.png")
  info={'room':room['id'],'group':group,'canvas_size':[w,h],'native_origin':[ox,oy],'active_bbox':bounds,'active_size':[x1-x0,y1-y0],'base_input':canonical,'base_canvas_input':canvas,'coverage':'coverage/'+f"{room['id']}-g{group:02d}.png",'controllers':[c for c in controls if c['group']==group],'note':'Union of original nonzero packed texel coverage across all entries; opaque black is retained. Shared bounds for every phase, not per-frame black-pixel crop.'};views.append(info);groupdata[group]=(translated,info)
 for entry in entries:
  e=entry['index'];group=entry['group']
  if group not in groupdata:continue
  translated,view=groupdata[group];own=sorted([t for t in translated if t['entry']==e],key=lambda t:t['tile'])
  if not own:continue
  key=f'{fnv(gh,struct.pack("<I",e)):016x}';entry['key']=key;w,h=view['canvas_size'];visible={i for i in initial if entries[i]['group']==group};families=[c for c in controls if any(sl['entry']==e for sl in c['slots'])];family=families[0] if families else None
  if family:visible.difference_update(sl['entry'] for sl in family['slots'])
  visible.add(e);context=compose(translated,visible,w,h)
  for t in sorted(own,key=lambda t:(t['depth'],t['ordinal']),reverse=True):
   x,y=t['x'],t['y'];opaque=t['pixels']!=0;dest=context[y:y+16,x:x+16];rgb=tile_rgb(t['pixels']);dest[opaque]=rgb[opaque]
  source=save_image(context,'inputs');x0,y0,x1,y1=view['active_bbox'];active=save_image(context[y0:y1,x0:x1],'active-inputs');ref=np.stack([t['pixels'] for t in own]);ref_sha=hashlib.sha256(ref.tobytes()).hexdigest();ref_name=key+'.npy'
  if (OUT/'refs'/ref_name).exists() and not np.array_equal(np.load(OUT/'refs'/ref_name),ref):ref_name=key+'-'+ref_sha[:12]+'.npy'
  np.save(OUT/'refs'/ref_name,ref)
  tasks.append({'room':room['id'],'group':group,'entry':e,'key':key,'input':source,'active_input':active,'active_bbox':view['active_bbox'],'native_origin':view['native_origin'],'size':[w,h],'tiles':[[t['x'],t['y']] for t in own],'tile_meta':[{k:t[k] for k in ['depth','ordinal','semi','mode']} for t in own],'control':family['id'] if family else None,'controls':[c['id'] for c in families],'ref':ref_name,'ref_sha256':ref_sha})
 roomdata.append({'room':room['id'],'geometry_key':f'{gh:016x}','entries':entries,'controls':controls});print(room['id'],count,'entries',flush=True)
(OUT/'tasks.json').write_text(json.dumps({'scope':'Serialized entry coverage; actual playback controlled by original scripts. Native immutable geometry keys retained. RGB context is not necessarily a reachable visible frame.','tasks':tasks,'rooms':roomdata},indent=2));(OUT/'views.json').write_text(json.dumps(views,indent=2));print(len(tasks),'atlases;',len(views),'views;',len(set(t['active_input'] for t in tasks)),'unique active entry contexts',flush=True)
