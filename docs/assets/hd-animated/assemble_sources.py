"""Combine fixed generated scenery with deterministic restoration of native states.

Every serialized state keeps its original geometry and duration. The union of
native temporal changes uses source restoration in every phase, including
the anchor phase. Hidden layer material and sparse foreground occluders also
come from their own source context, never from a differently occluded AI scene.
These are offline art operations; no runtime animation or game code is changed.
"""
from pathlib import Path
import argparse, json, shutil, os
import cv2
import numpy as np
from PIL import Image

R = Path(__file__).resolve().parent
p = argparse.ArgumentParser()
p.add_argument('--room')
p.add_argument('--require-complete', action='store_true')
p.add_argument('--shards',type=int,default=1)
p.add_argument('--shard',type=int,default=0)
a = p.parse_args()
assert 0 <= a.shard < a.shards
cv2.setNumThreads(1)
tasks = json.loads((R/'tasks.json').read_text())['tasks']
views = json.loads((R/'views.json').read_text())
anchors = {j['id']: j for j in json.loads((R/'anchor-jobs.json').read_text())}
av = {(v['room'], v['group']): v for v in json.loads((R/'anchor-views.json').read_text())}
alignment={x['id']:x for x in json.loads((R/'anchor-alignment.json').read_text())['anchors']} if (R/'anchor-alignment.json').exists() else {}
text = {x['id']: x['regions'] for x in json.loads((R/'text-regions.json').read_text())['anchors']}
out = R/'OpenAI-source-guided'
out.mkdir(exist_ok=True)
proof, pending = [], []

def rgb(path):
    return np.array(Image.open(path).convert('RGB'))

def write(path, data):
    tmp = path.with_suffix('.'+str(os.getpid())+'.tmp')
    Image.fromarray(np.uint8(np.clip(np.rint(data), 0, 255))).save(tmp, format='PNG',compress_level=3)
    tmp.replace(path)

def coverage(t, shape):
    mask = np.zeros(shape, np.uint8)
    for (x, y), ref in zip(t['tiles'], np.load(R/'refs'/t['ref'])):
        mask[y:y+16, x:x+16] |= np.uint8(ref != 0)
    return mask

