from pathlib import Path
import subprocess,sys,json,os
R=Path(__file__).resolve().parents[1];PY=sys.executable
rooms=['M003','M006','M013','M016','M020','M021','M026','M027','M028','M029','M032','M033','M034','M037']
def run(script,*args):subprocess.run([PY,str(R/script),*args],check=True)
for script in ['apply_structural.py','apply_temporal.py','apply_m037_source_motion.py','apply_m027_motion.py','finalize_gate.py','apply_gate_material.py']:run('feedback-fixes/'+script)
for room in rooms:
 run('../hd-poc/pack.py','OpenAI-feedback','--root',str(R),'--destination',str(R/'pack-feedback'),'--no-family-lock','--entry-overrides',str(R/'feedback-fixes/entry-overrides'),'--room',room)
 for v in json.loads((R/'views.json').read_text()):
  if v['room']==room:run('composite.py','--room',room,'--group',str(v['group']),'--pack','pack-feedback','--output','composites-feedback')
# Retain every unmodified view/phase in the complete comparison gallery.
for p in (R/'composites').glob('*.png'):
 q=R/'composites-feedback'/p.name
 if not q.exists():os.link(p,q)
p=R/'composites-feedback/manifest.json';new=json.loads(p.read_text());old=json.loads((R/'composites/manifest.json').read_text());merged={(c['room'],c['group'],c['controller'],c['phase']):c for c in old['composites']}
for c in new['composites']:merged[(c['room'],c['group'],c['controller'],c['phase'])]=c
new['composites']=list(merged.values());p.write_text(json.dumps(new,indent=2))
run('audit_pack.py','--pack','pack-feedback','--report','pack-audit-feedback.json');run('audit_motion.py','--composites','composites-feedback','--report','motion-audit-feedback.json');assert not json.loads((R/'motion-audit-feedback.json').read_text())['frozen_native_changing_pairs']
print('Complete feedback candidate packed and checked.')
