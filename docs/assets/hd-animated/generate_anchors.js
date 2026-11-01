async function(jobs,ready){
 const root='/Users/hasik/Projects/parasite-eve-pc/docs/assets/hd-animated';
 const done=new Set(ready),byid=new Map(jobs.map(j=>[j.id,j])),pending=new Map();
 const quote=s=>"'"+s.replaceAll("'","'\\''")+"'";
 const write=async(path,value)=>{const r=await tools.exec_command({cmd:'python3 -c '+quote('import pathlib,sys;p=pathlib.Path(sys.argv[1]);q=p.with_suffix(".tmp");q.write_text(sys.argv[2]);q.replace(p)')+' '+quote(path)+' '+quote(JSON.stringify(value,null,2)),max_output_tokens:100,workdir:"/tmp",login:false});if(r.exit_code!==0)throw Error(r.output)};
 async function generate(j){
  if(done.has(j.id)||j.source_only)return;
  if(pending.has(j.id))return pending.get(j.id);
  const work=(async()=>{
   try{
    if(j.reference){const parent=byid.get(j.reference);await generate(parent);if(!done.has(j.reference))throw Error('Material reference unavailable: '+j.reference)}
    const source=root+'/active-inputs/'+j.id+'.png';const reference=j.reference_file||(j.reference?root+'/generated/'+j.reference+'.png':null);
    const prompt=`Use case: precise-object-edit. Faithful high-resolution material restoration of IMAGE1, an original 1997 Parasite Eve prerendered background. This complete source view is ${j.size[0]}x${j.size[1]}; preserve EXACT full framing, aspect ratio, fixed camera, perspective and normalized pixel coordinates. ${reference?'IMAGE2 is a MATERIAL reference from the same place/building or an already restored matching view. Use it for consistent surface finish only. It must never override IMAGE1 geometry, current door state, objects, colors, lighting or camera.':''} ${j.material_note||''} Keep every wall, door, window, floor border/joint, staircase, railing, lamp, furniture, machine, curtain, car, shadow and reflection in precisely the source position and proportions. Add restrained fine detail only inside existing forms. Preserve the original floor material and seam layout: do not invent tile grids on a smooth surface, strong marble veins, crumpled metallic cobbles or new carved patterns. Retain source dark areas, exposure, color palette and original CG art direction. Legible source lettering must remain exact. ${j.literal_text?'The clearly legible sign includes exactly '+j.literal_text+'. Preserve that spelling.':''} Unreadable text must stay intentionally soft and unreadable in its original layout; NEVER invent plausible words, numbers, labels, license plates or signatures. Preserve existing statues, mannequins, paintings, source figures and displays; do not remove or redesign them. Do not add people, passengers, faces, objects, architecture, lights or text. This is an aligned game texture restoration, not a new illustration. Keep every black canvas margin, do not zoom/crop/rotate/recenter to fill it. Ideally return ${j.size[0]*4}x${j.size[1]*4}; one full image with no borders, padding, labels or contact sheet.`;
    let timer;const timeout=new Promise((_,reject)=>{timer=setTimeout(()=>reject(Error('Generation timed out; safe to resume from saved anchors')),360000)});
    let result;try{result=await Promise.race([tools.image_gen__imagegen({referenced_image_paths:reference?[source,reference]:[source],prompt,transparent_background:false}),timeout])}finally{clearTimeout(timer)}
    const path=result.output_hint.match(/as (\/Users\/[^\n ]+\.png) by default/)?.[1];if(!path||!/^\/Users\/hasik\/\.codex\/generated_images\/[0-9a-f-]+\/exec-[0-9a-f-]+\.png$/.test(path))throw Error('No generated path '+j.id);
    const copy=await tools.exec_command({cmd:'cp '+quote(path)+' '+quote(root+'/generated/'+j.id+'.partial.png')+' && mv '+quote(root+'/generated/'+j.id+'.partial.png')+' '+quote(root+'/generated/'+j.id+'.png'),max_output_tokens:100,workdir:"/tmp",login:false});if(copy.exit_code!==0)throw Error(copy.output);
    await write(root+'/metadata/'+j.id+'.json',{id:j.id,kind:'view material anchor',views:j.views,source,reference,source_size:j.size,prompt,tool:'OpenAI built-in imagegen; exact model version not exposed',generated_original:path,status:'generated; registration and visual review pending'});
    done.add(j.id);notify({id:j.id,views:j.views,status:'anchor saved'});
   }catch(e){await write(root+'/metadata/'+j.id+'.failed',{id:j.id,views:j.views,error:String(e)});throw e}
  })();pending.set(j.id,work);return work;
 }
 const queue=jobs.filter(j=>!j.source_only&&!done.has(j.id));let index=0;const results=[];
 await Promise.all(Array.from({length:12},async()=>{while(index<queue.length){const j=queue[index++];try{await generate(j);results.push({id:j.id,status:'ready'})}catch(e){results.push({id:j.id,status:'failed',error:String(e).slice(0,300)});notify({id:j.id,status:'retry needed'})}}}));
 text({requested:queue.length,ready:results.filter(x=>x.status==='ready').length,failed:results.filter(x=>x.status==='failed')});
}
