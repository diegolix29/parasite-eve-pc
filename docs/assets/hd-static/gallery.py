"""Create a local full-scene review gallery from packed-tile composites."""
from pathlib import Path
import json,html
from PIL import Image
R=Path(__file__).resolve().parent;out=R/'gallery';out.mkdir(exist_ok=True)
views=json.loads((R/'composites'/'manifest.json').read_text())['composites'];cards=[]
for v in views:
 if v['controller'] is not None:continue
 stem=f"{v['room']}-g{v['group']:02d}-cbase-p00"
 for suffix in ('native','hd'):
  im=Image.open(R/'composites'/f'{stem}-{suffix}.png').convert('RGB')
  if suffix=='native':im=im.resize((im.width*4,im.height*4),Image.Resampling.NEAREST)
  im.thumbnail((1536,2048));im.save(out/f'{stem}-{suffix}.webp',quality=94)
 cards.append(f'''<article data-room="{v['room']}"><h2>{v['room']} / {v['group']}</h2><div class="pair"><figure><img loading="lazy" src="{stem}-native.webp"><figcaption>Oryginał</figcaption></figure><figure><img loading="lazy" src="{stem}-hd.webp"><figcaption>OpenAI HD</figcaption></figure></div></article>''')
page='''<!doctype html><html lang="pl"><meta charset="utf-8"><title>Parasite Eve — statyczne tła HD</title><style>body{margin:24px;background:#11151b;color:#e7eaf0;font:16px system-ui}header{position:sticky;top:0;background:#11151bf5;padding:12px 0;z-index:1}h1{font-size:24px}input{padding:10px;background:#222b38;color:white;border:1px solid #526077}article{border-top:1px solid #3b4654;margin:30px 0}.pair{display:flex;align-items:flex-start;gap:12px}figure{margin:0;flex:1;min-width:0}img{width:100%;height:auto;display:block}figcaption{padding:8px;color:#adb8c8}</style><header><h1>Parasite Eve — statyczne tła HD</h1><p>Pełne sceny z atlasów gry, z oryginalnymi maskami, głębią i mieszaniem warstw. Podgląd offline: bez postaci i skryptów. Materiał do przeglądu, nie zatwierdzona wersja końcowa.</p><input id="filter" placeholder="Filtr mapy, np. M162"><span id="count"></span></header>'''+''.join(cards)+'''<script>const f=document.querySelector('#filter'),a=[...document.querySelectorAll('article')];function update(){let n=0;for(const e of a){e.hidden=!e.dataset.room.includes(f.value.trim().toUpperCase());n+=!e.hidden}document.querySelector('#count').textContent=' '+n+' ujęć'}f.oninput=update;update()</script></html>'''
(out/'index.html').write_text(page)
print(len(cards),'full-scene comparisons:',out/'index.html')
