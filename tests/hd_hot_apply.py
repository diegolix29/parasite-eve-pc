"""Real-disc, hidden-GPU regression: install/enable HD in an already loaded scene."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile


def main(executable):
    disc, pack = os.getenv('PE_TEST_DISC'), os.getenv('PE_TEST_HD_BACKGROUNDS')
    if not disc or not pack:
        return 77
    pack = str(Path(pack).resolve())
    output = Path(tempfile.mkdtemp(prefix='pe-hd-hot-', dir=Path(executable).resolve().parent))
    timeline = [(10000,8),(10010,0),(11000,16),(11010,0),(11500,16384),(11510,0)]
    for tick in (17000,17600,18200,18800,19400,20000,20600,21200,21800):
        timeline.extend(((tick,16384),(tick+10,0)))
    for tick, mask in ((23000,8),(23600,8),(24200,16384),(24800,16384)):
        timeline.extend(((tick,mask),(tick+10,0)))

    def run(scene, late):
        directory = output / f'{scene}-{late}'
        directory.mkdir()
        env = dict(os.environ, SDL_AUDIODRIVER='dummy', PSYZ_AUDIO_CLOCK='video',
                   PE_CONFIG_HOME=str(directory/'config'), PE_HD_BACKGROUNDS='' if late else pack)
        checkpoint = 25000 if scene == 'field' else 9900
        args = [str(Path(executable).resolve()), '--disc', str(Path(disc).resolve()),
                '--headless-gpu', '--internal-resolution', '4', '--widescreen', '1',
                '--frame-limit', str(checkpoint+100), '--control-at', str(checkpoint),
                '--dump-frame', str(directory/'frame.ppm'), '--dump-vram', str(directory/'vram.bin'),
                '--dump-audio', str(directory/'audio.pcm')]
        if scene == 'field':
            for tick, mask in timeline:
                args.extend(('--pad-at', f'{tick}:{mask}'))
        child = subprocess.Popen(args, cwd=directory, env=env, stdin=subprocess.PIPE,
                                 stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        snapshots = []
        try:
            with (directory/'game.log').open('w') as log:
                for line in child.stdout:
                    log.write(line)
                    if not line.startswith('{"event":"control"'):
                        continue
                    state = json.loads(line)
                    assert state['display']['rgb24'] == (scene == 'title')
                    if scene == 'field':
                        assert state['scene'] == 0xa8000148
                    snapshots.append((state, (directory/'frame.ppm').read_bytes(),
                                      hashlib.sha256((directory/'vram.bin').read_bytes()).hexdigest(),
                                      hashlib.sha256((directory/'audio.pcm').read_bytes()).hexdigest()))
                    step = len(snapshots)
                    command = 'quit' if step == 4 else (
                        f'backgrounds {0 if step == 2 else 1} {pack}' if late else 'advance 8 0')
                    child.stdin.write(command+'\n')
                    child.stdin.flush()
            assert child.wait(timeout=10) == 0 and len(snapshots) == 4
            return snapshots
        finally:
            if child.poll() is None:
                child.kill()
                child.wait()

    for scene in ('title', 'field'):
        baseline, late = run(scene, False), run(scene, True)
        for i, (a, b) in enumerate(zip(baseline, late)):
            assert a[0] == b[0] and a[2:] == b[2:], f'{scene}: native state/VRAM/PCM changed at {i}: {output}'
            assert (a[1] == b[1]) == (i in (1,3)), f'{scene}: HD activation/toggle ineffective at {i}: {output}'
    (output/'proof.json').write_text(json.dumps(dict(passed=True, scenes=['title','field'],
        late_activation_matches_startup=True, off_on_effective=True, native_state_vram_pcm_equal=True), indent=2))
    print(f'Late HD activation and off/on pass on title and field; native state/VRAM/PCM exact: {output}')
    return 0

if __name__ == '__main__':
    sys.exit(main(sys.argv[1]))
