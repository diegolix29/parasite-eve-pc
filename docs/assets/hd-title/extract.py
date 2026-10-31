from pathlib import Path
import struct,json,importlib.util,hashlib,argparse
from PIL import Image
R=Path(__file__).resolve().parents[3];O=Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('survey',R/'docs/assets/widescreen/survey.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
parser=argparse.ArgumentParser(description='Extract the original USA title backdrop and menu resources for HD registration.')
parser.add_argument('disc',type=Path,help='USA Disc 1 raw BIN image')
args=parser.parse_args()
d=m.Disc(str(args.disc));exe=d.read(d.find('SLUS_006.62'),0,d.find('SLUS_006.62')[1]);base=m.u32(exe,0x18);sectors=struct.unpack_from('<81H',exe,0x800+0x800930d8-base);b=d.read(d.find('PE.IMG'),sectors[70]*2048,(sectors[71]-sectors[70])*2048)[0x4264:];rows=[]
for i in range(8):
 at=m.u32(b,4+i*4);w,h=struct.unpack_from('<HH',b,at+16);w,h=(320,204) if i==0 else (w,h);pixels=b[at+20:at+20+w*h*3]
 if 0<w<=1024 and 0<h<=1024 and len(pixels)==w*h*3:
  im=Image.frombytes('RGB',(w,h),pixels);im.save(O/f'original-{i}.png');rows.append(dict(index=i,offset=at,width=w,height=h,sha256=hashlib.sha256(pixels).hexdigest()))
(O/'original.json').write_text(json.dumps(rows,indent=2));print(rows)
