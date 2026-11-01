"""Existing recorded fresh-game pad route through Day1 and Day2 transition.
Runs the original executable headlessly; no state injection, stat edits or saves.
"""
from pathlib import Path
import argparse,json,os,subprocess,hashlib,shutil,time
R=Path(__file__).resolve().parent;ROOT=R.parents[2];OLD=Path('/tmp/pe-campaign-replay-v3')
p=argparse.ArgumentParser();p.add_argument('mode',choices=['native','hd']);p.add_argument('--pack',default='pack');p.add_argument('--work-name');p.add_argument('--terminal-only',action='store_true');p.add_argument('--software',action='store_true');a=p.parse_args();work=R/'verify'/(a.work_name or a.mode+'-current-tail'+('-terminal' if a.terminal_only else '')+('-software' if a.software else ''));work.mkdir(parents=True,exist_ok=True);(work/'bu00').mkdir(exist_ok=True);(work/'bu10').mkdir(exist_ok=True)
raw=json.loads((R/'current-fixture-args.json').read_text());timeline={};i=0
while i<len(raw):
 if raw[i]=='--pad-at':
  t,b=map(lambda x:int(x,0),raw[i+1].split(':'));timeline[t]=b;i+=2
 else:i+=1
old_args=json.loads((OLD/'live/args.json').read_text())
for index,option in enumerate(old_args):
 if option=='--pad-at':
  t,b=map(lambda x:int(x,0),old_args[index+1].split(':'))
  if t>261381:timeline[t]=b
commands=[c for line in (OLD/'commands.jsonl').read_text().splitlines() if (c:=json.loads(line))['start']<=291736]
for c in commands:
 if c['start']>=261381:timeline[c['start']]=c['buttons']
end=291736;assert any(c['start']+c['frames']==end for c in commands)
points=[end] if a.terminal_only else sorted(set(range(20000,end,5000))|{24000,28000,269174,end});boundaries=sorted(set(points)|{t for t in timeline if points[0]<t<=end});(R/'empty-pack').mkdir(exist_ok=True)
env=dict(os.environ,PE_HD_BACKGROUNDS=str(R/a.pack if a.mode=='hd' else R/'empty-pack'),SDL_AUDIO_DRIVER='dummy',PSYZ_AUDIO_CLOCK='video',SDL_ASSERT='abort')
args=[str(ROOT/'build/parasite-eve-pc'),'--disc','/Users/hasik/Projects/parasite-pc/disc/Parasite Eve (Disc 1).cue','--disc2','/Users/hasik/Projects/parasite-pc/disc/Parasite Eve (USA) (Disc 2).cue',('--headless' if a.software else '--headless-gpu'),'--internal-resolution','4','--geometry-correction','1','--antialiasing','0','--model-filtering','1','--frame-limit',str(end+100),'--control-at',str(points[0]),'--dump-frame',str(work/'frame.ppm'),'--dump-vram',str(work/'vram.bin'),'--dump-audio',str(work/'audio.pcm')]
if a.software:
 for option in ['--internal-resolution','--geometry-correction','--antialiasing','--model-filtering']:
  index=args.index(option);del args[index:index+2]
for t,b in sorted(timeline.items()):
 if t<=points[0]:args+=['--pad-at',f'{t}:{b}']
(work/'args.json').write_text(json.dumps(args,indent=2));child=subprocess.Popen(args,cwd=work,env=env,stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True);(work/'pid.json').write_text(json.dumps({'pid':child.pid}));states=[];start=time.monotonic()
with (work/'log').open('w') as log:
 for line in child.stdout:
  log.write(line)
  if 'HD background geometry' in line:log.flush();print(a.mode,line.strip(),flush=True)
  if not line.startswith('{"event":"control"'):continue
  state=json.loads(line);tick=state['vblanks'];assert tick in boundaries,(tick,boundaries)
  if tick in points:
   shutil.copy2(work/'frame.ppm',work/f'{tick}.ppm');shutil.copy2(work/'vram.bin',work/f'{tick}.vram')
   record={'state':state,'vram_sha256':hashlib.sha256((work/'vram.bin').read_bytes()).hexdigest(),'pcm_size':(work/'audio.pcm').stat().st_size};states.append(record);(work/'states.json').write_text(json.dumps(states,indent=2));log.flush();print(a.mode,tick,state['scene'],state['story_stage'],flush=True)
  index=boundaries.index(tick);mask=next((timeline[t] for t in sorted(timeline,reverse=True) if t<=tick),0);child.stdin.write(('quit' if index+1==len(boundaries) else f'advance {boundaries[index+1]-tick} {mask}')+'\n');child.stdin.flush()
assert child.wait()==0;pcm=hashlib.file_digest((work/'audio.pcm').open('rb'),'sha256').hexdigest();(work/'proof.json').write_text(json.dumps({'mode':a.mode,'end':end,'checkpoints':len(states),'seconds':time.monotonic()-start,'pcm_sha256':pcm,'source_timeline':'Verified current native alligator-entry fixture inputs + recorded post-entry commands; no state edits','scope':'Recorded Day1 input route; successful progression must be asserted separately, not inferred from exit0','story_stage':states[-1]['state']['story_stage'],'completed_day1':states[-1]['state']['story_stage']==136},indent=2))
if a.mode=='hd':
 baseline=json.loads((R/'verify'/('native-current-tail'+('-terminal' if a.terminal_only else '')+('-software' if a.software else ''))/'states.json').read_text());assert states==baseline,'native state/VRAM/audio-length mismatch';reference=json.loads((R/'verify'/('native-current-tail'+('-terminal' if a.terminal_only else '')+('-software' if a.software else ''))/'proof.json').read_text());assert reference['pcm_sha256']==pcm,'PCM mismatch';(work/'parity.json').write_text(json.dumps({'passed':True,'checkpoints':len(states),'full_state_and_native_vram_identical':True,'full_pcm_identical':True},indent=2))
print(a.mode,'DONE',time.monotonic()-start,flush=True)
