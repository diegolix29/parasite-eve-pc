"""Paired full-scene review; source phase proportions, no emulated scripts."""
from pathlib import Path
from PIL import Image
import argparse, json
from concurrent.futures import ThreadPoolExecutor
pool=ThreadPoolExecutor(max_workers=6);pending=[]
R=Path(__file__).resolve().parent
p=argparse.ArgumentParser();p.add_argument('--room');a=p.parse_args()
out=R/'gallery';out.mkdir(exist_ok=True)
views=json.loads((R/'views.json').read_text());manifest=json.loads((R/'composites/manifest.json').read_text())['composites'];data=[]
old=json.loads((R/'review-manifest.json').read_text())['complete_sequences'] if a.room and (R/'review-manifest.json').exists() else []
data=[x for x in old if x['room']!=a.room]
def encode(stem,kind):
 source=R/'composites'/f'{stem}-{kind}.png';target=out/f'{stem}-{kind}.webp'
 if target.exists()and target.stat().st_mtime>=source.stat().st_mtime:return
 im=Image.open(source).convert('RGB')
 if kind=='native':im=im.resize((im.width*4,im.height*4),Image.Resampling.NEAREST)
 temp=target.with_suffix('.tmp');im.save(temp,format='WEBP',lossless=True,method=1);temp.replace(target)
for v in views:
 if a.room and v['room']!=a.room:continue
 controls=[{'id':'base','slots':[{'entry':-1,'duration':1}]}]+list(v['controllers'])
 if v['room']in('M038','M039')and(R/'composites'/f"{v['room']}-g{v['group']:02d}-csync-p00-hd.png").exists():controls.append({'id':'sync','slots':[{'entry':-1,'duration':3}for _ in v['controllers'][0]['slots']]})
 controls.extend({'id':x['controller'],'slots':[{'entry':int(x['controller'][5:]),'duration':1}]}for x in manifest if(x['room'],x['group'])==(v['room'],v['group'])and str(x['controller']).startswith('entry'))
 for c in controls:
  frames=[]
  for phase,slot in enumerate(c['slots']):
   stem=f"{v['room']}-g{v['group']:02d}-c{c['id']}-p{phase:02d}"
   if not all((R/'composites'/f'{stem}-{kind}.png').exists()for kind in('native','hd')):break
   for kind in('native','hd'):pending.append(pool.submit(encode,stem,kind))
   frames.append(dict(stem=stem,entry=slot['entry'],duration=slot['duration']))
  if len(frames)!=len(c['slots']):continue
  data.append(dict(room=v['room'],group=v['group'],controller=c['id'],flags=c.get('flags'),step=c.get('step'),frames=frames))
