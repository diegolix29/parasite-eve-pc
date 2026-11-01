from pathlib import Path
import struct,mmap,json,collections,hashlib
ROOT=Path(__file__).resolve().parents[3];OUT=ROOT/'docs/assets/widescreen';OUT.mkdir(parents=True,exist_ok=True)
def u32(b,o):return struct.unpack_from('<I',b,o)[0]
def u16(b,o):return struct.unpack_from('<H',b,o)[0]
class Disc:
 def __init__(self,path):
  self.file=open(path,'rb');self.data=mmap.mmap(self.file.fileno(),0,access=mmap.ACCESS_READ)
  pvd=self.sectors(16,1);r=pvd[156:];self.root=(u32(r,2),u32(r,10))
 def sectors(self,lba,count):return b''.join(self.data[(lba+i)*2352+24:(lba+i)*2352+24+2048]for i in range(count))
 def find(self,name):
  lba,size=self.root;raw=self.sectors(lba,(size+2047)//2048);o=0
  while o<len(raw):
   n=raw[o]
   if not n:o=(o//2048+1)*2048;continue
   rec=raw[o:o+n];key=rec[33:33+rec[32]].decode('ascii','replace').split(';')[0]
   if key==name:return (u32(rec,2),u32(rec,10))
   o+=n
  raise ValueError(name)
 def read(self,extent,offset,size):
  lba,length=extent;assert 0<=offset<=offset+size<=length,(offset,size,length)
  return self.sectors(lba+offset//2048,(offset%2048+size+2047)//2048)[offset%2048:offset%2048+size]
def signed(v):return (v+32768)%65536-32768
def scan():
 allrows=[];errors=[];discs=[]
 for discid,name,exename in [(1,'Parasite Eve (Disc 1).bin','SLUS_006.62'),(2,'Parasite Eve (USA) (Disc 2).bin','SLUS_006.68')]:
  d=Disc('/Users/hasik/Projects/parasite-pc/disc/'+name);img=d.find('PE.IMG')
  try:exe_extent=d.find(exename)
  except ValueError:
   cnf=d.read(d.find('SYSTEM.CNF'),0,d.find('SYSTEM.CNF')[1]).decode();print(cnf);raise
  exe=d.read(exe_extent,0,exe_extent[1]);base=u32(exe,0x18)
  # Same resident range-table address on the USA pair; validate every range.
  tableoff=0x800+0x80093378-base
  pack=d.read(img,72*2048,15*2048);labels=pack[u32(pack,44):].split(b'\0');locations=pack[u32(pack,36):u32(pack,36)+440]
  meta={'disc':discid,'image_name':name,'exe':exename,'exe_sha256':hashlib.sha256(exe).hexdigest(),'pe_img_size':img[1]};discs.append(meta)
  for idx in range(450):
   start,bits=struct.unpack_from('<II',exe,tableoff+idx*8);scratch=bits&255;tex=(bits>>8)&4095;room=(bits>>20)&4095
   if not start and not bits:continue
   mapid=idx+1;row={'disc':discid,'map':mapid,'id':f'M{mapid:03d}'if mapid<=409 else f'E{mapid-410:02d}','start_sector':start,'scratch_sectors':scratch,'texture_sectors':tex,'room_sectors':room,'location':labels[locations[mapid]].decode('shift_jis','replace')if mapid<len(locations)and locations[mapid]<len(labels)else'', 'views':[]}
   try:

    if not room:
     row['status']='empty-range';allrows.append(row);continue
    blob=d.read(img,(start+scratch+tex)*2048,room*2048)
    directory=u32(blob,4);packed=u32(blob,directory+0x1c);n=packed>>22;records=packed&0x3fffff
    assert n>0,('no taskAux',n)
    off=u32(blob,records+4)&0xffffff;length=u32(blob,records)&0xffffff;g=blob[off:off+length];declared=u32(g,0);entries=u16(g,6);views=u16(g,10);ep=u32(g,0x14);vp=u32(g,0x1c)
    assert 0x3c<=declared<=length and 0<views<256 and ep+entries*56<=declared and vp+views*52<=declared,(length,declared,entries,views,ep,vp)
    row['geometry_sha256']=hashlib.sha256(g[:declared]).hexdigest();row['geometry_bytes']=declared
    for v in range(views):
     pos=vp+52*v;w,h,minx,maxx,miny,maxy=struct.unpack_from('<HHhhhh',g,pos+40)
     tiles=[];active=[];mesh_ids=[]
     for e in range(entries):
      m=ep+e*56
      if g[m+36]!=v:continue
      flags=g[m];count=u16(g,m+38);pp=m+u32(g,m+40);uv=m+u32(g,m+44)
      if pp+count*4>declared or uv+count*8>declared:continue
      mesh_ids.append(e);sx,sy=struct.unpack_from('<hh',g,m+12)
      for t in range(count):
       a=u32(g,pp+4*t);x=sx+((a>>22)&1023);y=sy+((a>>12)&1023);tiles.append((x,y))
       if flags&2:active.append((x,y))
     def bounds(t):return [min(x for x,y in t),min(y for x,y in t),max(x for x,y in t)+16,max(y for x,y in t)+16]if t else None
     row['views'].append({'group':v,'width':w,'height':h,'camera_bounds':[minx,maxx,miny,maxy],'projection':u16(g,pos),'mesh_entries':len(mesh_ids),'tiles_all':len(tiles),'tiles_active':len(active),'tile_bounds_all':bounds(tiles),'tile_bounds_active':bounds(active),'_active_tiles':active})
    allrows.append(row)
   except (AssertionError,ValueError,struct.error,IndexError)as e:row['error']=str(e);errors.append(row);allrows.append(row)
  report={'discs':discs,'rooms':allrows,'errors':[{'disc':r['disc'],'id':r['id'],'error':r['error']}for r in errors]}
  (OUT/'catalog.json').write_text(json.dumps(report,indent=2));print('disc',discid,'parsed',sum(bool(r['views'])for r in allrows if r['disc']==discid),'errors',sum(r['disc']==discid for r in errors),flush=True)
 print('View dimensions',collections.Counter((v['width'],v['height'])for r in allrows for v in r['views']).most_common(25));print('errors',report['errors'][:10]);print('views',sum(len(r['views'])for r in allrows))

if __name__ == "__main__":scan()
