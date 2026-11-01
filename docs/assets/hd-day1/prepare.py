from pathlib import Path
from PIL import Image
import json,hashlib,shutil,collections
R=Path(__file__).resolve().parent;B=R.parent/'hd-benchmark';P=R.parent/'hd-poc';data=json.loads((R/'tasks.json').read_text());views=json.loads((R/'views.json').read_text());byview={(v['room'],v['group']):v for v in views}
for name in ['generated','metadata','canvas','review']:(R/name).mkdir(exist_ok=True)
jobs={}
for v in views:
 key=v['base_input'][:-4];jobs.setdefault(key,{'id':key,'kind':'base','source':str(R/'active-inputs'/v['base_input']),'views':[],'entries':[],'reference':None})['views'].append([v['room'],v['group']])
for t in data['tasks']:
 key=t['active_input'][:-4];v=byview[t['room'],t['group']]
 job=jobs.setdefault(key,{'id':key,'kind':'entry','source':str(R/'active-inputs'/t['active_input']),'views':[],'entries':[],'reference':v['base_input'][:-4]});job['entries'].append({'room':t['room'],'group':t['group'],'entry':t['entry'],'control':t['control']})
 if [t['room'],t['group']] not in job['views']:job['views'].append([t['room'],t['group']])
# Reuse the already reviewed active-v4 M010 assets and calibrated M002 asset.
for t in data['tasks']:
 if t['room'] not in ['M002','M010']:continue
 source=P/'OpenAI-active-v4'/t['input']
 if not source.exists():continue
 im=Image.open(source).convert('RGB');x0,y0,x1,y1=t['active_bbox'];im=im.crop((x0*4,y0*4,x1*4,y1*4));dest=R/'generated'/t['active_input'];im.save(dest);job=jobs[t['active_input'][:-4]];job['reused_from']=str(source);job['status']='complete';(R/'metadata'/(job['id']+'.json')).write_text(json.dumps({'status':'reused existing reviewed output','source':str(source),'sha256':hashlib.sha256(dest.read_bytes()).hexdigest()},indent=2))
for job in jobs.values():
 if (R/'generated'/(job['id']+'.png')).exists():job['status']='complete'
 else:job['status']='pending'
 # Stable canvas, not a per-state black bounding box.
 im=Image.open(job['source']);job['size']=list(im.size)
(R/'jobs.json').write_text(json.dumps(list(jobs.values()),indent=2));(R/'scope.json').write_text(json.dumps({'scope':'Day1 conservative asset superset: M001..M037 including optional theater/sewer variants and nighttime police-car shots. M007/M008/M015/M019 have no serialized2D views. Reachability verified separately by original script replay.','rooms_requested':37,'rooms_with_backgrounds':len({v['room'] for v in views}),'views':len(views),'controllers':sum(len(r['controls']) for r in data['rooms']),'entry_atlases':len(data['tasks']),'unique_generation_jobs':len(jobs),'jobs_reused':sum(j['status']=='complete' for j in jobs.values()),'native_tile_cells':sum(len(t['tiles']) for t in data['tasks']),'native_semantics':'Original coverage/STP, independent controllers, phase durations, camera movement, depth and gameplay remain authoritative. Static scene material sharing is offline, not a video replacing controllers.'},indent=2));print(json.loads((R/'scope.json').read_text()))