for future in pending:future.result()
pool.shutdown()
data.sort(key=lambda x:(x['room'],x['group'],str(x['controller'])))
html='''<!doctype html><html lang="pl"><meta charset="utf-8"><title>Parasite Eve — animowane tła</title><style>body{background:#11151b;color:#e8ecf2;font:16px system-ui;margin:24px}h1{font-size:24px}h2{font-size:19px}article{border-top:1px solid #445064;padding:20px 0;margin-top:30px}.pair{display:flex;gap:12px;align-items:flex-start}figure{flex:1;min-width:0;margin:0}img{width:100%;height:auto;display:block}figcaption{color:#aeb8c7;padding:8px}.controls{display:flex;gap:12px;align-items:center;flex-wrap:wrap}button,select{background:#263145;color:white;padding:8px;border:1px solid #596b83}input[type=range]{width:240px}</style><h1>Animowane tła — prace po wersji 0.1</h1><p>Pełne sceny z oryginalnymi maskami, głębią i mieszaniem warstw. Stałe materiały OpenAI; ruch i zmienne warstwy odtworzone z oryginału. Podgląd offline, bez postaci i skryptów.</p><p>Jeden kontroler zmienia fazę, pozostałe są w pierwszej fazie. „Warstwa” to diagnostyczne włączenie pojedynczej warstwy, nie odtworzenie skryptu gry. „Razem” łączy fazy obu kontrolerów samochodu do oceny obrazu.</p><label>Mapa <select id="room" onchange="render()"></select></label> <label>Tempo przeglądu <select id="rate"><option>8</option><option selected>15</option><option>30</option><option>60</option></select> jednostek/s</label><p>Czasy faz zachowują proporcje źródła. To nie jest emulacja czasu gry. Powtarzanie służy przeglądowi; drzwi i sekwencje zatrzymują oryginalne skrypty.</p><main id="cards"></main><script>
const sequences=DATA,timers={},loaded={};const rooms=[...new Set(sequences.map(x=>x.room))];room.innerHTML=rooms.map(x=>`<option>${x}</option>`).join('');const requested=new URLSearchParams(location.search).get('room');if(rooms.includes(requested))room.value=requested;
function show(i,p){const f=sequences[i].frames[p];document.querySelector('#n'+i).src=f.stem+'-native.webp';document.querySelector('#h'+i).src=f.stem+'-hd.webp';document.querySelector('#s'+i).value=p;document.querySelector('#t'+i).textContent='faza '+p+' / wpis '+f.entry+' / czas '+f.duration}
function stop(i){clearTimeout(timers[i]);timers[i]=null;const b=document.querySelector('#b'+i);if(b)b.textContent='Odtwórz'}
async function play(i){if(timers[i]){stop(i);return}const b=document.querySelector('#b'+i);b.textContent='Wczytywanie…';if(!loaded[i]){await Promise.all(sequences[i].frames.flatMap(f=>['native','hd'].map(k=>new Promise((resolve,reject)=>{const im=new Image();im.onload=resolve;im.onerror=reject;im.src=f.stem+'-'+k+'.webp'}))));loaded[i]=true}if(!document.contains(b))return;b.textContent='Pauza';let p=+document.querySelector('#s'+i).value;if(p===sequences[i].frames.length-1)p=0;function step(){show(i,p);const f=sequences[i].frames[p];timers[i]=setTimeout(()=>{if(f.duration<0){stop(i);return}p++;if(p===sequences[i].frames.length){if(document.querySelector('#l'+i).checked)p=0;else{stop(i);return}}step()},Math.max(1,f.duration)*1000/+document.querySelector('#rate').value)}step()}
function render(){Object.keys(timers).forEach(stop);const selected=sequences.map((s,i)=>[s,i]).filter(([s])=>s.room===room.value);cards.innerHTML=selected.map(([s,i])=>{const c=s.controller,label=c==='base'?'widok bazowy':c==='sync'?'kontrolery razem':String(c).startsWith('entry')?'warstwa '+String(c).slice(5):'kontroler '+c;return `<article><h2>${s.room} / ujęcie ${s.group} / ${label}</h2><div class="pair"><figure><img loading="lazy" id="n${i}"><figcaption>Oryginał</figcaption></figure><figure><img loading="lazy" id="h${i}"><figcaption>HD — z atlasów</figcaption></figure></div><div class="controls"><button id="b${i}" onclick="play(${i})">Odtwórz</button><input id="s${i}" type="range" min="0" max="${s.frames.length-1}" value="0" oninput="stop(${i});show(${i},+this.value)"><label><input id="l${i}" type="checkbox">Powtarzaj</label><span id="t${i}"></span></div></article>`}).join('');selected.forEach(([s,i])=>show(i,0));history.replaceState(null,'','?room='+room.value)}render();</script></html>'''.replace('DATA',json.dumps(data))
(out/'index.html').write_text(html)
(R/'review-manifest.json').write_text(json.dumps({'offline_only':True,'complete_sequences':data,'total_controllers':sum(len(v['controllers'])for v in views)},indent=2))
print(len(data),'complete paired sequences/views',flush=True)
