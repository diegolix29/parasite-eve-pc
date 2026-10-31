// Historical pilot experiment. The expansion uses native source restoration.
async function(chains,jobs,ready){
 const root='/Users/hasik/Projects/parasite-eve-pc/docs/assets/hd-animated';
 if(chains.some(c=>!['M038','M039','M040','M041','M042','M043'].includes(c.room)||c.frames.some(f=>f.empty)))throw Error('Pilot-only phase experiment; use source-guided restoration for the full catalog');
 const lookup=new Map(jobs.map(j=>[j.id,j])),done=new Set(ready),pending=new Map();
 const quote=s=>"'"+s.replaceAll("'","'\\''")+"'";
 const generate=async(frame,chain,previous,first)=>{
  if(done.has(frame.id))return;
  if(pending.has(frame.id))return pending.get(frame.id);
  const work=(async()=>{
   const refs=previous?[frame.source,previous.source,root+'/generated/'+previous.id+'.png',root+'/generated/'+first.id+'.png']:[frame.source,root+'/generated/'+chain.base+'.png'];
   const prompt=`Use case: precise-object-edit. Restore ONE exact original Parasite Eve background-layer phase, ${chain.room}, group ${chain.group}, controller ${chain.controller}, phase ${frame.phase}/${chain.frames.length-1}. IMAGE1 is the absolute target: its current source geometry, visibility, reflections, light state and animation pose must be reproduced exactly at the same normalized coordinates. ${previous?'IMAGE2 is the PREVIOUS ORIGINAL phase; IMAGE3 is its matching HD restoration. The difference between IMAGE2 and IMAGE1 is the ONLY animation change to make to IMAGE3. IMAGE4 is the FIRST HD phase, supplied to keep a consistent material and a clean cycle. Do not copy the previous pose unchanged; faithfully implement the small original motion/state delta.':'IMAGE2 is the canonical HD material anchor for this exact view. Restore the target layer context, including any intentionally exposed surfaces, without copying an occluder over them.'} Keep camera, full framing and aspect ${chain.size[0]}:${chain.size[1]}, exact architecture, object silhouettes, material identity, color and source brightness. This is a texture layer used with original tile coverage/depth and independent native animation controllers, not a video. Do not flatten unrelated moving layers or introduce animation not present in IMAGE1. For a door or window, reproduce the exact current opening angle and revealed surface. For a moving reflection or scenery, reproduce the current SOURCE motion and identity rather than random texture changes. Do not alter the stationary car pillars, mirrors, dashboard, seats, railings, floor seams or lamp fixtures. Preserve empty interiors; no people or passengers. For light/beacon phases keep original color and intensity changes; no arbitrary exposure flicker. Never invent readable words: legible source text stays exact, unreadable source text remains softly unreadable. No added objects, architecture, seams, labels or borders, no crop, zoom, regrading or camera motion. Ideally return ${chain.size[0]*4}x${chain.size[1]*4}. The last-to-first source transition is part of the sequence: keep it as coherent as neighboring transitions, without forcing different original poses to become identical.`;
   const result=await tools.image_gen__imagegen({referenced_image_paths:refs,prompt,transparent_background:false});
   const path=result.output_hint.match(/as (\/Users\/[^\n ]+\.png) by default/)?.[1];
   if(!path||!/^\/Users\/hasik\/\.codex\/generated_images\/[0-9a-f-]+\/exec-[0-9a-f-]+\.png$/.test(path))throw Error('No output '+frame.id);
   const meta={id:frame.id,room:chain.room,group:chain.group,controller:chain.controller,phase:frame.phase,duration:frame.duration,previous:previous?.id??null,first:first?.id??null,refs,prompt,tool:'OpenAI built-in imagegen; exact model version not exposed',generated_original:path,status:'generated phase; temporal/geometry review pending'};
   const cmd='cp '+quote(path)+' '+quote(root+'/generated/'+frame.id+'.partial.png')+' && mv '+quote(root+'/generated/'+frame.id+'.partial.png')+' '+quote(root+'/generated/'+frame.id+'.png')+' && python3 -c '+quote('import pathlib,sys;p=pathlib.Path(sys.argv[1]);q=p.with_suffix(".tmp");q.write_text(sys.argv[2]);q.replace(p)')+' '+quote(root+'/metadata/'+frame.id+'.json')+' '+quote(JSON.stringify(meta,null,2));
   const write=await tools.exec_command({cmd,max_output_tokens:100});if(write.exit_code!==0)throw Error(write.output);
   done.add(frame.id);notify({room:chain.room,controller:chain.controller,phase:frame.phase,status:'phase saved'});
  })();pending.set(frame.id,work);return work;
 };
 const results=await Promise.allSettled(chains.map(async chain=>{
  let previous=null,first=chain.frames[0];
  for(const frame of chain.frames){await generate(frame,chain,previous,first);previous=frame;}
  return {room:chain.room,controller:chain.controller,slots:chain.frames.length};
 }));
 results.forEach((r,i)=>text({room:chains[i].room,controller:chains[i].controller,status:r.status,error:r.status==='rejected'?String(r.reason).slice(0,500):undefined}));
}
