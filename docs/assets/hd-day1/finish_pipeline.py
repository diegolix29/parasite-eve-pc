from pathlib import Path
import subprocess,json,time,hashlib,sys
R=Path(__file__).resolve().parent;PY=sys.executable;start=time.monotonic();expected=json.loads((R/'source-only-retry-jobs.json').read_text())+json.loads((R/'source-only-retry-extra.json').read_text())
while True:
 missing=[j for j in expected if not (R/'source-only-generated'/(j['id']+'.png')).exists()]
 if not missing:break
 print(len(missing),'pose corrections pending',flush=True)
 if time.monotonic()-start>1800:raise RuntimeError('Generation still incomplete; no final pack claim')
 time.sleep(15)
def run(script,*args):
 print('RUN',script,*args,flush=True);subprocess.run([PY,str(R/script),*args],cwd=R,check=True)
run('compose_local.py');run('reconcile_animation.py');run('normalize.py','--require-complete','--coherent')
assert (R/'pack-before-animation-fit/manifest.json').exists(),'Missing immutable fallback pack for rejected registrations'
run('../hd-poc/pack.py','OpenAI','--root',str(R),'--destination',str(R/'pack'),'--no-family-lock','--fallback-pack',str(R/'pack-before-animation-fit'),'--retain-controller','M029:0:0','--retain-controller','M036:0:0');run('audit_pack.py')
# Capture exact input/selected-image provenance of this immutable test candidate.
jobs=json.loads((R/'jobs.json').read_text());records=[]
for j in jobs:
 paths=[R/'source-only-generated'/(j['id']+'.png'),R/'local-generated'/(j['id']+'.png'),R/'generated'/(j['id']+'.png')];image=next(p for p in paths if p.exists());records.append({'id':j['id'],'source':j['source'],'views':j['views'],'chosen_generated_image':str(image.relative_to(R)),'sha256':hashlib.sha256(image.read_bytes()).hexdigest()})
(R/'candidate-provenance.json').write_text(json.dumps({'jobs':records,'tool':'OpenAI built-in imagegen; underlying model version not exposed','scope':'Full Day1 serialized 2D background coverage; native masks/depth/controllers retained','visual_review':'separate; not implied by file completeness','animation_postprocess':json.loads((R/'coherence-report.json').read_text()),'registration_exclusions':json.loads((R/'coherence-exclusions.json').read_text()),'source_lettering_restoration':json.loads((R/'source-lettering.json').read_text()),'registered_crop_overrides':json.loads((R/'registered-overrides.json').read_text()) if (R/'registered-overrides.json').exists() else [],'native_detail_regions':json.loads((R/'native-detail-regions.json').read_text()) if (R/'native-detail-regions.json').exists() else [],'canonical_crop_patches':[json.loads((R/'metadata/phone-canonical-patch.json').read_text())] if (R/'metadata/phone-canonical-patch.json').exists() else []},indent=2))
run('replay.py','hd');proof=json.loads((R/'verify/hd-current-tail/proof.json').read_text());assert proof['completed_day1'];assert json.loads((R/'verify/hd-current-tail/parity.json').read_text())['passed']
log=(R/'verify/hd-current-tail/log').read_text();assert 'over-budget HD sprite atlas' not in log;assert 'runtime error:' not in log
run('composite.py');run('review_composites.py');run('review_corrections.py','--rooms','M017,M021')
(R/'pipeline-complete.json').write_text(json.dumps({'asset_and_runtime_checks_passed':True,'seconds':time.monotonic()-start,'full_day1_hd_completed':True,'native_state_vram_pcm_parity':True,'visual_review_still_separate':True},indent=2));print('ASSET/RUNTIME PIPELINE PASSED; visual review separate',flush=True)
