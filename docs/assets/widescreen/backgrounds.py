from pathlib import Path
import json,struct,importlib.util,collections
import numpy as np
from PIL import Image
spec=importlib.util.spec_from_file_location('survey',str(Path(__file__).with_name('survey.py')));survey=importlib.util.module_from_spec(spec);spec.loader.exec_module(survey)
survey.scan()
u32=survey.u32;u16=survey.u16;OUT=survey.OUT;D=survey.Disc('/Users/hasik/Projects/parasite-pc/disc/Parasite Eve (Disc 1).bin');IMG=D.find('PE.IMG')
report=json.loads((OUT/'catalog.json').read_text());errors=[];(OUT/'backgrounds').mkdir(exist_ok=True)
def upload(vram,b):
 directory=u32(b,4);p=u32(b,directory+0x28);count=p>>22;offset=p&0x3fffff;assert offset+20*count<=len(b)
 for i in range(count):
  q=offset+20*i;source=u32(b,q+4);rect=u32(b,q+8);primary=source&0xffffff
  for j,src,r in [(0,source,rect),(1,u32(b,q+12),u32(b,q+16))]:
   off=src&0xffffff
   if j and not off:continue
   if j:off+=primary
   height=src>>24
   if not j and not height:height=256
   width=r&1023;x=(r>>10)&2047;y=r>>21
   if not height or not width:continue
   assert x+width<=1024 and y+height<=512 and off+width*height*2<=len(b),(i,j,x,y,width,height,off,len(b))
   a=np.frombuffer(b,dtype='<u2',count=width*height,offset=off).reshape(height,width);vram[y:y+height,x:x+width]=a
for room in report['rooms']:
 if room['disc']!=1 or not room['views']:continue
 try:
  start=room['start_sector'];scratch=room['scratch_sectors'];tex=room['texture_sectors'];sectors=room['room_sectors'];blob=D.read(IMG,(start+scratch+tex)*2048,sectors*2048);di=u32(blob,4);rp=u32(blob,di+0x1c)&0x3fffff;off=u32(blob,rp+4)&0xffffff;length=u32(blob,rp)&0xffffff;g=blob[off:off+length];ep=u32(g,20);entries=u16(g,6)
  vram=np.zeros((512,1024),dtype='<u2');upload(vram,D.read(IMG,start*2048,scratch*2048));upload(vram,D.read(IMG,(start+scratch)*2048,tex*2048))
  # 8bpp reads two palette indices per VRAM word; UV and CLUT stay per tile.
  vb=vram.view(np.uint8).reshape(512,2048)
  for view in room['views']:
   w,h=view['width'],view['height'];assert 0<w<=2048 and 0<h<=2048
   out=np.zeros((h,w),dtype=np.uint16);covered=np.zeros((h,w),dtype=bool);tiles=[]
   for e in range(entries):
    m=ep+56*e
    if not g[m]&2 or g[m+36]!=view['group']:continue
    pp=m+u32(g,m+40);uv=m+u32(g,m+44);sx,sy=struct.unpack_from('<hh',g,m+12);count=u16(g,m+38);depth=(u32(g,m)>>8)&4095
    for t in range(count):
     a=u32(g,pp+4*t);word=u32(g,uv+8*t);u=g[uv+8*t+4];v=g[uv+8*t+3];semi=bool(u32(g,uv+8*t+4)&0x10000000)
     x=sx+(a>>22);y=sy+((a>>12)&1023);tx=(word>>10)&0x3c0;ty=(word>>7)&256;cx=(word>>5)&0x3f0;cy=word&511;mode=(word>>22)&3
     indices=vb[(ty+np.arange(v,v+16)%256)%512][:,(tx*2+np.arange(u,u+16)%256)%2048];pixels=vram[cy,(cx+indices.astype(np.uint16))%1024]
     tiles.append((depth+(a&4095),e*100000+t,x,y,pixels,semi,mode))
   for _,_,x,y,pixels,semi,mode in sorted(tiles,key=lambda t:(t[0],t[1]),reverse=True):
    x0=max(0,x);y0=max(0,y);x1=min(w,x+16);y1=min(h,y+16)
    if x0>=x1 or y0>=y1:continue
    p=pixels[y0-y:y1-y,x0-x:x1-x].copy();mask=p!=0;dest=out[y0:y1,x0:x1]
    if semi:
     stp=(p&0x8000)!=0
     for shift in [0,5,10]:
      fg=(p>>shift)&31;bg=(dest>>shift)&31
      if mode==0:c=(fg.astype(int)+bg)//2
      elif mode==1:c=np.minimum(31,fg.astype(int)+bg)
      elif mode==2:c=np.maximum(0,bg.astype(int)-fg)
      else:c=np.minimum(31,bg.astype(int)+fg//4)
      p=(p&~np.uint16(31<<shift))|np.where(stp,c,fg).astype(np.uint16)<<shift
    dest[mask]=p[mask];covered[y0:y1,x0:x1]|=mask
   rgb=np.stack([(out&31)*255//31,((out>>5)&31)*255//31,((out>>10)&31)*255//31],axis=-1).astype(np.uint8)
   alpha=(covered*255).astype(np.uint8);rgba=np.dstack((rgb,alpha));name=f"{room['id']}-g{view['group']:02d}.png";Image.fromarray(rgba).save(OUT/'backgrounds'/name)
   view['background_png']='backgrounds/'+name;view['opaque_fraction']=round(float(covered.mean()),5)
   dark=np.max(rgb,axis=2)<=12
   view['dark_or_empty_edge_fraction']=[round(float((dark|~covered)[:,:16].mean()),5),round(float((dark|~covered)[:,-16:].mean()),5)]
   view['static_tiles_drawn']=len(tiles)
 except Exception as e:errors.append({'id':room['id'],'error':str(e)})
(OUT/'catalog.json').write_text(json.dumps(report,indent=2));(OUT/'background-decode.json').write_text(json.dumps({'errors':errors,'rendered':len(list((OUT/'backgrounds').glob('*.png'))),'limitations':['Static default flags only; animated palettes/entries and runtime brightness not simulated','No live actors; unfilled pixels retain alpha, not proof of authored black','Graphics reconstructed for local analysis, not a widescreen runtime implementation']},indent=2));print('backgrounds',len(list((OUT/'backgrounds').glob('*.png'))),'errors',errors[:5],flush=True)
