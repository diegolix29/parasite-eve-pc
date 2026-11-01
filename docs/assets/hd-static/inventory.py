"""Conservative full-game static-background inventory; all outputs stay private."""
from pathlib import Path
import importlib.util, json, struct, hashlib, collections
ROOT=Path(__file__).resolve().parents[3]; OUT=Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('survey',ROOT/'docs/assets/widescreen/survey.py'); survey=importlib.util.module_from_spec(spec);spec.loader.exec_module(survey)
report=json.loads((ROOT/'docs/assets/widescreen/catalog.json').read_text())
discs={1:survey.Disc('/Users/hasik/Projects/parasite-pc/disc/Parasite Eve (Disc 1).bin'),2:survey.Disc('/Users/hasik/Projects/parasite-pc/disc/Parasite Eve (USA) (Disc 2).bin')}; archives={i:d.find('PE.IMG') for i,d in discs.items()}
rows=[]
for room in report['rooms']:
 if not any(v['tiles_all'] for v in room['views']):continue
 d=discs[room['disc']]; archive=archives[room['disc']]; start,scratch,tex,sectors=[room[k] for k in ['start_sector','scratch_sectors','texture_sectors','room_sectors']]
 blob=d.read(archive,(start+scratch+tex)*2048,sectors*2048); directory=survey.u32(blob,4); records=survey.u32(blob,directory+0x1c)&0x3fffff; offset=survey.u32(blob,records+4)&0xffffff; length=survey.u32(blob,records)&0xffffff;g=blob[offset:offset+length];ep=survey.u32(g,20)
 entries=[]
 for e in range(survey.u16(g,6)):
  p=ep+e*56
  if not survey.u16(g,p+38):continue
  entries.append({'index':e,'flags':g[p],'group':g[p+36],'tiles':survey.u16(g,p+38),'modulus':list(struct.unpack_from('<HH',g,p+4)),'origin':list(struct.unpack_from('<hh',g,p+8)),'speed':list(struct.unpack_from('<hh',g,p+28))})
 reasons=[]
 if room['map'] in (set(range(351,367)) - {358}) or room['id']=='E28':reasons.append('ui_tutorial_or_title_card')
 if survey.u16(g,4):reasons.append('animation_controllers')
 if any(not e['flags']&2 for e in entries):reasons.append('hidden_or_switchable_entries')
 if any(e['flags']&12 for e in entries):reasons.append('scroll_or_parallax_flags')
 texture_bytes=d.read(archive,start*2048,(scratch+tex)*2048)
 rows.append({**{k:room[k] for k in ('disc','id','map','location','views','geometry_sha256')},'classification':'static_candidate' if not reasons else 'excluded_dynamic_or_uncertain','reasons':reasons,'controllers':survey.u16(g,4),'entries':entries,'texture_sha256':hashlib.sha256(texture_bytes).hexdigest()})
summary={'room_disc_records':len(rows),'static_disc_records':sum(r['classification']=='static_candidate' for r in rows),'static_maps':len({r['id'] for r in rows if r['classification']=='static_candidate'}),'excluded_reasons':dict(collections.Counter(s for r in rows for s in r['reasons'])),'qualification':'Conservative serialized-data candidates, not a runtime proof. Camera panning alone is allowed; native masks, depth, per-entry palettes and script visibility remain authoritative.'}
(OUT/'inventory.json').write_text(json.dumps({'summary':summary,'rooms':rows},indent=2));print(json.dumps(summary,indent=2))
