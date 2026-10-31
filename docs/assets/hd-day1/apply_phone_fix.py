from pathlib import Path
from PIL import Image
import json,shutil,hashlib
R=Path(__file__).resolve().parent;v=next(v for v in json.loads((R/'views.json').read_text()) if v['room']=='M020');jobs=json.loads((R/'phone-fix-jobs.json').read_text());b=jobs[0]['local_bbox'];basepath=R/'generated'/v['base_input'];archive=R/'review/phone-canonical-before-object-fix.png'
if not archive.exists():shutil.copy2(basepath,archive)
base=Image.open(archive).convert('RGB').resize(tuple(x*4 for x in v['active_size']),Image.Resampling.LANCZOS);patch=Image.open(R/'review/phone-final-canonical.png').convert('RGB').resize(((b[2]-b[0])*4,(b[3]-b[1])*4),Image.Resampling.LANCZOS);base.paste(patch,(b[0]*4,b[1]*4));base.save(basepath)
for j in jobs:
 shutil.copy2(R/'review'/('phone-final-'+j['id']+'.png'),R/'source-only-generated'/(j['id']+'.png'))
 metadata=json.loads((R/'metadata'/('phone-final-'+j['id']+'.json')).read_text());metadata['status']='Accepted source-layer correction; complete phone and partial hidden fragments treated separately';(R/'metadata'/('source-only-'+j['id']+'.json')).write_text(json.dumps(metadata,indent=2))
# The static background also owns frontmost parts of the telephone. Correct its
# material with the registered native-composite crop, while the partial animated
# layer deliberately retains exposed desk/phone fragments from its own source.
override={'id':'df3980369ebc78b324a1f921','bbox':b,'image':'review/phone-final-canonical.png','reason':'M020 static backdrop phone misinterpreted as a tape deck; native-composite crop restores the desk telephone. Do not apply to animated partial layer11.'}
(R/'registered-overrides.json').write_text(json.dumps([override],indent=2));(R/'metadata/phone-canonical-patch.json').write_text(json.dumps({'canonical_id':Path(v['base_input']).stem,'native_composite_source':'review/phone-canonical-native-input.png','restored_crop':'review/phone-final-canonical.png','bbox':b,'prior_full_generated_image':'review/phone-canonical-before-object-fix.png','result_sha256':hashlib.sha256(basepath.read_bytes()).hexdigest(),'static_layer_override':override},indent=2))
