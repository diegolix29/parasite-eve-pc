from pathlib import Path
from PIL import Image,ImageDraw
import json,argparse
p=argparse.ArgumentParser();p.add_argument("--candidate",default="hd-current-tail");p.add_argument("--review",default="review");a=p.parse_args()
R=Path(__file__).resolve().parent;ticks=[24000,28000,50000,70000,100000,150000,180000,200000,225000,245000,265000,275000];states={x['state']['vblanks']:x['state'] for x in json.loads((R/'verify'/a.candidate/'states.json').read_text())}
for start in range(0,len(ticks),4):
 out=Image.new('RGB',(1280,1120));d=ImageDraw.Draw(out)
 for i,t in enumerate(ticks[start:start+4]):
  for col,mode in enumerate(['native','hd']):
   im=Image.open(R/'verify'/('native-current-tail' if mode=='native' else a.candidate)/f'{t}.ppm');im.thumbnail((630,250));out.paste(im,(col*640,i*280+25))
  d.text((0,i*280),f"tick{t} scene{states[t]['scene']:08x} story{states[t]['story_stage']} : native / OpenAI GPU gameplay",fill='white')
 out.save(R/a.review/f'runtime-{start//4:02d}.jpg')
