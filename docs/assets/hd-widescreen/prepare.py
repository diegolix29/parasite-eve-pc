"""Register reviewed outpainting and package only new horizontal side tiles.
The existing atlases and their 4:3 pixels are never rewritten.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import struct
import cv2
import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[3]
ASSETS = ROOT / 'docs/assets'


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def catalog():
    views = {}
    rooms = {}
    for study in ('hd-static', 'hd-day1', 'hd-animated'):
        root = ASSETS / study
        tasks = json.loads((root / 'tasks.json').read_text())
        rooms.update({r['room']: r for r in tasks['rooms'] if 'geometry_key' in r})
        for v in json.loads((root / 'views.json').read_text()):
            views[v['room'], v['group']] = {**v, 'study': study}
    return views, rooms


def register(center, generated, margin=224):
    """Similarity registration; reject composition drift rather than hide it."""
    a = cv2.cvtColor(np.asarray(center.convert('RGB')), cv2.COLOR_RGB2GRAY)
    b = cv2.cvtColor(np.asarray(generated.convert('RGB')), cv2.COLOR_RGB2GRAY)
    sift = cv2.SIFT_create(nfeatures=6000)
    ka, da = sift.detectAndCompute(a, None)
    kb, db = sift.detectAndCompute(b, None)
    if da is None or db is None:
        raise ValueError('No reliable image registration features')
    matches = cv2.BFMatcher().knnMatch(db, da, k=2)
    good = [pair[0] for pair in matches if len(pair) == 2 and pair[0].distance < .65 * pair[1].distance]
    if len(good) < 30:
        raise ValueError('Too few registration matches')
    src = np.float32([kb[m.queryIdx].pt for m in good])
    dst = np.float32([ka[m.trainIdx].pt for m in good]) + [margin, 0]
    matrix, inliers = cv2.estimateAffinePartial2D(src, dst, method=cv2.RANSAC,
        ransacReprojThreshold=2, maxIters=10000)
    if matrix is None or int(inliers.sum()) < 30 or inliers.mean() < .65:
        raise ValueError('Outpainting moved the original composition')
    error = np.linalg.norm(cv2.transform(src[None], matrix)[0] - dst, axis=1)[inliers[:, 0] > 0]
    p95 = float(np.quantile(error, .95))
    if p95 > 2 or abs(matrix[0, 1]) > .02:
        raise ValueError('Outpainting cannot be aligned safely')
    pixels = cv2.warpAffine(np.asarray(generated.convert('RGB')), matrix,
        (center.width + margin * 2, center.height), borderMode=cv2.BORDER_REPLICATE)
    # Check that the warp has real generated pixels everywhere in the new sides.
    coverage = cv2.warpAffine(np.ones(b.shape, np.uint8), matrix,
        (center.width + margin * 2, center.height), flags=cv2.INTER_NEAREST)
    if not coverage[2:-2,2:-2].all():
        raise ValueError('Registered image does not cover the whole extension')
    pixels[:, margin:margin+center.width] = np.asarray(center.convert('RGB'))
    return Image.fromarray(pixels), dict(transform=matrix.tolist(), matches=len(good),
        inliers=int(inliers.sum()), residual_p95=p95)


def package(room, group, generated, center_path, pack):
    views, rooms = catalog()
    view = views[room, group]
    if view['controllers']:
        raise ValueError('Animated views require a reviewed source-driven phase extension; static export refused')
    entries=[e for e in rooms[room]['entries'] if e['group']==group and e['tiles']]
    if any(not (e['flags']&2) or (e['flags']&12) for e in entries):
        raise ValueError('Script-switched/scrolled layers require a phase-aware review; static export refused')
    center = Image.open(center_path).convert('RGB')
    x0,y0,x1,y1 = view['active_bbox']
    if center.size != ((x1-x0)*4,(y1-y0)*4):
        raise ValueError('Center dimensions do not match the original active view at 4x')
    material, registration = register(center, Image.open(generated), 224)
    native_x,native_y = view['native_origin']
    left,right,top,bottom = x0+native_x-56,x1+native_x+56,y0+native_y,y1+native_y
    cells = []
    for y in range(top, bottom, 16):
        for x in list(range(left, x0+native_x, 16)) + list(range(x1+native_x, right, 16)):
            cells.append((x,y))
    cols = min(16,len(cells));rows = math.ceil(len(cells)/cols)
    atlas = Image.new('RGBA',(cols*68,rows*136))
    for i,(x,y) in enumerate(cells):
        tile = Image.new('RGBA',(64,64))
        for py in range(64):
            for px in range(64):
                nx,ny=x+px/4,y+py/4
                if ny>=bottom or (x0+native_x<=nx<x1+native_x) or nx>=right:
                    continue
                sx=int((nx-(x0+native_x))*4)+224;sy=int((ny-top)*4)
                if 0<=sx<material.width and 0<=sy<material.height:
                    tile.putpixel((px,py),(*material.getpixel((sx,sy)),255))
        padded=Image.fromarray(np.pad(np.asarray(tile),((2,2),(2,2),(0,0)),mode='edge'))
        atlas.paste(padded,((i%cols)*68,(i//cols)*68))
    geometry=rooms[room]['geometry_key']
    key=hashlib.sha256(f'{geometry}:{group}:widescreen-v1'.encode()).hexdigest()[:16]
    pack=Path(pack);manifest_path=pack/'manifest.json';manifest=json.loads(manifest_path.read_text())
    metadata = dict(room=room,group=group,key=key,geometry_key=geometry,
        bounds=[left,right,top,bottom],cells=len(cells),source=digest(Path(center_path)),
        generated=digest(Path(generated)),registration=registration,animation='static')
    # Rebuild the sidecar with all already installed groups for this geometry.
    records=[r for r in manifest.get('widescreen',[]) if (r['room'],r['group'])!=(room,group)]
    metadata['positions']=cells;records.append(metadata)
    selected=[r for r in records if r['geometry_key']==geometry]
    data=bytearray(struct.pack('<II',0x31574550,len(selected)))
    for r in selected:
        data.extend(struct.pack('<IIQiiii',r['group'],r['cells'],int(r['key'],16),*r['bounds']))
        for xy in r['positions']:data.extend(struct.pack('<hh',*xy))
    atlas.save(pack/(key+'.png'))
    sidecar=pack/(geometry+'-wide.bin');sidecar.write_bytes(data)
    extra_files={key+'.png':'widescreen-atlas',sidecar.name:'widescreen-index'}
    extras=[r for r in manifest.get('extras',[]) if r['file'] not in extra_files]
    extras.extend(dict(file=name,kind=kind,sha256=digest(pack/name)) for name,kind in extra_files.items())
    manifest['extras']=extras;manifest['widescreen']=records
    temporary=manifest_path.with_suffix('.partial');temporary.write_text(json.dumps(manifest,indent=2)+'\n');temporary.replace(manifest_path)
    material.save(Path(generated).with_name(f'{room}-g{group:02d}-wide-registered.png'))
    Path(generated).with_name(f'{room}-g{group:02d}-registration.json').write_text(json.dumps(metadata,indent=2)+'\n')
    return metadata


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--room',required=True);p.add_argument('--group',type=int,default=0)
    p.add_argument('--generated',type=Path,required=True);p.add_argument('--center',type=Path,required=True)
    p.add_argument('--pack',type=Path,required=True)
    a=p.parse_args();print(json.dumps(package(a.room,a.group,a.generated,a.center,a.pack),indent=2))
