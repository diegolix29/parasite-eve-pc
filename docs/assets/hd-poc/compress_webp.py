"""Build a separate high-quality WebP pack; native reference planes stay lossless."""
import argparse,concurrent.futures,hashlib,json,shutil,subprocess,tempfile
from pathlib import Path
from PIL import Image

def convert(source,destination,quality=95,workers=4):
 if not 1<=quality<=100 or not 1<=workers<=32:raise ValueError("Invalid quality/worker count")
 source,destination=Path(source).resolve(),Path(destination).resolve()
 if destination.exists():raise FileExistsError(destination)
 manifest=json.loads((source/'manifest.json').read_text());destination.parent.mkdir(parents=True,exist_ok=True)
 staging=Path(tempfile.mkdtemp(prefix=destination.name+'.partial-',dir=destination.parent))
 sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
 records={r['key']:r for r in manifest['atlases']}
 jobs={k:(r['file'],r['atlas_sha256']) for k,r in records.items()}
 extras=manifest.get('extras',[])
 for r in extras:
  if r['kind']=='widescreen-atlas':jobs[Path(r['file']).stem]=(r['file'],r['sha256'])
 def job(item):
  key,(name,digest)=item
  if len(key)!=16 or any(c not in '0123456789abcdef' for c in key) or Path(name).name!=name:raise ValueError('Unsafe pack entry')
  path=source/name
  if sha(path)!=digest:raise ValueError('Bad source hash: '+name)
  with Image.open(path) as image:
   w,h=image.size
   if w%68 or h%136:raise ValueError('Bad atlas dimensions')
   top=staging/(key+'.source.png');ref=staging/(key+'.ref.png');webp=staging/(key+'.webp')
   image.crop((0,0,w,h//2)).save(top)
   original=image.crop((0,h//2,w,h));original.save(ref,optimize=True)
   subprocess.run(['cwebp','-quiet','-q',str(quality),'-m','6','-exact',str(top),'-o',str(webp)],check=True)
   with Image.open(ref) as check:
    if check.convert('RGBA').tobytes()!=original.convert('RGBA').tobytes():raise ValueError('Reference changed')
   with Image.open(webp) as check:
    check.load()
    pixel_sha=hashlib.sha256(check.convert('RGBA').tobytes()+original.convert('RGBA').tobytes()).hexdigest()
    if check.size!=(w,h//2):raise ValueError('Invalid encoded dimensions')
   top.unlink()
  return key,dict(file=webp.name,pixel_sha256=pixel_sha,atlas_sha256=sha(webp),reference_file=ref.name,reference_sha256=sha(ref),source_atlas_sha256=digest,compressed_bytes=webp.stat().st_size+ref.stat().st_size)
 try:
  converted={}
  with concurrent.futures.ThreadPoolExecutor(max_workers=workers) as pool:
   for k,v in pool.map(job,jobs.items()):
    converted[k]=v
    if len(converted)%100==0:print(f'{len(converted)}/{len(jobs)}',flush=True)
  manifest['atlases']=[{**r,**converted[r['key']]} for r in manifest['atlases']]
  output=[]
  for r in extras:
   name=r['file'];path=source/name
   if Path(name).name!=name or sha(path)!=r['sha256']:raise ValueError('Bad extra: '+name)
   if r['kind']=='widescreen-atlas':
    v=converted[path.stem];output.extend([dict(file=v['file'],kind=r['kind'],sha256=v['atlas_sha256']),dict(file=v['reference_file'],kind='widescreen-reference',sha256=v['reference_sha256'])])
   elif r['kind']=='title-background':
    target=staging/'title.webp';subprocess.run(['cwebp','-quiet','-lossless','-exact','-m','6',str(path),'-o',str(target)],check=True)
    with Image.open(path) as a,Image.open(target) as b:
     if a.convert('RGBA').tobytes()!=b.convert('RGBA').tobytes():raise ValueError('Title changed')
    output.append({**r,'file':target.name,'sha256':sha(target)})
   else:shutil.copy2(path,staging/name);output.append(r)
  manifest['extras']=output;manifest['image_format']='webp';manifest['encoding']=dict(quality=quality,reference='lossless PNG',title='lossless WebP')
  fields=('room','group','entry','key','file','atlas_sha256','reference_file','reference_sha256','source_atlas_sha256','pixel_sha256','compressed_bytes')
  manifest['atlases']=[{k:r[k] for k in fields if k in r} for r in manifest['atlases']]
  manifest['source_manifest_sha256']=sha(source/'manifest.json')
  manifest['compression']=dict(lossless=False,quality=quality,material='WebP',native_reference='lossless PNG',title='lossless WebP')
  for r in manifest.get('widescreen',[]):r.pop('positions',None)
  manifest['image_bytes']=sum(p.stat().st_size for p in staging.iterdir() if p.is_file())
  (staging/'manifest.json').write_text(json.dumps(manifest,separators=(',',':'))+'\n');staging.rename(destination)
  print('complete bytes',sum(p.stat().st_size for p in destination.iterdir()),flush=True)
 except BaseException:shutil.rmtree(staging,ignore_errors=True);raise
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('source');p.add_argument('destination');p.add_argument('--quality',type=int,default=95);p.add_argument('--workers',type=int,default=4);a=p.parse_args();convert(a.source,a.destination,a.quality,a.workers)