for vi,v in enumerate(views):
    if vi % a.shards != a.shard:
        continue
    room, group = v['room'], v['group']
    if a.room and room != a.room:
        continue
    anchor = av[room, group]
    aid = anchor['id']
    job = anchors[aid]
    ap = R/'generated'/(aid+'.png')
    sp = R/'source-sr-full'/(aid+'.png')
    source_only = bool(job.get('source_only'))
    if source_only and sp.exists() and not ap.exists():
        shutil.copy2(sp, ap)
        (R/'metadata'/(aid+'.json')).write_text(json.dumps({
            'id': aid, 'method': 'Original source text/UI preserved with RealESRNet_x4plus',
            'source': job['source'], 'source_only': True,'reason':job.get('source_only_reason','Text-heavy source interface'),
            'weights_sha256': 'a820b9bde89a874d7599d545567308ce6c128fc8754a53208eda016d40aa81df'
        }, indent=2))
    if not ap.exists() or not sp.exists():
        pending.append([room, group, aid])
        continue
    registered=R/'registered-anchors'/(aid+'.png')
    registered_ok=registered.exists() and registered.stat().st_mtime>=max(ap.stat().st_mtime,sp.stat().st_mtime) and alignment.get(aid,{}).get('status')in ('source preserved','global affine registered')
    if a.require_complete and not registered_ok:
        pending.append([room,group,'Missing, stale or uncertain anchor registration']);continue
    ts = [t for t in tasks if (t['room'], t['group']) == (room, group)]
    missing = [t['active_input'] for t in ts if not (R/'source-sr-full'/t['active_input']).exists()]
    if missing:
        pending.append([room, group, missing])
        continue
    w, h = v['active_size']
    cw, ch = v['canvas_size']
    x0, y0, x1, y1 = v['active_bbox']
    original = rgb(R/'inputs'/anchor['canvas_input'])
    assert original.shape == (ch, cw, 3)
    restored_anchor = rgb(sp)
    assert restored_anchor.shape == (h*4, w*4, 3)
    generated = cv2.resize(rgb(registered if registered_ok else ap), (w*4, h*4), interpolation=cv2.INTER_LANCZOS4)
    # Lock original low-frequency illumination once per view. Shared material
    # references must not brighten a later night/damaged version of a room.
    # This field is fixed across all phases; phase lighting still comes from
    # each original state. High-frequency generated texture is retained.
    if not source_only:
        low_ai=cv2.GaussianBlur(generated.astype(np.float32),(0,0),16)
        low_source=cv2.GaussianBlur(restored_anchor.astype(np.float32),(0,0),16)
        illumination=np.clip((low_source+8)/(low_ai+8),.4,2.5)
        generated=np.uint8(np.clip(np.rint(generated*illumination),0,255))
    gain=np.ones(3)
    # Restore source glyph shapes. OCR supplies only bounding boxes, never text.
    protected = 0
    for region in ([] if source_only else text.get(aid, [])):
        bx0, by0, bx1, by1 = [int(k*4) for k in region['bbox']]
        bx0, by0, bx1, by1 = max(1,bx0), max(1,by0), min(w*4-1,bx1), min(h*4-1,by1)
        if bx1-bx0 < 4 or by1-by0 < 4:
            continue
        mask = np.zeros((h*4,w*4), np.uint8)
        mask[by0:by1,bx0:bx1] = 255
        source = np.uint8(np.clip(restored_anchor*gain,0,255))
        generated = cv2.seamlessClone(source,generated,mask,((bx0+bx1)//2,(by0+by1)//2),cv2.NORMAL_CLONE)
        protected += 1
    fixed = np.zeros((ch*4,cw*4,3), np.uint8)
    fixed[y0*4:y1*4,x0*4:x1*4] = generated
    dynamic = np.zeros((ch,cw),np.uint8)
    controlled = {s['entry'] for c in v['controllers'] for s in c['slots']}
    own = {}
    for t in ts:
        own[t['entry']] = coverage(t,(ch,cw))
    for controller in v['controllers']:
        states = [t for t in ts if t['entry'] in {s['entry'] for s in controller['slots']}]
        if not states:
            continue
        first = rgb(R/'inputs'/states[0]['input'])
        empty = any(not any(t['entry'] == s['entry'] for t in states) for s in controller['slots'])
        for t in states:
            dynamic |= np.any(rgb(R/'inputs'/t['input']) != first,axis=2).astype(np.uint8)
            if empty:
                dynamic |= own[t['entry']]
    # Shared support has one inward feather, constant for every state. No
    # rectangular crop-edge alpha, independently graded frame or optical flow.
    support = cv2.dilate(dynamic,np.ones((3,3),np.uint8))
    union = cv2.resize(support,(cw*4,ch*4),interpolation=cv2.INTER_NEAREST)
    common_alpha = np.clip(cv2.distanceTransform(np.pad(union,1),cv2.DIST_L2,5)[1:-1,1:-1]/8,0,1)
    for t in ts:
        native = rgb(R/'inputs'/t['input'])
        different = np.any(native != original,axis=2).astype(np.uint8)
        # Full source context differs wherever a hidden/background state owns
        # material absent from the selected anchor. Preserve those pixels.
        changed = cv2.dilate(different,np.ones((3,3),np.uint8))
        hi = cv2.resize(changed,(cw*4,ch*4),interpolation=cv2.INTER_NEAREST)
        alpha = np.maximum(common_alpha,np.clip(cv2.distanceTransform(np.pad(hi,1),cv2.DIST_L2,5)[1:-1,1:-1]/8,0,1))
        m = own[t['entry']]
        yy, xx = np.nonzero(m)
        if len(xx):
            # Sparse fences/poles must not inherit background lamps from an AI
            # composite. At dense-layer edges protect two native pixels too.
            density = len(xx)/((xx.max()-xx.min()+1)*(yy.max()-yy.min()+1))
            distance = cv2.distanceTransform(np.pad(m,1),cv2.DIST_L2,5)[1:-1,1:-1]
            protected_own = m.astype(np.float32) if density < .55 else np.clip((4-distance)/3,0,1)*m
            alpha = np.maximum(alpha,cv2.resize(protected_own,(cw*4,ch*4),interpolation=cv2.INTER_NEAREST))
        source = np.zeros(fixed.shape,np.float32)
        source[y0*4:y1*4,x0*4:x1*4] = rgb(R/'source-sr-full'/t['active_input']).astype(np.float32)*gain
        result = fixed.astype(np.float32)*(1-alpha[...,None])+source*alpha[...,None]
        write(out/t['input'],result)
    proof.append({'room':room,'group':group,'anchor':aid,'anchor_source_only':source_only,
                  'gain_rgb':gain.tolist(),'original_text_regions':protected,'registered_anchor':registered_ok,
                  'native_motion_pixels':int(dynamic.sum()),'contexts':len({t['input'] for t in ts}),
                  'controller_count':len(v['controllers'])})
    print(room,group,len(ts),'contexts',flush=True)
(R/('source-guided-assembly.json' if a.shards==1 else f'assembly-shard-{a.shard}.json')).write_text(json.dumps({
    'method':__doc__,'views':proof,'pending':pending,'illumination':'Fixed source low-frequency RGB field per view; generated fine texture, original phase lighting',
    'weights_sha256':'a820b9bde89a874d7599d545567308ce6c128fc8754a53208eda016d40aa81df',
    'inference_provenance':['source-sr-full/inference.json','source-sr-full/source-sr-inference.json','source-sr-full/source-sr-extra-inference.json']
},indent=2))
print(len(proof),'views assembled;',len(pending),'pending',flush=True)
if a.require_complete:
    assert not pending,pending
