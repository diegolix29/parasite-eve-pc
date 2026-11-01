"""Optional real-disc regression for HD side tiles (no visible game windows).
PE_TEST_DISC and PE_TEST_HD_BACKGROUNDS must point to the reviewed local assets.
Usage: python tests/hd_widescreen.py /path/to/parasite-eve-pc
"""
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


def main(executable):
    disc, pack = os.getenv('PE_TEST_DISC'), os.getenv('PE_TEST_HD_BACKGROUNDS')
    if not disc or not pack:
        return 77
    pack = Path(pack).resolve()
    manifest = json.loads((pack/'manifest.json').read_text())
    if not any(r['room']=='M002' and r['group']==0 for r in manifest.get('widescreen', [])):
        return 77
    # Real fresh-game route to the first theatre exterior, held in a dialogue.
    # The stationary camera catches extensions incorrectly gated on camera updates.
    timeline = [(10000,8),(10010,0),(11000,16),(11010,0),(11500,16384),(11510,0)]
    for tick in (17000,17600,18200,18800,19400,20000,20600,21200,21800):
        timeline.extend(((tick,16384),(tick+10,0)))
    for tick, mask in ((23000,8),(23600,8),(24200,16384),(24800,16384)):
        timeline.extend(((tick,mask),(tick+10,0)))
    output = Path(tempfile.mkdtemp(prefix='pe-hd-wide-',dir=Path(executable).resolve().parent))
    baseline = output/'original-width-pack';baseline.mkdir()
    for source in pack.iterdir():
        if source.is_file() and not source.name.endswith('-wide.bin'):
            try: os.link(source,baseline/source.name)
            except OSError: shutil.copyfile(source,baseline/source.name)

    broken=output/'incomplete-width-pack';broken.mkdir()
    for source in baseline.iterdir():
        try: os.link(source,broken/source.name)
        except OSError: shutil.copyfile(source,broken/source.name)
    geometry=next(r['geometry_key'] for r in manifest['widescreen'] if r['room']=='M002' and r['group']==0)
    (broken/(geometry+'-wide.bin')).write_bytes(b'PEW1\x01\x00\x00\x00')

    def run(case):
        wide, extended = case
        directory=output/f'{wide}-{extended}';directory.mkdir()
        env=dict(os.environ,SDL_AUDIODRIVER='dummy',PSYZ_AUDIO_CLOCK='video',
                 PE_CONFIG_HOME=str(directory/'config'),PE_HD_BACKGROUNDS=str((baseline,pack,broken)[extended]))
        args=[str(Path(executable).resolve()),'--disc',str(Path(disc).resolve()),'--headless-gpu',
              '--internal-resolution','4','--widescreen',str(wide),'--frame-limit','25100',
              '--control-at','25000','--dump-frame',str(directory/'frame.ppm'),
              '--dump-vram',str(directory/'vram.bin'),'--dump-audio',str(directory/'audio.pcm')]
        for tick, mask in timeline: args.extend(('--pad-at',f'{tick}:{mask}'))
        child=subprocess.Popen(args,cwd=directory,env=env,stdin=subprocess.PIPE,stdout=subprocess.PIPE,
                               stderr=subprocess.STDOUT,text=True)
        states=[];frames=[]
        try:
            with (directory/'game.log').open('w') as log:
                for line in child.stdout:
                    log.write(line)
                    if 'runtime error:' in line or 'ERROR: AddressSanitizer' in line:
                        child.kill();raise AssertionError(line)
                    if not line.startswith('{"event":"control"'):continue
                    state=json.loads(line)
                    assert state['scene']==0xa8000148 and state['display']['rgb24']==0
                    states.append((state,hashlib.sha256((directory/'vram.bin').read_bytes()).hexdigest(),
                                   hashlib.sha256((directory/'audio.pcm').read_bytes()).hexdigest()))
                    frames.append((directory/'frame.ppm').read_bytes())
                    child.stdin.write(('advance 8 0' if len(states)==1 else 'quit')+'\n');child.stdin.flush()
        except BaseException:
            if child.poll() is None: child.kill()
            child.wait()
            raise
        assert child.wait()==0 and len(states)==2
        return case,(states,frames)

    with ThreadPoolExecutor(max_workers=2) as pool:
        results=dict(pool.map(run,[(0,0),(0,1),(1,0),(1,1),(1,2)]))
    for wide in (0,1):
        a,b=results[wide,0],results[wide,1]
        assert a[0]==b[0],f'HD side tiles changed game state, native VRAM or PCM: {output}'
        for original,enhanced in zip(a[1],b[1]):
            if not wide:
                assert original==enhanced,'4:3 changed'
                continue
            header,pixels=enhanced.split(b'\n255\n',1)
            width,height=map(int,header.splitlines()[-1].split())
            margin=(width-320*4)//2
            lit=0
            for y in range(40,height-40):
                for x in list(range(8,margin-8))+list(range(width-margin+8,width-8)):
                    at=(y*width+x)*3
                    lit+=max(pixels[at:at+3])>12
            assert lit>(height-80)*margin//2,f'Side material is absent/stale: {output}'
    assert results[1,0]==results[1,2],f'Incomplete sidecar did not fall back exactly: {output}'
    (output/'proof.json').write_text(json.dumps(dict(passed=True,checkpoints=2,
        exact_4_3=True,native_vram_pcm_and_state_equal=True,visible_side_material=True,incomplete_sidecar_exact_fallback=True),indent=2)+'\n')
    print(f'HD widescreen: 4:3 exact; state/VRAM/PCM exact; persistent side artwork: {output}')
    return 0


if __name__=='__main__':sys.exit(main(sys.argv[1]))
