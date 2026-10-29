static int room_actor_view(void)
{
    FieldActor actor={0}, parent={0};
    FieldActorState state={0};
    u8 action_state=1;
    RenderMatrix matrices[2]={0};
    actor.state=&state; state.action_state=&action_state;
    actor.parent=&parent; actor.next=&parent; actor.prev=&parent;
    actor.type_id=7; actor.sub_id=9; actor.flags=0x400; actor.mode=3;
    actor.anim.parts.integer=19; actor.anim_prev=7<<16;
    actor.pos_x=123<<16; actor.pos_y=-456*65536; actor.pos_z=789<<16;
    actor.rot_y=0x123;
    actor.motion_x=11; actor.accel_y=22; actor.gravity_z=33;
    actor.render_object.matrices=matrices;
    actor.render_object.model_matrix.translation[2]=42;
    RoomLink *view=(RoomLink *)&actor;
    FieldActorNode *node=(FieldActorNode *)&actor;
    if(node->w00!=&state || node->next!=(FieldActorNode *)&parent || node->w08!=(FieldActorNode *)&parent ||
       node->b0C!=7 || node->b0D!=9 || node->w98!=0x400) return 0;
    if(view->variant!=3 || view->winLo!=19 || view->winHi!=7 || view->link18C!=(RoomLink *)&parent)
        return 0;
    if(view->pos[0]!=actor.pos_x || view->pos[1]!=actor.pos_y || view->rotation[1]!=actor.rot_y ||
       view->vel[0]!=11 || view->move[1]!=22 || view->accel[2]!=33 || view->p238!=matrices ||
       view->modelTranslation[2]!=42) return 0;
    view->pos[2]=-5*65536; view->posMirror[1]=17; view->h250=0x400;
    view->target->flags=0x40000000;
    view->target->startFlag=9;
    if(actor.pos_z!=-5*65536 || actor.base_y!=17 || actor.render_object.flags_9C!=0x400 ||
       state.core_flags!=0x40000000 || state.reserveda8[4]!=9) return 0;
    FieldActor snapshot=actor;
    int (*release[])(PmSlotHeader *)={RoomLib_ReleaseHandlerB,RoomLib_ReleaseHandlerC,
        RoomLib_ReleaseHandlerD,RoomLib_ReleaseHandlerE};
    /* A movement owner can outlive its enemy event. Releasing it must still
     * clear its PM slot/signal, without acknowledging a missing event. */
    unsigned cases=0;
    for(unsigned method=0;method<4;++method) for(unsigned active=0;active<2;++active)
    for(unsigned target=0;target<3;++target) for(unsigned value=0;value<5;++value)
    for(unsigned rotation=0;rotation<(method==2?2:1);++rotation) {
        int signal=1; RoomEnt object={0};
        object.link=view; object.active=active; object.flag3=1; object.sub.signal=&signal;
        action_state=value; state.action_state=target==2?&action_state:NULL;
        actor.state=target?&state:NULL;
        ((RoomLibHandlerDState *)&object.sub)->copyRotation=rotation;
        FieldActor before=actor; FieldActorState core_before=state;
        unsigned expected=value;
        if(target==2 && (rotation?value==1:active)) expected=4;
        if(release[method]((PmSlotHeader *)&object)!=0 || action_state!=expected ||
           signal || object.flag3 || object.state!=4 || memcmp(&actor,&before,sizeof(actor)) ||
           memcmp(&state,&core_before,sizeof(state))) return 0;
        ++cases;
    }
    actor=snapshot;
    puts("Original room actor view: canonical fields and 150 B/C/D/E release lifecycle cases with live/absent events and D rotation gate passed");
    return 1;
}

static int original_m023_drift_states(void)
{
    const int phases[]={-1,0,1,2,32767},ages[]={-1,0,1,3,4,7,8,32767},modes[]={0,1,3};
    const int poses[][6]={{0,0,0,0,0,0},{1,2,3,4,5,6},{32767,-32768,-1,35,-35,32767},
        {65535,0,32768,65535,32768,1},{-1,-1,-1,-127,-128,128},{123,-456,789,-35,34,0}};
    int saved_age=g_FieldFxCurrentAge;unsigned cases=0;
    uint64_t hash=UINT64_C(14695981039346656037);
    for(unsigned phase=0;phase<5;++phase)for(unsigned age=0;age<8;++age)
    for(unsigned pose=0;pose<6;++pose)for(unsigned mode=0;mode<3;++mode){
        struct {u16 before;RoomM023DriftPulseCallbackView state;u16 after;} record;
        memset(&record,0xa5,sizeof(record));
        record.state=(RoomM023DriftPulseCallbackView){poses[pose][0],poses[pose][1],poses[pose][2],
            phases[phase],poses[pose][3],poses[pose][4],poses[pose][5]};
        g_FieldFxCurrentAge=ages[age];
        FieldFxCallbackListCallback callback=RoomM023_DriftPulseCallback;
        u32 result=callback(modes[mode],&record.state,0);
        for(unsigned i=0;i<sizeof(record.state);++i)hash=(hash^((u8 *)&record.state)[i])*UINT64_C(1099511628211);
        for(unsigned i=0;i<4;++i)hash=(hash^((result>>(i*8))&255))*UINT64_C(1099511628211);
        if(record.before!=0xa5a5 || record.after!=0xa5a5)return 0;
        ++cases;
    }
    g_FieldFxCurrentAge=saved_age;
    if(hash!=UINT64_C(0x5f4bf4da84fc4de5)){
        fprintf(stderr,"M023 drift CPU hash: %u %016llx\n",cases,(unsigned long long)hash);return 0;}
    puts("Original M023 drift: 720 unhooked USA CPU states, phase fallthrough, age boundaries, halfword wrap and callback guards passed");
    return 1;
}

static int authored_m023_effect_cycle(SceneAssetView *room)
{
    PmPrimarySlot bank[11]={0};FieldActor actor={0},player={0};
    FieldActorState state={0},player_state={0};u8 action=1;
    RenderMatrix matrices[21]={0};
    for(unsigned i=0;i<21;++i){
        for(unsigned axis=0;axis<3;++axis)matrices[i].rotation[axis][axis]=4096;
        matrices[i].translation[1]=-100;matrices[i].translation[2]=1024;
    }
    actor.state=&state;actor.mode=2;actor.render_object.matrices=matrices;
    state.action_state=&action;player.state=&player_state;
    player.pos_x=player.pos_z=10000<<16;
    PmTables old_tables=g_PmTables;Pe1GameState old_game=g_GameState;u32 old_flags=g_GameStateFlags;
    FieldActor *old_player=g_PlayerEntity;
    RenderBufferPrefix old_buffers=D_800B0E38;GteMatrix *old_view=g_GeomScroll.words.viewMatrix;
    int old_slot=g_ActiveDrawSlot,old_offset=g_DrawPacketOffset,old_age=g_FieldFxCurrentAge;
    void *old_program=g_FieldFxTaskPrograms[33];u32 old_random=Psyz_RandState();
    FieldFxTaskContext *old_context=g_FieldFxTaskContext;FieldFxObjectPrefix *old_owner=g_FieldFxOwner;
    FieldFxTaskSlot *old_task=g_FieldFxActiveTaskSlot;char *old_model=g_RoomM023BeaconModel;
    GteMatrix view={.m={{4096,0,0},{0,4096,0},{0,0,4096}}};
    static RenderGpuTag ordering[4096];
    struct {_Alignas(void *) u8 bytes[65536];u8 guard[16];} packets;
    g_PmTables=(PmTables){g_PmCmdHandlerArray,bank,NULL};g_FieldFxTaskPrograms[33]=&g_RoomM023Effects;
    g_GameStateFlags=0x80;g_GameState.flags&=~8u;g_GameState.tim_load_state=0;
    g_GameState.loaded_scene_archive=&room->header;g_PlayerEntity=&player;
    g_GeomScroll.words.viewMatrix=&view;
    Psyz_GteCtrlWrite(26,256);Psyz_GteCtrlWrite(27,1);Psyz_GteCtrlWrite(28,0);
    Psyz_GteCtrlWrite(29,1365);Psyz_GteCtrlWrite(30,1024);gte_SetGeomOffset(160,120);
    FieldFx_InitTexturePageTable();srand(1);memset(g_RoomM005ScriptAnchorView-4,0,20);
    int valid=Scene_LoadRoomAssets(118,&actor)==0 && Pm_SendCmd(0,0,0,0,0,0)==0;
    unsigned drawn=0,linked=0,seen=0;
    for(unsigned frame=0;valid && frame<180;++frame){
        memset(&packets,0xa5,sizeof(packets));
        for(unsigned depth=0;depth<4096;++depth)ordering[depth]=(RenderGpuTag){.address=0xffffff};
        g_ActiveDrawSlot=frame&1;g_DrawPacketOffset=0;
        D_800B0E38.ordering[g_ActiveDrawSlot]=ordering;D_800B0E38.packets[g_ActiveDrawSlot]=(char *)packets.bytes;
        valid=Pm_Start(0)==0 && Pm_Exec(0)==0;
        if(!valid){fprintf(stderr,"M023 task frame %u failed\n",frame);break;}
        for(unsigned slot=0;slot<8;++slot)if(bank[0].task.tasks.slots[slot].id!=0xffff)
            seen|=1u<<bank[0].task.tasks.slots[slot].id;
        if(g_DrawPacketOffset<0 || g_DrawPacketOffset>sizeof(packets.bytes)){valid=0;break;}
        drawn+=g_DrawPacketOffset!=0;
        for(unsigned depth=0;valid && depth<4096;++depth){
            uintptr_t address=ordering[depth].address;unsigned chain=0;
            while(address!=0xffffff){
                if(address<(uintptr_t)packets.bytes || address+sizeof(RenderGpuTag)>(uintptr_t)packets.bytes+g_DrawPacketOffset ||
                   address%_Alignof(RenderGpuTag) || ++chain>512){valid=0;break;}
                RenderGpuTag *tag=(void *)address;++linked;address=tag->address;
            }
        }
        for(unsigned i=0;i<16;++i)if(packets.guard[i]!=0xa5)valid=0;
    }
    valid=valid && drawn && linked && seen==7 && g_RoomM023BeaconModel && bank[0].task.tasks.count==0;
    if(!valid)fprintf(stderr,"M023 cycle drawn=%u linked=%u seen=%u remaining=%d\n",drawn,linked,seen,bank[0].task.tasks.count);
    Pm_Stop(0,&actor,0);
    g_FieldFxTaskContext=old_context;g_FieldFxOwner=old_owner;g_FieldFxActiveTaskSlot=old_task;g_FieldFxCurrentAge=old_age;
    g_PmTables=old_tables;g_GameState=old_game;g_GameStateFlags=old_flags;g_FieldFxTaskPrograms[33]=old_program;
    g_PlayerEntity=old_player;g_RoomM023BeaconModel=old_model;
    D_800B0E38=old_buffers;g_GeomScroll.words.viewMatrix=old_view;g_ActiveDrawSlot=old_slot;g_DrawPacketOffset=old_offset;
    srand(old_random);memset(g_RoomM005ScriptAnchorView-4,0,20);
    if(valid)puts("Original M023: 180 PM frames, all three task types, real room model, complete task teardown and OT/packet guards passed");
    return valid;
}

static int authored_paired_glow_cycle(SceneAssetView *room)
{
#define PAIRED_FAIL() do {fprintf(stderr,"Paired glow check line%u used%d\n",__LINE__,g_DrawPacketOffset);goto done;}while(0)
    RenderBufferPrefix old_buffers=D_800B0E38;Pe1GameState old_game=g_GameState;
    int old_slot=g_ActiveDrawSlot,old_offset=g_DrawPacketOffset;
    GteMatrix *old_view=g_GeomScroll.words.viewMatrix;FieldActor *old_player=g_PlayerEntity;
    FieldFxState *old_state=g_FieldFxScriptState;FieldFxObject *old_object=g_FieldFxScriptObject;
    FieldFxSlot *old_slots=g_FieldFxSlotTable;char *old_data=g_FieldFxScriptData;int *old_commands=g_FieldFxCommandTable;
    RoomFloorY old_floor=*g_RoomFloorY;u32 old_random=Psyz_RandState();int ok=0;
    GteMatrix view={.m={{4096,0,0},{0,4096,0},{0,0,4096}}};
    static RenderGpuTag ordering[4096];struct {_Alignas(16) u8 bytes[65536];u8 guard[32];} packets;
    g_GeomScroll.words.viewMatrix=&view;g_GameState.loaded_scene_archive=&room->header;g_RoomFloorY->y=0;
    Psyz_GteCtrlWrite(26,256);Psyz_GteCtrlWrite(29,1365);Psyz_GteCtrlWrite(30,1024);gte_SetGeomOffset(160,112);
    FieldFx_InitTexturePageTable();
    for(unsigned draw=0;draw<2;++draw){
        FieldFxObject object={0};FieldActor actor={0},player={0};FieldActorState core={0},player_core={0};u8 action=2;
        RenderMatrix matrix={.rotation={{4096,0,0},{0,4096,0},{0,0,4096}},.translation={0,-100,1200}};
        actor.state=&core;core.action_state=&action;core.control10.command_value=1;actor.render_object.matrices=&matrix;player.state=&player_core;
        actor.pos_y=-100*65536;actor.pos_z=1200*65536;object.prefix.actor=&actor;g_PlayerEntity=&player;
        g_ActiveDrawSlot=draw;D_800B0E38.ordering[draw]=ordering;D_800B0E38.packets[draw]=(char *)packets.bytes;srand(1);
        g_RoomPairedGlowHandlers.init();g_RoomPairedGlowHandlers.initialize((PmSlotHeader *)&object);
        if(object.state.commands!=g_RoomScript__paired)PAIRED_FAIL();
        /* The USA interpreter also visits only types0/1/3/4. Type2 remains
         * defined: its no-op callbacks preserve data and dispatch control. */
        u8 inert[300],expected[300];memset(inert,0xa5,sizeof(inert));memcpy(expected,inert,sizeof(inert));
        RoomFxControl inert_control={2,1,17},expected_control=inert_control;
        if(g_RoomInitList__paired[2](&object,&inert_control,inert) ||
           g_RoomDrawList__paired[2](&object,&inert_control,inert) ||
           g_RoomUpdateList__paired[2](&object,&inert_control,inert) ||
           memcmp(inert,expected,sizeof(inert)) || memcmp(&inert_control,&expected_control,sizeof(inert_control)))PAIRED_FAIL();
        unsigned seen=0,drawn=0,linked=0,frame=0;
        for(;frame<640;++frame){
            memset(&packets,0xa5,sizeof(packets));for(unsigned i=0;i<4096;++i)ordering[i]=(RenderGpuTag){.address=0xffffff};g_DrawPacketOffset=0;
            if(g_RoomPairedGlowHandlers.start((PmSlotHeader *)&object) || g_RoomPairedGlowHandlers.execute((PmSlotHeader *)&object))PAIRED_FAIL();
            for(unsigned i=0;i<64;++i)if(object.slots[i].flag){
                FieldFxSlot *slot=&object.slots[i];seen|=1u<<slot->handler_id;
                if(slot->handler_id>=5 || slot->data_offset<0 || (unsigned)slot->data_offset>=FIELDFX_DATA_SIZE ||
                   (uintptr_t)(object.data+slot->data_offset)%_Alignof(void *))PAIRED_FAIL();
            }
            if(g_DrawPacketOffset<0 || g_DrawPacketOffset>sizeof(packets.bytes))PAIRED_FAIL();
            drawn+=g_DrawPacketOffset!=0;
            for(unsigned i=0;i<4096;++i){uintptr_t link=ordering[i].address;unsigned count=0;
                while(link!=0xffffff){
                    if(link<(uintptr_t)packets.bytes || link+sizeof(RenderGpuTag)>(uintptr_t)packets.bytes+g_DrawPacketOffset ||
                       link%_Alignof(RenderGpuTag) || ++count>512)PAIRED_FAIL();
                    ++linked;link=((RenderGpuTag *)link)->address;
                }
            }
            for(unsigned i=0;i<sizeof(packets.guard);++i)if(packets.guard[i]!=0xa5)PAIRED_FAIL();
            if(object.prefix.reserved00==4)break;
        }
        if(frame==640 || seen!=0x1b || !drawn || !linked || !g_RoomPairedGlowPacket__paired){
            fprintf(stderr,"Paired glow PM draw%u frame%u seen%x drawn%u linked%u pc%d\n",draw,frame,seen,drawn,linked,object.state.pc);PAIRED_FAIL();
        }
        g_RoomPairedGlowHandlers.stop((PmSlotHeader *)&object);g_RoomPairedGlowHandlers.unload();
        if(action!=4 || !(player_core.flags&0x4000) || !(core.core_flags&0x80000000u))PAIRED_FAIL();
    }
    ok=1;
done:
    D_800B0E38=old_buffers;g_ActiveDrawSlot=old_slot;g_DrawPacketOffset=old_offset;g_GeomScroll.words.viewMatrix=old_view;
    g_GameState=old_game;g_PlayerEntity=old_player;*g_RoomFloorY=old_floor;
    g_FieldFxScriptState=old_state;g_FieldFxScriptObject=old_object;g_FieldFxSlotTable=old_slots;g_FieldFxScriptData=old_data;g_FieldFxCommandTable=old_commands;srand(old_random);
    if(ok)puts("Original paired glow PM: complete authored program, four authored types plus unused type2 no-ops, real room model, both draw slots, natural expiry and packet/OT guards passed");
    return ok;
#undef PAIRED_FAIL
}

static int original_paired_glow_assets(PeDisc *disc,PeDiscFile *exe)
{
    u8 header[0x800];u32 base;PeDiscFile archive;SceneRoomSectorRange ref;
    if(!pe_disc_read(disc,exe,0,header,sizeof(header)) || !pe_disc_find(disc,"PE.IMG",&archive))return 0;
    memcpy(&base,header+0x18,4);const unsigned ids[]={107,114};u8 module[18240],reference[0x35a0];
    if(!pe_disc_read(disc,exe,0x800ull+0x80093378u-base+58*8,&ref,sizeof(ref)) ||
       !pe_disc_read(disc,&archive,(u64)(ref.start+ref.scratchSectors+ref.textureSectors)*2048+8,reference,sizeof(reference)))return 0;
    for(unsigned r=0;r<2;++r){
        SceneRoomSectorRange range;if(!pe_disc_read(disc,exe,0x800ull+0x80093378u-base+(ids[r]-1)*8,&range,sizeof(range)))return 0;
        size_t bytes=(size_t)range.roomSectors*2048;SceneAssetView *room=malloc(bytes);if(!room)return 0;
        int valid=bytes>=8+sizeof(module) && pe_disc_read(disc,&archive,(u64)(range.start+range.scratchSectors+range.textureSectors)*2048,room,bytes);
        if(valid){
            SceneRoomDirectory *dir=SceneAsset_ResolveOffset(room,room->header.directoryOffset);
            SceneRoomRecord *records=SceneAsset_ResolveOffset(room,dir->scripts&0x3fffff);unsigned mask=0;
            valid=dir->scripts>>22==2;
            for(unsigned i=0;valid && i<2;++i){
                valid=(records[i].source.offsetAndSlot&0xffffff)==8 && records[i].reserved[0]==0x40 && records[i].reserved[1]==0x47;
                if(records[i].source.bytes.slot==35 && records[i].u.handlerAddress==0x801935dc)mask|=1;
                else if(records[i].source.bytes.slot==(r?72:75) && records[i].u.handlerAddress==(r?0x801936ccu:0x801936b0u))mask|=2;
                else valid=0;
            }
            valid=valid && mask==3;
            if(!r)memcpy(module,(u8 *)room+8,sizeof(module));else valid=valid && !memcmp(module,(u8 *)room+8,sizeof(module));
            uint64_t hash=UINT64_C(14695981039346656037);
            for(unsigned i=0;i<sizeof(module);++i)hash=(hash^module[i])*UINT64_C(1099511628211);
            valid=valid && hash==UINT64_C(0x69dfe82737160a0a);
            const u32 init[]={0x8018f394,0x8018f528,0x8018facc,0x8018fae4,0x8018fd2c,0xffffffff};
            const u32 draw[]={0x8018f4a0,0x8018f6d8,0x8018fad4,0x8018fb04,0x8018feac,0xffffffff};
            const u32 update[]={0x8018f4a8,0x8018f9d8,0x8018fadc,0x8018fcc8,0x80190178,0xffffffff};
            const u32 methods[]={0x8018f38c,0x8018f1d4,0x8018f200,0x8018f22c,0x8018f274,0x8018f300,0x8018f384};
            const u16 sizes[]={48,320,300,20,320,65535};
            valid=valid && !memcmp(module+0x4598,init,sizeof(init)) && !memcmp(module+0x45b0,draw,sizeof(draw)) &&
                !memcmp(module+0x45c8,update,sizeof(update)) && !memcmp(module+0x45e0,sizes,sizeof(sizes)) &&
                !memcmp(module+0x45ec,methods,sizeof(methods)) && !memcmp(module+0x4608,g_RoomScript__paired,104) &&
                !memcmp(module+0x4670,g_RoomPairedGlowColorTable__paired,24);
            const u16 low[]={0x218,0x408,0x624,0xf80,0x11ac,0x1364,0x1a24,0x1b5c,0x1b98,0x1d6c,
                0x2030,0x2550,0x2714,0x2758,0x2884,0x2c24,0x3128,0x31cc,0x3230,0x3288};
            const u16 tables[]={0x2a4,0xff0,0x1ab4,0x25cc};
            for(unsigned o=0x1dc;valid && o<0x34f8;o+=4){
                u32 expected,actual;memcpy(&expected,reference+o,4);memcpy(&actual,module+o+0x10a0,4);u32 target=(expected&0x3ffffff)<<2;
                if((expected>>26==2 || expected>>26==3) && target>=0x18f1cc && target<0x1924e8)expected+=0x10a0/4;
                for(unsigned j=0;j<20;++j)if(o==low[j])expected=(expected&0xffff0000)|(u16)(expected+0x10a0);
                for(unsigned j=0;j<4;++j)if(o==tables[j])expected=(expected&0xffff0000)|(u16)(expected+8);
                valid=expected==actual;
            }
            for(unsigned o=4;valid && o<0x1dc;o+=4){
                u32 expected,actual;memcpy(&expected,reference+o,4);memcpy(&actual,module+o+8,4);
                if(expected>=0x8018f1cc && expected<0x801924e8)expected+=0x10a0;
                valid=expected==actual;
            }
            for(unsigned i=0;valid && i<42;++i){u32 expected,actual;memcpy(&expected,reference+0x34f8+i*4,4);memcpy(&actual,module+0x4688+i*4,4);valid=actual==expected+0x10a0;}
        }
        if(!valid)fprintf(stderr,"Paired glow asset comparison failed for M%03u\n",ids[r]);
        if(valid && !r){valid=authored_paired_glow_cycle(room);if(!valid)fprintf(stderr,"Paired glow authored PM failed\n");}
        free(room);if(!valid)return 0;
    }
    /* M111/M118 contain the whole paired module without the 53-function actor
     * library. Compare every instruction and every authored table after the
     * code, data and BSS relocations, not just the exported descriptor. */
    const unsigned paired_only[]={111,118,122};
    const u16 data_low[]={0x20,0x88,0xd4,0xe8,0xf0,0x750,0xf24};
    const u16 bss_low[]={0x26c,0x278,0x284,0x28c,0x294,0x29c,0x2a4,0x2ac,0x2bc,
        0x9fc,0xa8c,0xaa4,0xab4,0xac8,0xad8};
    for(unsigned r=0;r<3;++r){
        SceneRoomSectorRange range;
        if(!pe_disc_read(disc,exe,0x800ull+0x80093378u-base+(paired_only[r]-1)*8,&range,sizeof(range)))return 0;
        size_t bytes=(size_t)range.roomSectors*2048;SceneAssetView *room=malloc(bytes);if(!room)return 0;
        int valid=bytes>=8+4520 && pe_disc_read(disc,&archive,(u64)(range.start+range.scratchSectors+range.textureSectors)*2048,room,bytes);
        if(valid){
            const unsigned code_delta=r==2?0x1c4:0x1d8,data_delta=r==2?0x26ac:0x34f4,bss_delta=r==2?0x2678:0x3598;
            const unsigned shift=r==2?0x14:0,library_bytes=r==2?8392:4520;
            const u8 *candidate=(const u8 *)room+8;
            SceneRoomDirectory *dir=SceneAsset_ResolveOffset(room,room->header.directoryOffset);
            SceneRoomRecord *record=SceneAsset_ResolveOffset(room,dir->scripts&0x3fffff);
            valid=dir->scripts>>22==(r==2?2:1);
            unsigned mask=0;
            for(unsigned i=0;valid && i<(r==2?2:1);++i){
                valid=(record[i].source.offsetAndSlot&0xffffff)==8 && record[i].reserved[0]==(r==2?0xc8:0xa8) && record[i].reserved[1]==(r==2?0x20:0x11);
                if(record[i].source.bytes.slot==35 && record[i].u.handlerAddress==(r==2?0x80190f30u:0x801900e8u))mask|=1;
                else if(r==2 && record[i].source.bytes.slot==136 && record[i].u.handlerAddress==0x80190fecu)mask|=2;
                else valid=0;
            }
            uint64_t hash=UINT64_C(14695981039346656037);
            for(unsigned i=0;i<library_bytes;++i)hash=(hash^candidate[i])*UINT64_C(1099511628211);
            valid=valid && mask==(r==2?3:1) && hash==(r==2?UINT64_C(0x865d23b4d2d74ff5):UINT64_C(0xca3ab00863c76dc9));
            const u32 seed[]={0x400,0};valid=valid && !memcmp(candidate+4,seed,sizeof(seed));
            for(unsigned o=0xc+shift;valid && o<0x10a4+shift;o+=4){
                u32 expected,actual;memcpy(&expected,module+o+code_delta,4);memcpy(&actual,candidate+o,4);
                u32 target=(expected&0x3ffffff)<<2;
                if((expected>>26==2 || expected>>26==3) && target>=0x18eff0 && target<0x19026c)expected-=code_delta/4;
                for(unsigned j=0;j<7;++j)if(o==data_low[j]+shift)expected=(expected&0xffff0000)|(u16)(expected-data_delta);
                for(unsigned j=0;j<15;++j)if(o==bss_low[j]+shift)expected=(expected&0xffff0000)|(u16)(expected-bss_delta);
                valid=expected==actual;
            }
            for(unsigned o=0x4598;valid && o<0x4608;o+=4){
                u32 expected,actual;memcpy(&expected,module+o,4);memcpy(&actual,candidate+o-data_delta,4);
                if(o<0x45e0 || o>=0x45ec)if(expected!=0xffffffff)expected-=code_delta;
                valid=expected==actual;
            }
            valid=valid && !memcmp(module+0x4608,candidate+0x4608-data_delta,128);
        }
        if(!valid)fprintf(stderr,"Paired-only module comparison failed for M%03u\n",paired_only[r]);
        if(valid)valid=authored_paired_glow_cycle(room);
        free(room);if(!valid)return 0;
    }
    puts("Original M107/M114, M111/M118 and M122: complete modules, descriptors, all actor and paired functions/tables, script/sizes/ramp and real-room authored PM passed");return 1;
}

static u64 m349_hash_bytes(u64 hash,const void *data,size_t count)
{
    const u8 *bytes=data;for(size_t i=0;i<count;++i)hash=(hash^bytes[i])*UINT64_C(1099511628211);
    return hash;
}

static int original_m256_states(void)
{
    FieldActor *old_player=g_PlayerEntity,player={0};FieldFxTaskContext *old_context=g_FieldFxTaskContext,context={0};
    FieldFxTaskSlot *old_task=g_FieldFxActiveTaskSlot;PolygonVertex *old_polygon=g_FloorPolygonVertices;u16 old_count=g_FloorPolygonVertexCount;
    PolygonVertex polygon[]={{0,-1000,0,-1000},{0,1000,0,-1000},{0,1000,0,1000},{0,-1000,0,1000}};
    RoomFloorY old_floor=*g_RoomFloorY;u32 old_rng=Psyz_RandState();
    struct {_Alignas(void *) char data[2048];u8 guard[16];} pool;FieldFxTaskSlot task={.end=pool.data};
    g_FieldFxActiveTaskSlot=&task;g_FieldFxTaskContext=&context;player.pos_x=player.pos_z=10000*65536;g_PlayerEntity=&player;
    g_FloorPolygonVertices=polygon;g_FloorPolygonVertexCount=4;
    const FieldFxTaskCallback callbacks[]={RoomM256_SwirlSparkCallback,RoomM256_ScatterDropCallback,RoomM256_ModelPulseParticleCallback,RoomM256_TrailCallback};
    const int modes[]={0,1,3},timers[]={0,1,7,15,31,47,63,65535},velocities[]={-32768,-1,32767},floors[]={-100,0,100};const u32 seeds[]={0,0x12345678};
    u64 hash=UINT64_C(14695981039346656037);unsigned cases=0;int ok=1;
    for(unsigned fn=0;fn<4;++fn)for(unsigned mode=0;mode<3;++mode)for(unsigned state=fn==3?2:0;state<(fn==0?2:fn==1?5:fn==2?1:6);++state)
    for(unsigned timer=0;timer<8;++timer)for(unsigned velocity=0;velocity<3;++velocity)for(unsigned floor=0;floor<3;++floor)
    for(unsigned seed=0;seed<2;++seed)for(unsigned full=0;full<(fn==1?2:1);++full){
        memset(&pool,0xa5,sizeof(pool));FieldFx_InitCallbackList(pool.data,20,55,RoomM256_ScatterDropCallback);FieldFxCallbackList *list=(void *)pool.data;
        if(full)for(unsigned i=0;i<55;++i)((FieldFxCallbackListEntry *)(list->entries+i*list->stride))->active=1;
        _Alignas(void *) u8 data[40];memset(data,0xa5,sizeof(data));
        if(fn==2){u16 words[]={timers[timer],65530,(u16)velocities[velocity],8192};memcpy(data+8,words,8);}
        else if(fn==3){u16 words[]={123,(u16)-77,456,0,(u16)velocities[velocity],(u16)velocities[velocity],(u16)velocities[velocity],8192,77,10,state,timers[timer]};memcpy(data+8,words,24);}
        else {u16 words[]={123,(u16)-77,456,65530,(u16)velocities[velocity],(u16)velocities[velocity],(u16)velocities[velocity],8192,state,timers[timer]};memcpy(data+8,words,20);}
        g_RoomFloorY->y=floors[floor];Psyz_Srand(seeds[seed]);u32 result=callbacks[fn](modes[mode],data+8,0),rng=Psyz_RandState();
        hash=m349_hash_bytes(hash,&result,4);hash=m349_hash_bytes(hash,data,sizeof(data));hash=m349_hash_bytes(hash,&rng,4);
        for(unsigned i=0;i<55;++i){FieldFxCallbackListEntry *entry=(void *)(list->entries+i*list->stride);hash=m349_hash_bytes(hash,entry,4);hash=m349_hash_bytes(hash,entry->state,20);}
        for(unsigned i=0;i<16;++i)if(pool.guard[i]!=0xa5)ok=0;++cases;
    }
    g_PlayerEntity=old_player;g_FieldFxTaskContext=old_context;g_FieldFxActiveTaskSlot=old_task;g_FloorPolygonVertices=old_polygon;g_FloorPolygonVertexCount=old_count;*g_RoomFloorY=old_floor;Psyz_Srand(old_rng);
    if(!ok || cases!=7344 || hash!=UINT64_C(0xc1db2c472afca4ed)){fprintf(stderr,"M256 CPU states mismatch %u/%016llx guards%d\n",cases,(unsigned long long)hash,ok);return 0;}
    puts("Original M256:7344 particle states, damped splashes, swirl, pulse and numeric trail states/pool/RNG match USA MIPS");return 1;
}

static int original_m256_swing(void)
{
    const int angles[]={-4095,-1024,0,1024,4095},durations[]={1,2,7};
    u64 hash=UINT64_C(14695981039346656037);unsigned cases=0,updates=0;
    for(unsigned a=0;a<5;++a)for(unsigned b=0;b<5;++b)for(unsigned d0=0;d0<3;++d0)for(unsigned d1=0;d1<3;++d1)for(unsigned d2=0;d2<3;++d2){
        RoomM256Swing object;memset(&object,0xa5,sizeof(object));FieldActor actor={0};u32 signal=0;
        object.status=2;object.actor=&actor;actor.action=64;actor.anim.fixed=80*65536;
        PmSlotHeader *slot=(void *)&object;g_RoomM256SwingClass.init();g_RoomM256SwingClass.initialize(slot);
        if(g_RoomM256SwingClass.send(slot,1,0x100,angles[a],angles[b],0) ||
           g_RoomM256SwingClass.send(slot,1,0x101,durations[d0],durations[d1],durations[d2]) ||
           g_RoomM256SwingClass.send(slot,1,0x19,(intptr_t)&signal,0,0) || object.cycle.signal!=(void *)&signal || signal!=1)return 0;
        for(unsigned frame=0;frame<(unsigned)(durations[d0]+durations[d1]+durations[d2]+4);++frame){
            actor.mode=frame?11:0;u32 result=g_RoomM256SwingClass.execute(slot),status=object.status,callback=object.cycle.callback==RoomM256_SwingStep;
            if(object.cycle.callback!=RoomM256_SwingAwaitMode && !callback)return 0;
            hash=m349_hash_bytes(hash,&result,4);hash=m349_hash_bytes(hash,&status,4);hash=m349_hash_bytes(hash,&callback,4);
            hash=m349_hash_bytes(hash,&signal,4);hash=m349_hash_bytes(hash,&object.cycle.started,1);hash=m349_hash_bytes(hash,&object.cycle.segment,2);
            hash=m349_hash_bytes(hash,object.cycle.angles,22);hash=m349_hash_bytes(hash,&actor.anim.fixed,4);hash=m349_hash_bytes(hash,&actor.render_object.rotation_overrides[0],8);
            ++updates;if(object.status==4)break;
        }
        if(object.status!=4 || signal || g_RoomM256SwingClass.start(slot) || g_RoomM256SwingClass.stop(slot))return 0;
        g_RoomM256SwingClass.unload();++cases;
    }
    if(cases!=675 || updates!=8100 || hash!=UINT64_C(0x3763c05cb2343a37)){fprintf(stderr,"M256 swing CPU mismatch %u/%u/%016llx\n",cases,updates,(unsigned long long)hash);return 0;}
    s32 saved_turn=g_RoomM256TrailTurnScale;
    const s32 values[]={INT32_MIN,-1,0,511,512,513,4096,32767,INT32_MAX};
    for(unsigned i=0;i<sizeof(values)/sizeof(*values);++i){
        if(RoomM256_ScatterConfigure(i,values[i],0,0)!=(intptr_t)&g_RoomM256ScatterConfig ||
           RoomM256_SetTrailTurnScale(i,values[i],0,0)!=(intptr_t)&g_RoomM256TrailConfig ||
           g_RoomM256TrailTurnScale!=(values[i]<512?512:values[i])){g_RoomM256TrailTurnScale=saved_turn;return 0;}
    }
    g_RoomM256TrailTurnScale=saved_turn;
    puts("Original M256 swing:675 full cycles/8100 updates, mode gate, callback context, full-width completion pointer, easing, animation reset and release match USA MIPS");return 1;
}

static int original_m256_programs(SceneAssetView *room)
{
    s32 old_scatter=g_RoomM256ScatterConfig,old_config=g_RoomM256TrailConfig,old_turn=g_RoomM256TrailTurnScale;
    void *old_trail=g_RoomM256TrailModel,*old_model=g_RoomM256PulseModel;
    RoomM256TrailHistory old_history[3];memcpy(old_history,g_RoomM256TrailHistory,sizeof(old_history));
    s16 old_brightness=g_RoomM256PulseBrightness;GteShortVector old_angles=g_RoomM256PulseAngles;RoomModelPulseAnchor old_center=g_RoomM256PulseCenter;
    PmTables old_tables=g_PmTables;Pe1GameState old_game=g_GameState;u32 old_flags=g_GameStateFlags;
    FieldFxTaskProgram *old_programs[]={g_FieldFxTaskPrograms[46],g_FieldFxTaskPrograms[47]};
    FieldFxTaskProgram *programs[]={&g_RoomM256ScatterEffects,&g_RoomM256TrailEffects};
    int old_arg=g_RoomScriptArg;FieldActor *old_player=g_PlayerEntity;
    FieldFxObjectPrefix *old_owner=g_FieldFxOwner;FieldFxTaskContext *old_context=g_FieldFxTaskContext;
    FieldFxTaskSlot *old_task=g_FieldFxActiveTaskSlot;int old_age=g_FieldFxCurrentAge;
    RenderBufferPrefix old_buffers=D_800B0E38;int old_draw=g_ActiveDrawSlot,old_offset=g_DrawPacketOffset;
    RenderEffectParameters old_params=g_FieldFxSpriteParams;
    GteMatrix *old_view=g_GeomScroll.words.viewMatrix;int *old_projection=g_GeomScroll.words.projectionDistance;
    u32 old_rng=Psyz_RandState();RoomFloorY old_floor=*g_RoomFloorY;
    PolygonVertex *old_polygon=g_FloorPolygonVertices;u16 old_vertices=g_FloorPolygonVertexCount;
    PolygonVertex polygon[]={{0,-2000,0,-2000},{0,2000,0,-2000},{0,2000,0,2000},{0,-2000,0,2000}};
    AkaoQueueEntry old_audio[16];memcpy(old_audio,g_AkaoMessageQueue,sizeof(old_audio));int old_count=g_AkaoMessageQueueCount;
    static RenderGpuTag ordering[4096];static struct {_Alignas(void *) u8 data[1048576];u8 guard[32];} packets;
    GteMatrix view={.m={{4096,0,0},{0,4096,0},{0,0,4096}}};int projection=256,ok=1;unsigned cycles=0;
    g_GeomScroll.words.viewMatrix=&view;g_GeomScroll.words.projectionDistance=&projection;
    g_GameState.loaded_scene_archive=(void *)room;g_GameStateFlags=0x80;g_GameState.flags&=~8u;g_GameState.tim_load_state=0;g_RoomFloorY->y=0;
    g_FloorPolygonVertices=polygon;g_FloorPolygonVertexCount=4;
    for(unsigned i=0;i<2;++i)g_FieldFxTaskPrograms[46+i]=programs[i];
    FieldFx_InitTexturePageTable();Psyz_GteCtrlWrite(26,256);Psyz_GteCtrlWrite(29,1365);Psyz_GteCtrlWrite(30,1024);gte_SetGeomOffset(160,112);
    for(unsigned program=0;ok && program<2;++program)for(unsigned variant=0;ok && variant<2;++variant)for(unsigned draw=0;ok && draw<2;++draw){
        struct {PmPrimarySlot slots[11];u8 guard[32];} bank={0};memset(bank.guard,0xa5,32);
        g_RoomM256ScatterConfig=g_RoomM256TrailConfig=g_RoomM256TrailTurnScale=0;
        g_RoomM256TrailModel=g_RoomM256PulseModel=NULL;memset(g_RoomM256TrailHistory,0,sizeof(g_RoomM256TrailHistory));
        g_RoomM256PulseBrightness=0;g_RoomM256PulseAngles=(GteShortVector){0};g_RoomM256PulseCenter=(RoomModelPulseAnchor){0};
        FieldActor actor={0},player={0};FieldActorState state={0},player_state={0};u8 action=1;
        RenderMatrix matrices[5]={0};actor.render_object.matrices=matrices;actor.state=&state;state.action_state=&action;
        actor.mode=11;actor.action=64;actor.pos_y=-200*65536;actor.pos_z=1024*65536;player.state=&player_state;player.pos_z=variant?964*65536:3000*65536;player.pos_x=variant?0:3000*65536;
        for(unsigned j=0;j<5;++j){for(unsigned k=0;k<3;++k)matrices[j].rotation[k][k]=4096;matrices[j].translation[1]=-200;matrices[j].translation[2]=1024;}
        g_PlayerEntity=&player;g_PmTables=(PmTables){g_PmCmdHandlerArray,bank.slots,NULL};srand(variant+1);
        if(Scene_LoadRoomAssets(131+program,&actor)!=0 || Pm_SendCmd(0,0,0,60,0,0)){ok=0;break;}
        unsigned frame=0,drawn=0,children=0,seen=0,handshake=0;
        for(;frame<360;++frame){
            for(unsigned j=0;j<4096;++j)ordering[j]=(RenderGpuTag){.address=0xffffff};
            memset(packets.guard,0xa5,32);g_ActiveDrawSlot=draw;g_DrawPacketOffset=0;
            D_800B0E38.ordering[draw]=ordering;D_800B0E38.packets[draw]=(char *)packets.data;
            if(Pm_Start(0) || Pm_Exec(0)){ok=0;break;}
            handshake|=action==2;
            if(g_AkaoMessageQueueCount<0 || g_AkaoMessageQueueCount>16){ok=0;break;}
            g_AkaoMessageQueueCount=0;drawn+=g_DrawPacketOffset!=0;FieldFxTaskContext *ctx=&bank.slots[0].task.tasks;
            for(unsigned k=0;k<8;++k)if(ctx->slots[k].id!=0xffff){
                unsigned id=ctx->slots[k].id;if(id>1 || !ctx->slots[k].end){ok=0;break;}seen|=1u<<id;
                FieldFxCallbackList *list=(void *)ctx->slots[k].end;
                if(list->count!=((unsigned[2][2]){{50,32},{32,16}})[program][id] || list->callback!=((FieldFxTaskCallback[2][2]){{RoomM256_ScatterDropCallback,RoomM256_SwirlSparkCallback},{RoomM256_TrailCallback,RoomM256_ModelPulseParticleCallback}})[program][id]){ok=0;break;}
                for(unsigned j=0;j<(unsigned)list->count;++j)children+=((FieldFxCallbackListEntry *)(list->entries+j*list->stride))->active!=0;
            }
            if(ctx->used>sizeof(ctx->arena) || g_DrawPacketOffset<0 || g_DrawPacketOffset>sizeof(packets.data)){ok=0;break;}
            for(unsigned j=0;j<4096 && ok;++j){uintptr_t link=ordering[j].address;unsigned count=0;
                while(link && link!=0xffffff){if(link<(uintptr_t)packets.data || link+sizeof(RenderGpuTag)>(uintptr_t)packets.data+g_DrawPacketOffset || ++count>8192){ok=0;break;}link=((RenderGpuTag *)link)->address;}}
            for(unsigned j=0;j<32;++j)if(bank.guard[j]!=0xa5 || packets.guard[j]!=0xa5)ok=0;
            if(!ok || !bank.slots[0].header.state)break;
        }
        int hit=(player_state.flags&0x4000)!=0;
        if(hit!=(int)variant || !handshake || action!=4)ok=0;
        if(!ok || frame==360 || !drawn || !children || seen!=3 || bank.slots[0].header.state || bank.slots[0].task.tasks.count){
            fprintf(stderr,"M256 PM %u/%u/%u frames%u drawn%u children%u state%u hit%d action%u\n",program,variant,draw,frame,drawn,children,bank.slots[0].header.state,hit,action);ok=0;}
        if(ok){++cycles;fprintf(stderr,"M256 PM %u/%u/%u frames%u drawn%u children%u hit%d passed\n",program,variant,draw,frame,drawn,children,hit);}
    }
    g_RoomM256ScatterConfig=old_scatter;g_RoomM256TrailConfig=old_config;g_RoomM256TrailTurnScale=old_turn;
    g_RoomM256TrailModel=old_trail;g_RoomM256PulseModel=old_model;memcpy(g_RoomM256TrailHistory,old_history,sizeof(old_history));
    g_RoomM256PulseBrightness=old_brightness;g_RoomM256PulseAngles=old_angles;g_RoomM256PulseCenter=old_center;
    g_PmTables=old_tables;g_GameState=old_game;g_GameStateFlags=old_flags;g_PlayerEntity=old_player;g_RoomScriptArg=old_arg;
    for(unsigned i=0;i<2;++i)g_FieldFxTaskPrograms[46+i]=old_programs[i];
    g_FieldFxOwner=old_owner;g_FieldFxTaskContext=old_context;g_FieldFxActiveTaskSlot=old_task;g_FieldFxCurrentAge=old_age;g_FieldFxSpriteParams=old_params;
    D_800B0E38=old_buffers;g_ActiveDrawSlot=old_draw;g_DrawPacketOffset=old_offset;g_GeomScroll.words.viewMatrix=old_view;g_GeomScroll.words.projectionDistance=old_projection;
    g_FloorPolygonVertices=old_polygon;g_FloorPolygonVertexCount=old_vertices;
    srand(old_rng);*g_RoomFloorY=old_floor;memcpy(g_AkaoMessageQueue,old_audio,sizeof(old_audio));g_AkaoMessageQueueCount=old_count;
    if(ok)printf("Original M256 PM: %u complete authored cycles, two programs, both draw slots, hit/no-hit, rendering, natural expiry and pool/packet/OT guards passed\n",cycles);
    return ok;
}

static int original_m318_numeric(void)
{
    FieldFxTaskCallback callbacks[]={RoomM318_EmberBurstParticleCallback,RoomM318_RisingColumnParticleCallback,
        RoomM318_FlashBurstParticleCallback,RoomM318_EmberFountainParticleCallback,RoomM318_EmberSpiralParticleCallback,
        RoomM318_FanSweepTrailCallback,RoomM318_SpiralDropSparkCallback,RoomM318_ArcingSparkCallback,
        RoomM318_ShakeBurstParticleCallback,RoomM318_CrossFlashParticleCallback,RoomM318_TwinRibbonPulseCallback};
    const int modes[]={0,1,3},timers[]={0,1,7,15,23,31,32767,65535},velocities[]={-32768,-1,32767},floors[]={-100,0,100};
    const u32 seeds[]={0,0x12345678};u64 hash=UINT64_C(14695981039346656037);unsigned count=0;
    int old_age=g_FieldFxCurrentAge;RoomFloorY old_floor=*g_RoomFloorY;u32 old_rng=Psyz_RandState();
    for(unsigned cb=0;cb<11;++cb)for(unsigned mode=0;mode<3;++mode)for(unsigned state=0;state<3;++state)
    for(unsigned timer=0;timer<8;++timer)for(unsigned velocity=0;velocity<3;++velocity)for(unsigned floor=0;floor<3;++floor)for(unsigned seed=0;seed<2;++seed){
        _Alignas(void *) u8 data[36];memset(data,0xa5,sizeof(data));
        u16 words[]={123,(u16)-77,456,65530,velocities[velocity],velocities[velocity],velocities[velocity],8192,state,timers[timer]};memcpy(data+8,words,20);
        g_RoomFloorY->y=floors[floor];g_FieldFxCurrentAge=timers[timer];srand(seeds[seed]);
        u32 result=callbacks[cb](modes[mode],data+8,0),rng=Psyz_RandState();
        hash=m349_hash_bytes(hash,&result,4);hash=m349_hash_bytes(hash,data,36);hash=m349_hash_bytes(hash,&rng,4);++count;
    }
    g_FieldFxCurrentAge=old_age;*g_RoomFloorY=old_floor;srand(old_rng);
    if(count!=14256 || hash!=UINT64_C(0x6825cef1d3ef1dbd)){fprintf(stderr,"M318 numeric mismatch %u/%016llx\n",count,(unsigned long long)hash);return 0;}
    puts("Original M318:14256 states of eleven callbacks, complete payload guards and RNG match USA MIPS");return 1;
}

static int original_m318_pool_states(void)
{
    FieldFxTaskCallback callbacks[]={RoomM318_FanSweepSpark,RoomM318_ShakeSweepParticleCallback};
    const int modes[]={0,1,3},timers[]={0,1,7,23,35,36,32767,65535},velocities[]={-32768,-1,32767},floors[]={-100,0,100};
    const u32 seeds[]={0,0x12345678};u64 hash=UINT64_C(14695981039346656037);unsigned count=0;
    int old_age=g_FieldFxCurrentAge;RoomFloorY old_floor=*g_RoomFloorY;u32 old_rng=Psyz_RandState();
    FieldFxTaskContext *old_context=g_FieldFxTaskContext;FieldFxTaskSlot *old_task=g_FieldFxActiveTaskSlot;
    void *old_pool=g_RoomM318FanSweepSparkPool;GteShortVector old_fan=g_RoomM318FanSweepAnchor,old_sweep=g_RoomM318ShakeSweepAnchor;
    g_FieldFxTaskContext=NULL;g_RoomM318FanSweepAnchor=g_RoomM318ShakeSweepAnchor=(GteShortVector){123,-200,1024,0};
    for(unsigned cb=0;cb<2;++cb)for(unsigned mode=0;mode<3;++mode)for(unsigned state=0;state<3;++state)
    for(unsigned timer=0;timer<8;++timer)for(unsigned velocity=0;velocity<3;++velocity)for(unsigned floor=0;floor<3;++floor)
    for(unsigned seed=0;seed<2;++seed)for(unsigned full=0;full<2;++full){
        _Alignas(void *) u8 pool[2048],data[32];memset(pool,0xa5,sizeof(pool));memset(data,0xa5,sizeof(data));
        FieldFxTaskSlot task={.end=(char *)pool};g_FieldFxActiveTaskSlot=&task;g_RoomM318FanSweepSparkPool=pool;
        FieldFx_InitCallbackList((char *)pool,16,24,callbacks[cb]);FieldFxCallbackList *list=(void *)pool;
        if(full)for(unsigned i=0;i<24;++i)((FieldFxCallbackListEntry *)(list->entries+i*list->stride))->active=1;
        u16 fan[]={state,8192,timers[timer],123,(u16)-77,456,velocities[velocity],0xa5a5};
        u16 sweep[]={123,(u16)-77,456,state,velocities[velocity],8192,timers[timer],velocities[velocity]};
        memcpy(data+8,cb?sweep:fan,16);g_RoomFloorY->y=floors[floor];g_FieldFxCurrentAge=timers[timer];srand(seeds[seed]);
        u32 result=callbacks[cb](modes[mode],data+8,0),rng=Psyz_RandState();
        hash=m349_hash_bytes(hash,&result,4);hash=m349_hash_bytes(hash,data,32);hash=m349_hash_bytes(hash,&rng,4);
        for(unsigned i=0;i<24;++i){FieldFxCallbackListEntry *entry=(void *)(list->entries+i*list->stride);
            hash=m349_hash_bytes(hash,entry,4);hash=m349_hash_bytes(hash,entry->state,16);}
        ++count;
    }
    g_FieldFxCurrentAge=old_age;*g_RoomFloorY=old_floor;srand(old_rng);g_FieldFxTaskContext=old_context;g_FieldFxActiveTaskSlot=old_task;
    g_RoomM318FanSweepSparkPool=old_pool;g_RoomM318FanSweepAnchor=old_fan;g_RoomM318ShakeSweepAnchor=old_sweep;
    if(count!=5184 || hash!=UINT64_C(0x0a6f03b306e499ce)){fprintf(stderr,"M318 pool mismatch %u/%016llx\n",count,(unsigned long long)hash);return 0;}
    puts("Original M318:5184 FanSweepSpark/ShakeSweepParticle states, full/free child pools and RNG match USA MIPS");return 1;
}

static int original_m318_programs(PeDisc *disc)
{
    PeDiscFile archive;
    if(!pe_disc_find(disc,"PE.IMG",&archive))return 0;
    size_t common_bytes=(g_PeImageSectorTable[4]-g_PeImageSectorTable[3])*2048u;
    u8 *common=malloc(common_bytes);
    if(!common || !pe_disc_read(disc,&archive,(u64)g_PeImageSectorTable[3]*2048,common,common_bytes))return 0;
    D_800F32D8=Asset_FindTable08ByU32Key(common,0x8AB5B684);
    if(!D_800F32D8){free(common);return 0;}
    FieldFxTaskProgram *programs[]={&g_RoomM318FanSweepEffects,&g_RoomM318ShakeSweepEffects,
        &g_RoomM318ShakeBurstEffects,&g_RoomM318EmberBurstEffects,&g_RoomM318RisingColumnEffects,
        &g_RoomM318EmberFountainEffects,&g_RoomM318FlashEffects,&g_RoomM318TwinRibbonEffects};
    static RenderGpuTag ordering[4096];
    static struct {_Alignas(void *) u8 data[1048576];u8 guard[32];} packets;
    GteMatrix view={.m={{4096,0,0},{0,4096,0},{0,0,4096}}};int projection=256;
    PolygonVertex polygon[]={{0,-4000,0,-4000},{0,4000,0,-4000},{0,4000,0,4000},{0,-4000,0,4000}};
    g_GeomScroll.words.viewMatrix=&view;g_GeomScroll.words.projectionDistance=&projection;
    g_GameStateFlags=0x80;g_GameState.flags&=~8u;g_GameState.tim_load_state=0;
    g_RoomFloorY->y=0;g_FloorPolygonVertices=polygon;g_FloorPolygonVertexCount=4;
    FieldFx_InitTexturePageTable();Psyz_GteCtrlWrite(26,256);Psyz_GteCtrlWrite(29,1365);Psyz_GteCtrlWrite(30,1024);gte_SetGeomOffset(160,112);
    int ok=1;unsigned cycles=0;
    for(unsigned program=0;ok && program<8;++program)for(unsigned variant=0;ok && variant<(program==6?4u:2u);++variant)for(unsigned draw=0;ok && draw<2;++draw){
        struct {PmPrimarySlot slots[11];u8 guard[32];} bank={0};memset(bank.guard,0xa5,32);
        FieldActor actor={0},player={0};FieldActorState state={0};Combatant player_state={0};u8 action=1;
        RenderMatrix matrices[22]={0};actor.render_object.matrices=matrices;actor.state=&state;state.action_state=&action;
        actor.type_id=6;actor.sub_id=0;state.control10.command_value=10;state.core_flags=0x01000000;
        actor.mode=11;actor.action=64;actor.pos_y=-200*65536;actor.pos_z=1024*65536;
        RenderMatrix player_matrix={.rotation={{4096,0,0},{0,4096,0},{0,0,4096}}};
        player.render_object.matrices=&player_matrix;
        player.state=(void *)&player_state;player.pos_z=variant?1024*65536:3000*65536;player.pos_x=variant?0:3000*65536;
        player_matrix.translation[0]=player.pos_x/65536;player_matrix.translation[2]=player.pos_z/65536;
        /* Isolated actor fixture: halt/resume only reads the frame count. */
        RenderAnimationDataHeader player_animation={.frame_count=10};
        for(unsigned j=0;j<48;++j)g_GameState.bank_rows[0][j]=&player_animation;
        player_state.curHP=100;player_state.actionMode12=0;g_ActiveActor=&player_state;
        g_PlayerEntity=&player;g_FieldActorListHead=&actor;
        for(unsigned j=0;j<22;++j){for(unsigned k=0;k<3;++k)matrices[j].rotation[k][k]=4096;matrices[j].translation[1]=-200;matrices[j].translation[2]=1024;}
        g_PmTables=(PmTables){g_PmCmdHandlerArray,bank.slots,NULL};srand(variant+1);
        g_RoomM318ScriptArg2=g_RoomM318StarFlarePhase=g_RoomScriptArg=0;
        memset(g_RoomM318RisingColumnParams,0,sizeof(g_RoomM318RisingColumnParams));
        RoomM318_SetActorParams(0,6,0,0);
        if(g_FieldFxTaskPrograms[83+program]!=programs[program] || Scene_LoadRoomAssets(168+program,&actor)!=0){ok=0;break;}
        int status;
        if(program==0 || program==3)status=Pm_SendCmd(0,0,0,6,0,0);
        else if(program==4)status=Pm_SendCmd(0,0,0,32,400,200);
        else if(program==6)status=Pm_SendCmd(0,0,0,60,0,0);
        else status=Pm_SendCmd(0,0,0,0,0,0);
        if(status){ok=0;break;}
        if(program==4 && variant)Pm_SendCmd(0,0,1,1,0,0);
        unsigned frame=0,drawn=0,children=0,seen=0;
        for(;frame<512;++frame){
            if(program==7 && frame==64)Pm_SendCmd(0,0,1,1,0,0);
            if(program==6 && variant<2 && frame==(variant?20u:40u))player_state.stateFlags|=0x10000000;
            for(unsigned j=0;j<4096;++j)ordering[j]=(RenderGpuTag){.address=0xffffff};
            memset(packets.guard,0xa5,32);g_ActiveDrawSlot=draw;g_DrawPacketOffset=0;
            D_800B0E38.ordering[draw]=ordering;D_800B0E38.packets[draw]=(char *)packets.data;
            if(Pm_Start(0) || Pm_Exec(0)){ok=0;break;}
            if(g_AkaoMessageQueueCount<0 || g_AkaoMessageQueueCount>16){ok=0;break;}
            g_AkaoMessageQueueCount=0;drawn+=g_DrawPacketOffset!=0;FieldFxTaskContext *ctx=&bank.slots[0].task.tasks;
            for(unsigned k=0;k<8;++k)if(ctx->slots[k].id!=0xffff){
                unsigned id=ctx->slots[k].id;if(id>2){ok=0;break;}seen|=1u<<id;
                if(ctx->slots[k].end){FieldFxCallbackList *list=(void *)ctx->slots[k].end;
                    for(unsigned j=0;j<(unsigned)list->count;++j)children+=((FieldFxCallbackListEntry *)(list->entries+j*list->stride))->active!=0;}
            }
            if(ctx->used>sizeof(ctx->arena) || g_DrawPacketOffset<0 || g_DrawPacketOffset>sizeof(packets.data)){ok=0;break;}
            for(unsigned j=0;j<4096 && ok;++j){uintptr_t link=ordering[j].address;unsigned count=0;
                while(link && link!=0xffffff){if(link<(uintptr_t)packets.data || link+sizeof(RenderGpuTag)>(uintptr_t)packets.data+g_DrawPacketOffset || ++count>8192){ok=0;break;}link=((RenderGpuTag *)link)->address;}}
            for(unsigned j=0;j<32;++j)if(bank.guard[j]!=0xa5 || packets.guard[j]!=0xa5)ok=0;
            if(!ok || !bank.slots[0].header.state)break;
        }
        const unsigned expected_seen[]={3,3,3,1,1,3,7,1};
        int expected_hit=program==1 || program==2 || (program==5 && variant);
        if(!!(player_state.stateFlags&0x4000)!=expected_hit || action!=((program==1 || program==2 || program==5)?4:1))ok=0;
        int missed_flash=program==6 && variant>=2;
        if(!ok || frame==512 || (missed_flash?(drawn || children):(!drawn || !children)) ||
           seen!=(missed_flash?4u:expected_seen[program]) || bank.slots[0].header.state || bank.slots[0].task.tasks.count){
            fprintf(stderr,"M318 PM %u/%u/%u frames%u drawn%u children%u seen%x state%u action%u\n",program,variant,draw,frame,drawn,children,seen,bank.slots[0].header.state,action);ok=0;}
        if(ok){++cycles;fprintf(stderr,"M318 PM %u/%u/%u frames%u drawn%u children%u hit%d action%u passed\n",program,variant,draw,frame,drawn,children,!!(player_state.stateFlags&0x4000),action);}
    }
    free(common);return ok && cycles==36;
}

static int original_m318_assets(PeDisc *disc,PeDiscFile *exe)
{
    SceneRoomSectorRange range;PeDiscFile archive;
    if(!pe_disc_find(disc,"PE.IMG",&archive) || !pe_disc_read(disc,exe,0x800ull+0x80093378u-0x80010000u+317*8,&range,8))return 0;
    size_t bytes=(size_t)range.roomSectors*2048;SceneAssetView *room=malloc(bytes);if(!room)return 0;
    int ok=bytes==378880 && pe_disc_read(disc,&archive,(u64)(range.start+range.scratchSectors+range.textureSectors)*2048,room,bytes);
    const struct {u32 address;const void *data;size_t size;} data[]={
        {0x8018f204u,&g_RoomM318EmberHaloColor,4},
        {0x8018f200u,&g_RoomM318EmberRingColor,4},
        {0x8019993cu,&g_RoomM318EmberFountainAnchor,8},
        {0x8018f1f4u,&g_RoomM318ZeroSpin,8},
        {0x8018f240u,&g_RoomM318EmberFountainParticleColor,4},
        {0x8018f1f0u,&g_RoomM318FanSweepSparkColor,4},
        {0x801998f8u,&g_RoomM318FanSweepAnchor,8},
        {0x8018f214u,&g_RoomM318TwinRibbonRoot,8},
        {0x8018f21cu,&g_RoomM318TwinRibbonTip,8},
        {0x8018f224u,&g_RoomM318TwinRibbonFork,8},
        {0x8018f22cu,&g_RoomM318TwinRibbonShed,8},
        {0x8018f234u,&g_RoomM318TwinRibbonColorA,4},
        {0x8018f238u,&g_RoomM318TwinRibbonColorB,4},
        {0x8018f23cu,&g_RoomM318TwinRibbonColorC,4},
        {0x80199904u,&g_RoomM318ShakeSweepAnchor,8},
        {0x8018f210u,&g_RoomM318FlashBurstColor,4},
        {0x80199924u,&g_RoomM318RisingColumnAnchor,8},
        {0x8018f208u,&g_RoomM318FlatTilt,8},
        {0x80199930u,&g_RoomM318CrossFlashAnchor,8},
        {0x8018f1fcu,&g_RoomM318EmberParticleColor,4},
        {0x8018f1ccu,&g_RoomM318GetMayaPosErrorFormat,36},
        {0x80199464u,&g_RoomM318FanSweepTrailColorTrack,40},
        {0x8019948cu,&g_RoomM318SpiralDropSparkColorTrack,48},
        {0x801994bcu,&g_RoomM318SpiralDropColorTrack,48},
        {0x801995bcu,&g_RoomM318StarFlareColorTrackA,32},
        {0x801995dcu,&g_RoomM318StarFlareColorTrackB,32},
        {0x801995fcu,&g_RoomM318ShakeBurstParticleColorTrack,40},
        {0x80199674u,&g_RoomM318EmberBurstParticleColorTrack,40},
        {0x801996ecu,&g_RoomM318RisingColumnParticleColorTrack,32},
        {0x80199770u,&g_RoomM318FlashBurstColorTrack,40},
        {0x80199798u,&g_RoomM318CrossFlashParticleColorTrack,40},
        {0x80199868u,&g_RoomM318EmberFountainParticleColorTrack,40},
        {0x80199890u,&g_RoomM318EmberSpiralColorTrack,40},
        {0x80199444u,&g_RoomM318FanSweepScript,32},
        {0x80199524u,&g_RoomM318ShakeSweepScript,36},
        {0x80199580u,&g_RoomM318ShakeBurstScript,56},
        {0x8019965cu,&g_RoomM318EmberBurstScript,24},
        {0x801996d4u,&g_RoomM318RisingColumnScript,24},
        {0x80199744u,&g_RoomM318FlashScript,44},
        {0x801997f8u,&g_RoomM318TwinRibbonScript,20},
        {0x80199844u,&g_RoomM318EmberFountainScript,36},
    };
    for(unsigned i=0;ok && i<sizeof(data)/sizeof(*data);++i){
        const u8 *retail=(u8 *)room+data[i].address-0x8018efe8;
        if(data[i].data==&g_RoomM318FlashScript){
            /* Deliberate timeout cleanup: preserve every other retail word. */
            u16 expected[22];memcpy(expected,retail,sizeof(expected));
            if(expected[10]!=1 || g_RoomM318FlashScript[10]!=0)ok=0;
            expected[10]=0;
            if(memcmp(expected,data[i].data,sizeof(expected)))ok=0;
        }else if(memcmp(retail,data[i].data,data[i].size))ok=0;
        if(!ok)fprintf(stderr,"M318 data mismatch %u\n",i);
    }
    if(ok){
        {
            FieldFxTaskProgram *p=&g_RoomM318FanSweepEffects;
            const u32 original[14]={0x80192a7cu,0x80192ed4u,0x0u,0x0u,0x0u,0x0u,0x0u,0x0u,0x100010u,0x0u,0x0u,0x0u,0x80199218u,0x80199444u};
            if(memcmp((u8 *)room+0x801994ecu-0x8018efe8u,original,sizeof(original)))ok=0;
            FieldFxTaskCallback callbacks[8]={RoomM318_FanSweep,RoomM318_SpiralDrop,NULL,NULL,NULL,NULL,NULL,NULL};
            const u16 sizes[8]={sizeof(RoomFanSweep),16,0,0,0,0,0,0};
            for(unsigned j=0;j<8;++j)if(p->table.callbacks[j]!=callbacks[j] || p->table.sizes[j]!=sizes[j])ok=0;
            if(p->initialize!=RoomM318_SetActorParams || p->script!=g_RoomM318FanSweepScript)ok=0;
        }
        {
            FieldFxTaskProgram *p=&g_RoomM318ShakeSweepEffects;
            const u32 original[14]={0x80193638u,0x80193b60u,0x0u,0x0u,0x0u,0x0u,0x0u,0x0u,0x8000cu,0x0u,0x0u,0x0u,0x0u,0x80199524u};
            if(memcmp((u8 *)room+0x80199548u-0x8018efe8u,original,sizeof(original)))ok=0;
            FieldFxTaskCallback callbacks[8]={RoomM318_ShakeSweep,RoomM318_ArcingSparks,NULL,NULL,NULL,NULL,NULL,NULL};
            const u16 sizes[8]={12,8,0,0,0,0,0,0};
            for(unsigned j=0;j<8;++j)if(p->table.callbacks[j]!=callbacks[j] || p->table.sizes[j]!=sizes[j])ok=0;
            if(p->initialize!=NULL || p->script!=g_RoomM318ShakeSweepScript)ok=0;
        }
        {
            FieldFxTaskProgram *p=&g_RoomM318ShakeBurstEffects;
            const u32 original[14]={0x80193d88u,0x80194164u,0x0u,0x0u,0x0u,0x0u,0x0u,0x0u,0x8000cu,0x0u,0x0u,0x0u,0x0u,0x80199580u};
            if(memcmp((u8 *)room+0x80199624u-0x8018efe8u,original,sizeof(original)))ok=0;
            FieldFxTaskCallback callbacks[8]={RoomM318_StarFlare,RoomM318_ShakeBurst,NULL,NULL,NULL,NULL,NULL,NULL};
            const u16 sizes[8]={12,8,0,0,0,0,0,0};
            for(unsigned j=0;j<8;++j)if(p->table.callbacks[j]!=callbacks[j] || p->table.sizes[j]!=sizes[j])ok=0;
            if(p->initialize!=NULL || p->script!=g_RoomM318ShakeBurstScript)ok=0;
        }
        {
            FieldFxTaskProgram *p=&g_RoomM318EmberBurstEffects;
            const u32 original[14]={0x80194aacu,0x0u,0x0u,0x0u,0x0u,0x0u,0x0u,0x0u,0x10u,0x0u,0x0u,0x0u,0x80199234u,0x8019965cu};
            if(memcmp((u8 *)room+0x8019969cu-0x8018efe8u,original,sizeof(original)))ok=0;
            FieldFxTaskCallback callbacks[8]={RoomM318_EmberBurst,NULL,NULL,NULL,NULL,NULL,NULL,NULL};
            const u16 sizes[8]={16,0,0,0,0,0,0,0};
            for(unsigned j=0;j<8;++j)if(p->table.callbacks[j]!=callbacks[j] || p->table.sizes[j]!=sizes[j])ok=0;
            if(p->initialize!=RoomM318_SetEmberBurstParams || p->script!=g_RoomM318EmberBurstScript)ok=0;
        }
        {
            FieldFxTaskProgram *p=&g_RoomM318RisingColumnEffects;
            const u32 original[14]={0x801955e4u,0x0u,0x0u,0x0u,0x0u,0x0u,0x0u,0x0u,0xcu,0x0u,0x0u,0x0u,0x80199250u,0x801996d4u};
            if(memcmp((u8 *)room+0x8019970cu-0x8018efe8u,original,sizeof(original)))ok=0;
            FieldFxTaskCallback callbacks[8]={RoomM318_RisingColumn,NULL,NULL,NULL,NULL,NULL,NULL,NULL};
            const u16 sizes[8]={12,0,0,0,0,0,0,0};
            for(unsigned j=0;j<8;++j)if(p->table.callbacks[j]!=callbacks[j] || p->table.sizes[j]!=sizes[j])ok=0;
            if(p->initialize!=RoomM318_SetRisingColumnParams || p->script!=g_RoomM318RisingColumnScript)ok=0;
        }
        {
            FieldFxTaskProgram *p=&g_RoomM318FlashEffects;
            const u32 original[14]={0x80195d20u,0x80196820u,0x801992a4u,0x0u,0x0u,0x0u,0x0u,0x0u,0x100008u,0x0u,0x0u,0x0u,0x80199294u,0x80199744u};
            if(memcmp((u8 *)room+0x801997c0u-0x8018efe8u,original,sizeof(original)))ok=0;
            FieldFxTaskCallback callbacks[8]={RoomM318_FlashBurst,RoomM318_CrossFlash,RoomM318_PlayerGate,NULL,NULL,NULL,NULL,NULL};
            const u16 sizes[8]={8,16,0,0,0,0,0,0};
            for(unsigned j=0;j<8;++j)if(p->table.callbacks[j]!=callbacks[j] || p->table.sizes[j]!=sizes[j])ok=0;
            if(p->initialize!=RoomLib_SetArg1 || p->script!=g_RoomM318FlashScript)ok=0;
        }
        {
            FieldFxTaskProgram *p=&g_RoomM318TwinRibbonEffects;
            const u32 original[14]={0x80196e4cu,0x0u,0x0u,0x0u,0x0u,0x0u,0x0u,0x0u,0x188u,0x0u,0x0u,0x0u,0x8019937cu,0x801997f8u};
            if(memcmp((u8 *)room+0x8019980cu-0x8018efe8u,original,sizeof(original)))ok=0;
            FieldFxTaskCallback callbacks[8]={RoomM318_TwinRibbon,NULL,NULL,NULL,NULL,NULL,NULL,NULL};
            const u16 sizes[8]={392,0,0,0,0,0,0,0};
            for(unsigned j=0;j<8;++j)if(p->table.callbacks[j]!=callbacks[j] || p->table.sizes[j]!=sizes[j])ok=0;
            if(p->initialize!=RoomM318_SetScriptArg2 || p->script!=g_RoomM318TwinRibbonScript)ok=0;
        }
        {
            FieldFxTaskProgram *p=&g_RoomM318EmberFountainEffects;
            const u32 original[14]={0x80197cbcu,0x801988f8u,0x0u,0x0u,0x0u,0x0u,0x0u,0x0u,0x100010u,0x0u,0x0u,0x0u,0x8019938cu,0x80199844u};
            if(memcmp((u8 *)room+0x801998b8u-0x8018efe8u,original,sizeof(original)))ok=0;
            FieldFxTaskCallback callbacks[8]={RoomM318_EmberFountain,RoomM318_EmberSpiral,NULL,NULL,NULL,NULL,NULL,NULL};
            const u16 sizes[8]={16,16,0,0,0,0,0,0};
            for(unsigned j=0;j<8;++j)if(p->table.callbacks[j]!=callbacks[j] || p->table.sizes[j]!=sizes[j])ok=0;
            if(p->initialize!=RoomM318_GetActorParams || p->script!=g_RoomM318EmberFountainScript)ok=0;
        }
    }
    free(room);if(ok)puts("Original M318 CUE: 41 constant/track/script ranges and all eight complete native descriptors passed");return ok;
}

static int original_m318_gate(void)
{
    FieldActor *old_player=g_PlayerEntity;FieldFxTaskContext *old_context=g_FieldFxTaskContext;
    int old_age=g_FieldFxCurrentAge;FieldFxTaskContext task,expected;FieldActor player={0};Combatant state={0};
    player.state=(void *)&state;g_PlayerEntity=&player;g_FieldFxTaskContext=&task;unsigned count=0;int ok=1;
    for(int mode=-1;mode<5;mode++)for(int age=-1;age<66;age++)for(int limit=0;limit<65;limit+=8)for(int flag=0;flag<2;flag++){
        memset(&task,0x5a,sizeof(task));expected=task;g_FieldFxCurrentAge=age;
        state.stateFlags=0x89432101u|(flag?0x10000000u:0);unsigned flags=state.stateFlags;int want=0;
        if(mode==0){expected.variables[0]=expected.variables[1]=0;flags&=~0x10000000u;}
        if(mode==1 && (flag || age>=limit)){want=1;expected.variables[0]=1;expected.variables[1]=flag;}
        if(RoomM318_PlayerGate(mode,NULL,(intptr_t)&limit)!=want || state.stateFlags!=flags || memcmp(&task,&expected,sizeof(task)))ok=0;
        ++count;
    }
    g_PlayerEntity=old_player;g_FieldFxTaskContext=old_context;g_FieldFxCurrentAge=old_age;
    if(ok && count==7236)puts("Original M318 player gate: 7236 USA MIPS cases and entire native task canary passed");
    return ok && count==7236;
}

static int original_m256_assets(PeDisc *disc,PeDiscFile *exe)
{
    SceneRoomSectorRange range;PeDiscFile archive;
    if(!pe_disc_find(disc,"PE.IMG",&archive) || !pe_disc_read(disc,exe,0x800ull+0x80093378u-0x80010000u+255*8,&range,8))return 0;
    size_t bytes=(size_t)range.roomSectors*2048;SceneAssetView *room=malloc(bytes);if(!room)return 0;
    int ok=bytes==348160 && pe_disc_read(disc,&archive,(u64)(range.start+range.scratchSectors+range.textureSectors)*2048,room,bytes);
    if(ok){
        SceneRoomDirectory *dir=SceneAsset_ResolveOffset(room,room->header.directoryOffset);SceneRoomRecord *records=SceneAsset_ResolveOffset(room,dir->scripts&0x3fffff);
        const u32 addresses[]={0x80195d38,0x80195eb4,0x80195de8,0x80195da8};const unsigned slots[]={75,132,131,76};
        if(dir->scripts>>22!=4)ok=0;
        for(unsigned i=0;i<4 && ok;++i)if(records[i].source.bytes.slot!=slots[i] || records[i].u.handlerAddress!=addresses[i])ok=0;
        const struct {u32 address;const void *data;size_t size;} data[]={
            {0x8018f1cc,&g_RoomM256SpriteTilt,8},{0x8018f1d4,&g_RoomM256DropColor,4},
            {0x8018f208,&g_RoomM256ScatterDropAnchor,8},{0x8018f210,&g_RoomM256SwirlSparkAnchor,8},
            {0x8018f218,&g_RoomM256TrailColor,4},{0x8018f21c,&g_RoomM256TrailDarkColor,4},
            {0x8018f220,&g_RoomM256TrailAnchor,8},{0x8018f258,&g_RoomM256PulseAnchor,8},
            {0x80195dc4,g_RoomM256ScatterScript,36},{0x80195e20,g_RoomM256TrailScript,68},
            {0x80195e64,g_RoomM256PulseParticleColorTrack,40},{0x80195e8c,g_RoomM256PulseColorTrack,40}};
        for(unsigned i=0;i<sizeof(data)/sizeof(*data) && ok;++i)if(memcmp((u8 *)room+data[i].address-0x8018efe8,data[i].data,data[i].size))ok=0;
        const u32 class_methods[]={0x8019257c,0x80192584,0x801925ac,0x80192624,0x8019262c,0x80192808,0x8019283c};
        if(memcmp((u8 *)room+0x80195da8u-0x8018efe8u,class_methods,sizeof(class_methods)))ok=0;
        if(g_RoomM256SwingClass.init!=RoomM256_SwingNop0 || g_RoomM256SwingClass.initialize!=RoomM256_SwingInit ||
           g_RoomM256SwingClass.send!=RoomM256_SwingConfigure || g_RoomM256SwingClass.start!=RoomM256_SwingNop3 ||
           g_RoomM256SwingClass.execute!=RoomLib_UpdateCallback || g_RoomM256SwingClass.stop!=RoomM256_SwingRelease || g_RoomM256SwingClass.unload!=RoomM256_SwingNop6)ok=0;
        for(unsigned i=0;i<2 && ok;++i){
            FieldFxTaskProgram *p=i?&g_RoomM256TrailEffects:&g_RoomM256ScatterEffects;u32 raw[14];memcpy(raw,(u8 *)room+(i?0x80195eb4u:0x80195de8u)-0x8018efe8u,sizeof(raw));
            u32 callbacks[2][2]={{0x801934ac,0x80193bfc},{0x80195320,0x80195728}};
            FieldFxTaskCallback native[2][2]={{RoomM256_ScatterDropEmitter,RoomM256_SwirlSparkEmitter},{RoomM256_TrailEmitter,RoomM256_ModelPulse}};
            for(unsigned j=0;j<8;++j){if(raw[j]!=(j<2?callbacks[i][j]:0) || p->table.callbacks[j]!=(j<2?native[i][j]:NULL))ok=0;}
            if(raw[12]!=(i?0x80195cd8:0x80195cc8) || raw[13]!=(i?0x80195e20:0x80195dc4) || p->initialize!=(i?RoomM256_SetTrailTurnScale:RoomM256_ScatterConfigure) || p->script!=(i?g_RoomM256TrailScript:g_RoomM256ScatterScript))ok=0;
            u16 sizes[8];memcpy(sizes,raw+8,16);for(unsigned j=0;j<8;++j)if(sizes[j]!=p->table.sizes[j])ok=0;
        }
    }
    /* Whole shared actor module and every class entry, with only local relocation. */
    if(ok){
        SceneRoomSectorRange canonical={0};if(!pe_disc_read(disc,exe,0x800ull+0x80093378u-0x80010000u+21*8,&canonical,8))ok=0;
        size_t size=(size_t)canonical.roomSectors*2048;u8 *data=ok?malloc(size):NULL;
        if(!data || size<0x35a8 || !pe_disc_read(disc,&archive,(u64)(canonical.start+canonical.scratchSectors+canonical.textureSectors)*2048,data,size))ok=0;
        unsigned jumps=0,addresses=0;
        for(unsigned at=0;ok && at<13084;at+=4){
            u32 a,b;memcpy(&a,data+0x1e4+at,4);memcpy(&b,(u8 *)room+0x278+at,4);
            if(a==b)continue;unsigned op=a>>26;u32 target=0x80000000u+((a&0x3ffffffu)<<2);
            if((op==2 || op==3) && b==a+37 && target>=0x8018f1ccu && target<0x801924e8u){++jumps;continue;}
            target=0x80190000u+(s16)a;
            if(op==9 && (a&0xffff0000u)==(b&0xffff0000u) && (u16)b==(u16)(a+148) && target>=0x8018f1ccu && target<0x801924e8u){++addresses;continue;}
            fprintf(stderr,"M256 shared instruction mismatch %x %08x/%08x\n",at,a,b);ok=0;
        }
        for(unsigned i=0;ok && i<42;++i){u32 a,b;memcpy(&a,data+0x3500+i*4,4);memcpy(&b,(u8 *)room+0x6d18+i*4,4);if(!((!a && !b) || b==a+148))ok=0;}
        if(jumps!=111 || addresses!=20)ok=0;free(data);
    }
    if(ok)ok=original_m256_programs(room);
    free(room);if(ok)puts("Original M256 CUE: four descriptors, complete swing class, two programs/scripts and all constants passed");return ok;
}

static int original_m404_states(void)
{
    FieldActor *old_player=g_PlayerEntity;FieldActor player={0};
    FieldFxTaskContext *old_context=g_FieldFxTaskContext,context={0};
    FieldFxTaskSlot *old_task=g_FieldFxActiveTaskSlot;
    PolygonVertex *old_polygon=g_FloorPolygonVertices;u16 old_count=g_FloorPolygonVertexCount;
    PolygonVertex polygon[]={{0,-1000,0,-1000},{0,1000,0,-1000},{0,1000,0,1000},{0,-1000,0,1000}};
    RoomFloorY old_floor=*g_RoomFloorY;u32 old_rng=Psyz_RandState();
    struct {_Alignas(void *) char data[2048];u8 guard[16];} pool;
    FieldFxTaskSlot task={.end=pool.data};g_FieldFxActiveTaskSlot=&task;g_FieldFxTaskContext=&context;
    player.pos_x=player.pos_z=10000*65536;g_PlayerEntity=&player;
    g_FloorPolygonVertices=polygon;g_FloorPolygonVertexCount=4;
    const int modes[]={0,1,3},timers[]={0,1,7,15,31,47,63,65535},velocities[]={-32768,-1,32767},floors[]={-100,0,100};
    const u32 seeds[]={0,0x12345678};u64 hash=UINT64_C(14695981039346656037);unsigned cases=0;int ok=1;
    for(unsigned fn=0;fn<2;++fn)for(unsigned mode=0;mode<3;++mode)for(unsigned state=0;state<(fn?5:2);++state)
    for(unsigned timer=0;timer<8;++timer)for(unsigned velocity=0;velocity<3;++velocity)for(unsigned floor=0;floor<3;++floor)
    for(unsigned seed=0;seed<2;++seed)for(unsigned full=0;full<(fn?2:1);++full){
        memset(&pool,0xa5,sizeof(pool));FieldFx_InitCallbackList(pool.data,20,55,RoomM404_SinkingDropCallback);
        FieldFxCallbackList *list=(void *)pool.data;
        if(full)for(unsigned i=0;i<55;++i)((FieldFxCallbackListEntry *)(list->entries+i*list->stride))->active=1;
        _Alignas(void *) u8 data[36];memset(data,0xa5,sizeof(data));
        u16 words[]={123,(u16)-77,456,65530,(u16)velocities[velocity],(u16)velocities[velocity],(u16)velocities[velocity],8192,state,timers[timer]};
        memcpy(data+8,words,20);g_RoomFloorY->y=floors[floor];Psyz_Srand(seeds[seed]);
        u32 result=(fn?RoomM404_SinkingDropCallback:RoomM404_DebrisSparkCallback)(modes[mode],data+8,0),rng=Psyz_RandState();
        hash=m349_hash_bytes(hash,&result,4);hash=m349_hash_bytes(hash,data,sizeof(data));hash=m349_hash_bytes(hash,&rng,4);
        for(unsigned i=0;i<55;++i){FieldFxCallbackListEntry *entry=(void *)(list->entries+i*list->stride);hash=m349_hash_bytes(hash,entry,4);hash=m349_hash_bytes(hash,entry->state,20);}
        for(unsigned i=0;i<16;++i)if(pool.guard[i]!=0xa5)ok=0;
        ++cases;
    }
    g_PlayerEntity=old_player;g_FieldFxTaskContext=old_context;g_FieldFxActiveTaskSlot=old_task;
    g_FloorPolygonVertices=old_polygon;g_FloorPolygonVertexCount=old_count;*g_RoomFloorY=old_floor;Psyz_Srand(old_rng);
    if(!ok || cases!=5184 || hash!=UINT64_C(0x3a727777dace1af6)){
        fprintf(stderr,"M404 CPU state mismatch %u/%016llx guards%d\n",cases,(unsigned long long)hash,ok);return 0;}
    puts("Original M404: 5184 particle states, damping, bounce, floor polygon, real pool/full pool and BIOS RNG match USA MIPS");return 1;
}

static int original_m404_contact(void)
{
    FieldActor *old_player=g_PlayerEntity;FieldActor player={0},actor={0};FieldActorState core={0};Combatant player_core={0};
    FieldFxObjectPrefix *old_owner=g_FieldFxOwner,owner={.actor=&actor};
    FieldFxTaskContext *old_context=g_FieldFxTaskContext,context={0};FieldFxTaskSlot *old_task=g_FieldFxActiveTaskSlot;
    PolygonVertex *old_polygon=g_FloorPolygonVertices;u16 old_count=g_FloorPolygonVertexCount;
    PolygonVertex polygon[]={{0,-1000,0,-1000},{0,1000,0,-1000},{0,1000,0,1000},{0,-1000,0,1000}};
    RoomFloorY old_floor=*g_RoomFloorY;u32 old_rng=Psyz_RandState();
    struct {_Alignas(void *) char data[2048];u8 guard[16];} pool;FieldFxTaskSlot task={.end=pool.data};
    player.state=(void *)&player_core;actor.state=&core;g_PlayerEntity=&player;g_FieldFxOwner=&owner;g_FieldFxTaskContext=&context;g_FieldFxActiveTaskSlot=&task;
    g_FloorPolygonVertices=polygon;g_FloorPolygonVertexCount=4;g_RoomFloorY->y=0;
    const int states[]={0,1,4},xs[]={-111,-110,-109,0,109,110,111},heights[]={-579,-578,-577,0};
    const u32 flags[]={0,0x01000000,0x01123456,0x81000000,0x11000000,0xc1000000,0xffffffff};
    u64 hash=UINT64_C(14695981039346656037);unsigned cases=0;int ok=1;
    for(unsigned state=0;state<3;++state)for(unsigned x=0;x<7;++x)for(unsigned enabled=0;enabled<2;++enabled)
    for(unsigned flag=0;flag<7;++flag)for(unsigned height=0;height<4;++height){
        memset(&pool,0xa5,sizeof(pool));FieldFx_InitCallbackList(pool.data,20,55,RoomM404_SinkingDropCallback);
        _Alignas(void *) u8 data[36];memset(data,0xa5,sizeof(data));u16 words[]={123,(u16)heights[height],456,65530,0,0,0,8192,states[state],0};memcpy(data+8,words,20);
        context.flags=enabled;core.core_flags=flags[flag];player_core.stateFlags=0x12345678;player.pos_x=(123+xs[x])*65536;player.pos_z=456*65536;Psyz_Srand(0x12345678);
        u32 result=RoomM404_SinkingDropCallback(1,data+8,0),rng=Psyz_RandState();
        hash=m349_hash_bytes(hash,&result,4);hash=m349_hash_bytes(hash,data,36);hash=m349_hash_bytes(hash,&rng,4);
        hash=m349_hash_bytes(hash,&core.core_flags,4);hash=m349_hash_bytes(hash,&player_core.stateFlags,4);
        FieldFxCallbackList *list=(void *)pool.data;
        for(unsigned i=0;i<55;++i){FieldFxCallbackListEntry *entry=(void *)(list->entries+i*list->stride);hash=m349_hash_bytes(hash,entry,4);hash=m349_hash_bytes(hash,entry->state,20);}
        for(unsigned i=0;i<16;++i)if(pool.guard[i]!=0xa5)ok=0;++cases;
    }
    g_PlayerEntity=old_player;g_FieldFxOwner=old_owner;g_FieldFxTaskContext=old_context;g_FieldFxActiveTaskSlot=old_task;
    g_FloorPolygonVertices=old_polygon;g_FloorPolygonVertexCount=old_count;*g_RoomFloorY=old_floor;Psyz_Srand(old_rng);
    if(!ok || cases!=1176 || hash!=UINT64_C(0x895af889e0f51685)){fprintf(stderr,"M404 contact CPU mismatch %u/%016llx\n",cases,(unsigned long long)hash);return 0;}
    puts("Original M404 contact: 1176 radius/height/held-context/core-phase cases, both actor flags, payload, children and RNG match USA MIPS");return 1;
}

static int original_m404_programs(SceneAssetView *room)
{
    PmTables old_tables=g_PmTables;Pe1GameState old_game=g_GameState;u32 old_flags=g_GameStateFlags;
    FieldFxTaskProgram *old_programs[]={g_FieldFxTaskPrograms[48],g_FieldFxTaskPrograms[49]};
    FieldFxTaskProgram *programs[]={&g_RoomM404SinkingDropEffects,&g_RoomM404DebrisBurstEffects};
    int old_arg=g_RoomScriptArg;FieldActor *old_player=g_PlayerEntity;
    FieldFxObjectPrefix *old_owner=g_FieldFxOwner;FieldFxTaskContext *old_context=g_FieldFxTaskContext;
    FieldFxTaskSlot *old_task=g_FieldFxActiveTaskSlot;int old_age=g_FieldFxCurrentAge;
    RenderBufferPrefix old_buffers=D_800B0E38;int old_draw=g_ActiveDrawSlot,old_offset=g_DrawPacketOffset;
    RenderEffectParameters old_params=g_FieldFxSpriteParams;
    GteMatrix *old_view=g_GeomScroll.words.viewMatrix;int *old_projection=g_GeomScroll.words.projectionDistance;
    u32 old_rng=Psyz_RandState();RoomFloorY old_floor=*g_RoomFloorY;
    PolygonVertex *old_polygon=g_FloorPolygonVertices;u16 old_vertices=g_FloorPolygonVertexCount;
    PolygonVertex polygon[]={{0,-2000,0,-2000},{0,2000,0,-2000},{0,2000,0,2000},{0,-2000,0,2000}};
    AkaoQueueEntry old_audio[16];memcpy(old_audio,g_AkaoMessageQueue,sizeof(old_audio));int old_count=g_AkaoMessageQueueCount;
    static RenderGpuTag ordering[4096];static struct {_Alignas(void *) u8 data[262144];u8 guard[32];} packets;
    GteMatrix view={.m={{4096,0,0},{0,4096,0},{0,0,4096}}};int projection=256,ok=1;unsigned cycles=0;
    g_GeomScroll.words.viewMatrix=&view;g_GeomScroll.words.projectionDistance=&projection;
    g_GameState.loaded_scene_archive=(void *)room;g_GameStateFlags=0x80;g_GameState.flags&=~8u;g_GameState.tim_load_state=0;g_RoomFloorY->y=0;
    g_FloorPolygonVertices=polygon;g_FloorPolygonVertexCount=4;
    for(unsigned i=0;i<2;++i)g_FieldFxTaskPrograms[48+i]=programs[i];
    FieldFx_InitTexturePageTable();Psyz_GteCtrlWrite(26,256);Psyz_GteCtrlWrite(29,1365);Psyz_GteCtrlWrite(30,1024);gte_SetGeomOffset(160,112);
    for(unsigned program=0;ok && program<2;++program)for(unsigned variant=0;ok && variant<2;++variant)for(unsigned draw=0;ok && draw<2;++draw){
        struct {PmPrimarySlot slots[11];u8 guard[32];} bank={0};memset(bank.guard,0xa5,32);
        FieldActor actor={0},player={0};FieldActorState state={0},player_state={0};u8 action=1;
        RenderMatrix matrices[4]={0};actor.render_object.matrices=matrices;actor.state=&state;state.action_state=&action;
        actor.pos_y=-200*65536;actor.pos_z=1024*65536;player.state=&player_state;player.pos_z=variant?964*65536:3000*65536;player.pos_x=variant?0:3000*65536;
        for(unsigned j=0;j<4;++j){for(unsigned k=0;k<3;++k)matrices[j].rotation[k][k]=4096;matrices[j].translation[1]=-200;matrices[j].translation[2]=1024;}
        g_PlayerEntity=&player;g_PmTables=(PmTables){g_PmCmdHandlerArray,bank.slots,NULL};srand(variant+1);
        if(Scene_LoadRoomAssets(133+program,&actor)!=0 || Pm_SendCmd(0,0,0,60,0,0)){ok=0;break;}
        unsigned frame=0,drawn=0,children=0,seen=0,handshake=0;
        for(;frame<240;++frame){
            for(unsigned j=0;j<4096;++j)ordering[j]=(RenderGpuTag){.address=0xffffff};
            memset(packets.guard,0xa5,32);g_ActiveDrawSlot=draw;g_DrawPacketOffset=0;
            D_800B0E38.ordering[draw]=ordering;D_800B0E38.packets[draw]=(char *)packets.data;
            if(Pm_Start(0) || Pm_Exec(0)){ok=0;break;}
            handshake|=action==2;
            if(g_AkaoMessageQueueCount<0 || g_AkaoMessageQueueCount>16){ok=0;break;}
            g_AkaoMessageQueueCount=0;drawn+=g_DrawPacketOffset!=0;FieldFxTaskContext *ctx=&bank.slots[0].task.tasks;
            for(unsigned k=0;k<8;++k)if(ctx->slots[k].id!=0xffff){
                if(ctx->slots[k].id!=0 || !ctx->slots[k].end){ok=0;break;}seen=1;
                FieldFxCallbackList *list=(void *)ctx->slots[k].end;
                if(list->count!=(program?32:55) || list->callback!=(program?RoomM404_DebrisSparkCallback:RoomM404_SinkingDropCallback)){ok=0;break;}
                for(unsigned j=0;j<(unsigned)list->count;++j)children+=((FieldFxCallbackListEntry *)(list->entries+j*list->stride))->active!=0;
            }
            if(ctx->used>sizeof(ctx->arena) || g_DrawPacketOffset<0 || g_DrawPacketOffset>sizeof(packets.data)){ok=0;break;}
            for(unsigned j=0;j<4096 && ok;++j){uintptr_t link=ordering[j].address;unsigned count=0;
                while(link && link!=0xffffff){if(link<(uintptr_t)packets.data || link+sizeof(RenderGpuTag)>(uintptr_t)packets.data+g_DrawPacketOffset || ++count>8192){ok=0;break;}link=((RenderGpuTag *)link)->address;}}
            for(unsigned j=0;j<32;++j)if(bank.guard[j]!=0xa5 || packets.guard[j]!=0xa5)ok=0;
            if(!ok || !bank.slots[0].header.state)break;
        }
        int hit=(player_state.flags&0x4000)!=0;
        if((!program && (hit!=(int)variant || !handshake || action!=4)) || (program && hit))ok=0;
        if(!ok || frame==240 || !drawn || !children || !seen || bank.slots[0].header.state || bank.slots[0].task.tasks.count){
            fprintf(stderr,"M404 PM %u/%u/%u frames%u drawn%u children%u state%u hit%d action%u\n",program,variant,draw,frame,drawn,children,bank.slots[0].header.state,hit,action);ok=0;}
        if(ok){++cycles;fprintf(stderr,"M404 PM %u/%u/%u frames%u drawn%u children%u hit%d passed\n",program,variant,draw,frame,drawn,children,hit);}
    }
    g_PmTables=old_tables;g_GameState=old_game;g_GameStateFlags=old_flags;g_PlayerEntity=old_player;g_RoomScriptArg=old_arg;
    for(unsigned i=0;i<2;++i)g_FieldFxTaskPrograms[48+i]=old_programs[i];
    g_FieldFxOwner=old_owner;g_FieldFxTaskContext=old_context;g_FieldFxActiveTaskSlot=old_task;g_FieldFxCurrentAge=old_age;g_FieldFxSpriteParams=old_params;
    D_800B0E38=old_buffers;g_ActiveDrawSlot=old_draw;g_DrawPacketOffset=old_offset;g_GeomScroll.words.viewMatrix=old_view;g_GeomScroll.words.projectionDistance=old_projection;
    g_FloorPolygonVertices=old_polygon;g_FloorPolygonVertexCount=old_vertices;
    srand(old_rng);*g_RoomFloorY=old_floor;memcpy(g_AkaoMessageQueue,old_audio,sizeof(old_audio));g_AkaoMessageQueueCount=old_count;
    if(ok)printf("Original M404 PM: %u complete authored cycles, two programs, both draw slots, hit/no-hit, rendering, natural expiry and pool/packet/OT guards passed\n",cycles);
    return ok;
}

static int original_m404_assets(PeDisc *disc,PeDiscFile *exe)
{
    SceneRoomSectorRange range;PeDiscFile archive;
    if(!pe_disc_find(disc,"PE.IMG",&archive) || !pe_disc_read(disc,exe,0x800ull+0x80093378u-0x80010000u+403*8,&range,8))return 0;
    size_t bytes=(size_t)range.roomSectors*2048;SceneAssetView *room=malloc(bytes);if(!room)return 0;
    int ok=bytes==0x41800 && pe_disc_read(disc,&archive,(u64)(range.start+range.scratchSectors+range.textureSectors)*2048,room,bytes);
    if(ok){
        SceneRoomDirectory *dir=SceneAsset_ResolveOffset(room,room->header.directoryOffset);SceneRoomRecord *records=SceneAsset_ResolveOffset(room,dir->scripts&0x3fffff);
        const u32 addresses[]={0x80193e6c,0x80193f1c,0x80193f88};const unsigned slots[]={75,133,134};
        if(dir->scripts>>22!=3)ok=0;
        for(unsigned i=0;i<3 && ok;++i)if(records[i].source.bytes.slot!=slots[i] || records[i].u.handlerAddress!=addresses[i])ok=0;
        const struct {u32 address;const void *data;size_t size;} data[]={
            {0x8018f1cc,&g_RoomM404DropFanTilt,8},{0x8018f1d4,&g_RoomM404DropFanColor,4},{0x8018f1d8,&g_RoomM404DropGlowColor,4},
            {0x8018f208,&g_RoomM404SinkingDropAnchor,8},{0x8018f210,&g_RoomM404DebrisSparkRotation,8},
            {0x8018f218,&g_RoomM404DebrisSparkColor,4},{0x8018f21c,&g_RoomM404DebrisBurstAnchor,8},
            {0x80193edc,g_RoomM404SinkingDropScript,32},{0x80193efc,g_RoomM404SinkingDropColorTrack,32},
            {0x80193f54,g_RoomM404DebrisBurstScript,20},{0x80193f68,g_RoomM404DebrisSparkColorTrack,32}};
        for(unsigned i=0;i<sizeof(data)/sizeof(*data) && ok;++i)if(memcmp((u8 *)room+data[i].address-0x8018efe8,data[i].data,data[i].size))ok=0;
        for(unsigned i=0;i<2 && ok;++i){
            FieldFxTaskProgram *p=i?&g_RoomM404DebrisBurstEffects:&g_RoomM404SinkingDropEffects;u32 raw[14];memcpy(raw,(u8 *)room+addresses[i+1]-0x8018efe8,sizeof(raw));
            if(raw[0]!=(i?0x80193a6c:0x80193368) || raw[12]!=(i?0:0x80193e24) || raw[13]!=(i?0x80193f54:0x80193edc))ok=0;
            if(p->table.callbacks[0]!=(i?RoomM404_DebrisBurst:RoomM404_SinkingDrop) || p->initialize!=(i?NULL:RoomLib_SetArg1) || p->script!=(i?g_RoomM404DebrisBurstScript:g_RoomM404SinkingDropScript))ok=0;
            for(unsigned j=1;j<8;++j)if(raw[j] || p->table.callbacks[j])ok=0;
            u16 sizes[8];memcpy(sizes,raw+8,16);for(unsigned j=0;j<8;++j)if(sizes[j]!=p->table.sizes[j])ok=0;
        }
    }
    /* Whole shared actor module and every class entry, with only local relocation. */
    if(ok){
        SceneRoomSectorRange canonical={0};if(!pe_disc_read(disc,exe,0x800ull+0x80093378u-0x80010000u+21*8,&canonical,8))ok=0;
        size_t size=(size_t)canonical.roomSectors*2048;u8 *data=ok?malloc(size):NULL;
        if(!data || size<0x35a8 || !pe_disc_read(disc,&archive,(u64)(canonical.start+canonical.scratchSectors+canonical.textureSectors)*2048,data,size))ok=0;
        unsigned jumps=0,addresses=0;
        for(unsigned at=0;ok && at<13084;at+=4){
            u32 a,b;memcpy(&a,data+0x1e4+at,4);memcpy(&b,(u8 *)room+0x23c+at,4);
            if(a==b)continue;unsigned op=a>>26;u32 target=0x80000000u+((a&0x3ffffffu)<<2);
            if((op==2 || op==3) && b==a+22 && target>=0x8018f1ccu && target<0x801924e8u){++jumps;continue;}
            target=0x80190000u+(s16)a;
            if(op==9 && (a&0xffff0000u)==(b&0xffff0000u) && (u16)b==(u16)(a+88) && target>=0x8018f1ccu && target<0x801924e8u){++addresses;continue;}
            fprintf(stderr,"M404 shared instruction mismatch %x %08x/%08x\n",at,a,b);ok=0;
        }
        for(unsigned i=0;ok && i<42;++i){u32 a,b;memcpy(&a,data+0x3500+i*4,4);memcpy(&b,(u8 *)room+0x4e4c+i*4,4);if(!((!a && !b) || b==a+88))ok=0;}
        if(jumps!=111 || addresses!=20)ok=0;free(data);
    }
    if(ok)ok=original_m404_programs(room);
    free(room);if(ok)puts("Original M404 assets: all three real CUE descriptors, both complete programs/scripts, all constants, shared 13084 code bytes/42 class pointers passed");return ok;
}

static int original_m350_states(void)
{
    const FieldFxTaskCallback callbacks[]={RoomM350_HandSparkCallback,RoomM350_HandPulseCallback,RoomM350_HandGlowCallback,RoomM350_HandSweepTrapCallback,RoomM350_HandSweepHitCallback,RoomM350_HandFlashCallback,RoomM350_OrbHandSparkCallback,RoomM350_OrbBurstCallback,RoomM350_OrbTrailCallback,RoomM350_OrbTableFlashCallback,RoomM350_AreaSparkCallback,RoomM350_AreaDropletCallback,RoomM350_AreaPlayerPulseCallback,RoomM350_DropHitCallback,RoomM350_DropBurstCallback,RoomM350_DropTrailCallback,RoomM350_DropFlareCallback,RoomM350_DropSplashCallback,RoomM350_BodyDebrisCallback,RoomM350_BodyRingCallback};
    const int modes[]={0,1,3},ages[]={-1,0,1,3,4,7,8,15,16,17,31,32,47,48,127,255,32767,65535};
    int old_age=g_FieldFxCurrentAge;u64 hash=UINT64_C(14695981039346656037);unsigned cases=0;
    for(unsigned fn=0;fn<20;++fn)for(unsigned mode=0;mode<3;++mode)for(unsigned age=0;age<18;++age)for(unsigned variant=0;variant<4;++variant){
        _Alignas(void *) u8 data[80];for(unsigned i=0;i<80;++i)data[i]=(i*37+variant*3)&255;
        g_FieldFxCurrentAge=ages[age];u32 result=callbacks[fn](modes[mode],data+8,0);
        hash=m349_hash_bytes(hash,&result,4);hash=m349_hash_bytes(hash,data,80);++cases;
    }
    g_FieldFxCurrentAge=old_age;
    if(cases!=4320 || hash!=UINT64_C(0x5e7c9ee5b292e233)){
        fprintf(stderr,"M350 callback CPU mismatch %u/%016llx\n",cases,(unsigned long long)hash);return 0;}
    RoomM350OrbStorage saved=g_RoomM350OrbData;FieldFxObjectPrefix *old_owner=g_FieldFxOwner;
    FieldFxTaskSlot *old_slot=g_FieldFxActiveTaskSlot;FieldFxTaskContext *old_context=g_FieldFxTaskContext;
    _Alignas(void *) char pool[2048];u8 start_guard[16];memset(start_guard,0xa5,16);
    FieldFxTaskSlot task={.start=(char *)start_guard,.end=pool};FieldActor actor={.rot_y=123};FieldFxObjectPrefix owner={.actor=&actor};
    g_FieldFxOwner=&owner;g_FieldFxActiveTaskSlot=&task;g_FieldFxTaskContext=NULL;memset(&g_RoomM350OrbData,0,sizeof(g_RoomM350OrbData));
    GteShortVector positions[]={{10,20,30,0},{-100,0,300,0},{1000,-10,-20,0}};
    for(unsigned i=0;i<3;++i)g_RoomM350OrbData.pointers[i]=&positions[i];
    g_RoomM350OrbState.pointerCount=3;
    if(RoomM350_OrbImpactEmitter(0,NULL,0)<=0 || RoomM350_OrbImpactEmitter(1,NULL,0) || g_RoomM350OrbState.pointerCount)return 0;
    FieldFxCallbackList *list=(void *)pool;
    if(list->count!=16 || list->callback!=RoomM350_OrbImpactCallback)return 0;
    for(unsigned i=0;i<3;++i){
        FieldFxCallbackListEntry *entry=(void *)(list->entries+i*list->stride);GteShortVector *pointer;short offset;
        memcpy(&pointer,entry->state,sizeof(pointer));memcpy(&offset,entry->state+sizeof(pointer),2);
        if(entry->active!=1 || pointer!=&positions[i] || offset)return 0;
    }
    for(unsigned i=0;i<2;++i)g_RoomM350OrbData.positions[i]=positions[i];g_RoomM350OrbState.positionCount=2;
    if(RoomM350_OrbTrailEmitter(0,NULL,0)<=0 || RoomM350_OrbTrailEmitter(1,NULL,0) || g_RoomM350OrbState.positionCount)return 0;
    list=(void *)pool;if(list->count!=50 || list->callback!=RoomM350_OrbTrailCallback)return 0;
    for(unsigned i=0;i<2;++i){FieldFxCallbackListEntry *entry=(void *)(list->entries+i*list->stride);GteShortVector value;memcpy(&value,entry->state,8);
        if(entry->active!=1 || value.x!=positions[i].x || value.y!=positions[i].y || value.z!=positions[i].z || value.pad!=123)return 0;}
    for(unsigned i=0;i<16;++i)if(start_guard[i]!=0xa5)return 0;
    /* Execute all authored flash-table frames with both transform owners and
     * exhausted pools; the golden result executes the retail emitter and pool. */
    RenderMatrix own_matrices[32]={0},other_matrices[32]={0};FieldActor other={0};
    actor.render_object.matrices=own_matrices;other.render_object.matrices=other_matrices;
    for(unsigned i=0;i<32;++i)for(unsigned j=0;j<3;++j){
        own_matrices[i].translation[j]=i*100+j*7+11;
        other_matrices[i].translation[j]=-(int)(i*103+j*13+17);
    }
    u64 table_hash=UINT64_C(14695981039346656037);unsigned table_cases=0;
    for(unsigned frame=0;frame<18;++frame)for(unsigned repeat=0;repeat<3;++repeat)for(unsigned complete=0;complete<2;++complete){
        const unsigned repeats[]={1,2,8};memset(pool,0xa5,sizeof(pool));
        actor.anim.fixed=frame*65536;g_RoomM350OrbActor=&other;g_RoomM350OrbState.completionFlags[0]=complete;
        if(RoomM350_OrbTableFlashEmitter(0,NULL,0)<=0)return 0;
        u32 result=0;for(unsigned i=0;i<repeats[repeat];++i)result=RoomM350_OrbTableFlashEmitter(1,NULL,0);
        table_hash=m349_hash_bytes(table_hash,&result,4);list=(void *)pool;
        if(list->count!=10 || list->callback!=RoomM350_OrbTableFlashCallback)return 0;
        for(unsigned i=0;i<10;++i){FieldFxCallbackListEntry *entry=(void *)(list->entries+i*list->stride);
            table_hash=m349_hash_bytes(table_hash,&entry->active,2);
            if(entry->active)table_hash=m349_hash_bytes(table_hash,entry->state,8);
        }
        ++table_cases;
    }
    if(table_cases!=108 || table_hash!=UINT64_C(0xceb97844900f5774)){
        fprintf(stderr,"M350 flash-table CPU mismatch %u/%016llx\n",table_cases,(unsigned long long)table_hash);return 0;}
    FieldActor *old_player=g_PlayerEntity;FieldActor player={0};FieldActorState own_state={0},player_state={0};
    player.state=&player_state;g_PlayerEntity=&player;s16 old_gate=g_RoomM350DropHitCooldown;RoomFloorY old_floor=*g_RoomFloorY;
    const int gates[]={-1,0,1,30},floors[]={-100,0,200},heights[]={-513,-512,100},radii[]={-1,0,80,200};
    const int pairs[][2]={{0,0},{79,0},{80,0},{81,0},{56,56},{57,57},{199,0},{200,0},{201,0},{-80,0},{0,-80}};
    u64 hit_hash=UINT64_C(14695981039346656037);unsigned hit_cases=0;
    for(unsigned gate=0;gate<4;++gate)for(unsigned floor=0;floor<3;++floor)for(unsigned height=0;height<3;++height)
    for(unsigned pair=0;pair<11;++pair)for(unsigned radius=0;radius<4;++radius)for(unsigned present=0;present<2;++present){
        actor.state=present?&own_state:NULL;own_state.core_flags=0x5678;player_state.flags=0x1234;
        player.render_object.model_matrix.translation[0]=-123+pairs[pair][0];player.render_object.model_matrix.translation[2]=456+pairs[pair][1];
        g_RoomFloorY->y=floors[floor];g_RoomM350DropHitCooldown=gates[gate];
        s16 data[14]={111,222,333,444,-123,floors[floor]+heights[height],456,77,77,77,77,77,77,77};
        u32 result=RoomM350_TestDropPlayerHit(data,radii[radius]);
        hit_hash=m349_hash_bytes(hit_hash,&result,4);hit_hash=m349_hash_bytes(hit_hash,&player_state.flags,4);
        hit_hash=m349_hash_bytes(hit_hash,&own_state.core_flags,4);hit_hash=m349_hash_bytes(hit_hash,&g_RoomM350DropHitCooldown,2);hit_hash=m349_hash_bytes(hit_hash,data,sizeof(data));++hit_cases;
    }
    g_PlayerEntity=old_player;g_RoomM350DropHitCooldown=old_gate;*g_RoomFloorY=old_floor;
    if(hit_cases!=3168 || hit_hash!=UINT64_C(0x55c2bddf90a3b015)){
        fprintf(stderr,"M350 drop-contact CPU mismatch %u/%016llx\n",hit_cases,(unsigned long long)hit_hash);return 0;}
    g_RoomM350OrbData=saved;g_FieldFxOwner=old_owner;g_FieldFxActiveTaskSlot=old_slot;g_FieldFxTaskContext=old_context;
    puts("Original M350: 4320 states of 20 unchanged USA CPU callbacks match, 108 retail flash-table/pool and 3168 drop-contact states, native pointer/position queue aliases and full-width callback payloads passed");return 1;
}

static int original_m350_programs(SceneAssetView *room)
{
    PmTables old_tables=g_PmTables;Pe1GameState old_game=g_GameState;u32 old_flags=g_GameStateFlags;
    FieldFxTaskProgram *old_programs[6];const unsigned commands[]={155,156,158,159,160,161};
    GteShortVector old_points[4],old_corners[4];memcpy(old_points,g_RoomM350HandPoints,sizeof(old_points));memcpy(old_corners,g_RoomM350HandSweepTrapCorners,sizeof(old_corners));
    s16 old_repeats=g_RoomM350HandSweepRepeats;u8 old_done=g_RoomM350HandSweepDone,old_hit=g_RoomM350HandSweepHit;
    RenderEffectParameters old_params=g_FieldFxSpriteParams;
    FieldFxTaskProgram *programs[]={&g_RoomM350HandEffects,&g_RoomM350OrbEffects,&g_RoomM350AreaEffects,&g_RoomM350DropEffects,&g_RoomM350RushEffects,&g_RoomM350BodyEffects};
    FieldActor *old_player=g_PlayerEntity,*old_head=g_FieldActorListHead,*old_hand=g_RoomM350HandActor;
    RoomM350OrbStorage old_orb=g_RoomM350OrbData;RoomM350AreaStorage old_area=g_RoomM350AreaData;
    RoomM350TrailQueue old_drop=g_RoomEffectTrailPositions;RoomM350RushStorage old_rush=g_RoomM350RushData;
    FieldFxObjectPrefix *old_owner=g_FieldFxOwner;FieldFxTaskContext *old_context=g_FieldFxTaskContext;
    FieldFxTaskSlot *old_task=g_FieldFxActiveTaskSlot;int old_age=g_FieldFxCurrentAge;
    RenderBufferPrefix old_buffers=D_800B0E38;int old_draw=g_ActiveDrawSlot,old_offset=g_DrawPacketOffset;
    GteMatrix *old_view=g_GeomScroll.words.viewMatrix;int *old_projection=g_GeomScroll.words.projectionDistance;
    u32 old_rng=Psyz_RandState();RoomFloorY old_floor=*g_RoomFloorY;
    AkaoQueueEntry old_audio[16];memcpy(old_audio,g_AkaoMessageQueue,sizeof(old_audio));int old_count=g_AkaoMessageQueueCount;
    static RenderGpuTag ordering[4096];static struct {_Alignas(void *) u8 data[1048576];u8 guard[32];} packets;
    GteMatrix view={.m={{4096,0,0},{0,4096,0},{0,0,4096}}};int projection=256,ok=1;unsigned cycles=0;
    for(unsigned i=0;i<6;++i){old_programs[i]=g_FieldFxTaskPrograms[commands[i]-85];g_FieldFxTaskPrograms[commands[i]-85]=programs[i];}
    g_GeomScroll.words.viewMatrix=&view;g_GeomScroll.words.projectionDistance=&projection;
    g_GameState.loaded_scene_archive=(void *)room;g_GameStateFlags=0x80;g_GameState.flags&=~8u;g_GameState.tim_load_state=0;g_RoomFloorY->y=0;
    FieldFx_InitTexturePageTable();Psyz_GteCtrlWrite(26,256);Psyz_GteCtrlWrite(29,1365);Psyz_GteCtrlWrite(30,1024);gte_SetGeomOffset(160,112);
    for(unsigned program=0;ok && program<6;++program)for(unsigned variant=0;ok && variant<2;++variant)for(unsigned draw=0;ok && draw<2;++draw){
        struct {PmPrimarySlot slots[11];u8 guard[32];} bank={0};memset(bank.guard,0xa5,32);
        FieldActor actor={0},player={0};FieldActorState state={0},player_state={0};u8 action=1;
        RenderMatrix matrices[48]={0};RenderObjectHeader header={.part_count=48};
        actor.render_object.matrices=matrices;actor.render_object.header=&header;actor.state=&state;state.action_state=&action;
        actor.type_id=6;actor.sub_id=0;actor.mode=((u8[]){11,13,9,7,9,0})[program];actor.action=64;
        actor.pos_y=-200*65536;actor.pos_z=1024*65536;
        player.state=&player_state;player.mode=4;int px=variant?(program==0?-212:(program==1 || program==3)?100:0):3000,pz=variant?(program==0?-512:1024):3000;player.pos_x=px*65536;player.pos_z=pz*65536;
        player.render_object.matrices=matrices;player.render_object.model_matrix.translation[0]=px;player.render_object.model_matrix.translation[2]=pz;
        for(unsigned j=0;j<48;++j){for(unsigned k=0;k<3;++k)matrices[j].rotation[k][k]=4096;matrices[j].translation[0]=(j&1)?100:-100;matrices[j].translation[1]=-200;matrices[j].translation[2]=1024;}
        actor.render_object.model_matrix=matrices[0];g_PlayerEntity=&player;g_FieldActorListHead=&actor;
        memset(&g_RoomM350OrbData,0,sizeof(g_RoomM350OrbData));memset(&g_RoomM350AreaData,0,sizeof(g_RoomM350AreaData));
        memset(&g_RoomEffectTrailPositions,0,sizeof(g_RoomEffectTrailPositions));memset(&g_RoomM350RushData,0,sizeof(g_RoomM350RushData));
        g_PmTables=(PmTables){g_PmCmdHandlerArray,bank.slots,NULL};srand(1);
        intptr_t first=program<2?6:program==2?variant:1;
        if(Scene_LoadRoomAssets(commands[program],&actor)!=0 || Pm_SendCmd(0,0,0,first,program==2 && variant?-512:0,0)){ok=0;break;}
        unsigned frame=0,drawn=0,children=0,seen=0,child_seen=0;
        for(;frame<720;++frame){
            for(unsigned j=0;j<4096;++j)ordering[j]=(RenderGpuTag){.address=0xffffff};
            memset(packets.guard,0xa5,32);g_ActiveDrawSlot=draw;g_DrawPacketOffset=0;
            D_800B0E38.ordering[draw]=ordering;D_800B0E38.packets[draw]=(char *)packets.data;
            if(program==3 && variant)actor.mode=frame<8?6:7;
            if(Pm_Start(0) || Pm_Exec(0)){ok=0;break;}
            if(g_AkaoMessageQueueCount<0 || g_AkaoMessageQueueCount>16){ok=0;break;}
            g_AkaoMessageQueueCount=0;drawn+=g_DrawPacketOffset!=0;FieldFxTaskContext *ctx=&bank.slots[0].task.tasks;
            for(unsigned k=0;k<8;++k)if(ctx->slots[k].id!=0xffff){
                seen|=1u<<ctx->slots[k].id;
                if(ctx->slots[k].end){FieldFxCallbackList *list=(void *)ctx->slots[k].end;
                    for(unsigned j=0;j<(unsigned)list->count;++j)if(((FieldFxCallbackListEntry *)(list->entries+j*list->stride))->active){++children;child_seen|=1u<<ctx->slots[k].id;}}
            }
            if(ctx->used>sizeof(ctx->arena) || g_DrawPacketOffset<0 || g_DrawPacketOffset>sizeof(packets.data)){ok=0;break;}
            for(unsigned j=0;j<4096 && ok;++j){uintptr_t link=ordering[j].address;unsigned count=0;
                while(link && link!=0xffffff){if(link<(uintptr_t)packets.data || link+sizeof(RenderGpuTag)>(uintptr_t)packets.data+g_DrawPacketOffset || ++count>65536){ok=0;break;}link=((RenderGpuTag *)link)->address;}}
            for(unsigned j=0;j<32;++j)if(bank.guard[j]!=0xa5 || packets.guard[j]!=0xa5)ok=0;
            if(!ok || !bank.slots[0].header.state)break;
            actor.anim_prev=actor.anim.fixed;actor.anim.fixed+=65536;
        }
        if(child_seen!=((unsigned[6][2]){{0x2f,0x3f},{0x3f,0x3f},{5,13},{0x3f,0x20},{5,5},{3,3}})[program][variant])ok=0;
        if((!variant && (player_state.flags&0x4000)) || (variant && (program<5 && program!=3) && !(player_state.flags&0x4000)))ok=0;
        if(!ok || frame==720 || !drawn || !children || seen!=((unsigned[]){0xbf,0xbf,0x8f,0xbf,0x85,3})[program] || bank.slots[0].header.state || bank.slots[0].task.tasks.count){
            fprintf(stderr,"M350 PM %u/%u frames%u drawn%u children%u seen%x state%u remaining%d\n",program,draw,frame,drawn,children,seen,bank.slots[0].header.state,bank.slots[0].task.tasks.count);ok=0;}
        if(ok){++cycles;fprintf(stderr,"M350 PM %u/%u/%u passed frames%u seen%x children%x hit%u\n",program,variant,draw,frame,seen,child_seen,(player_state.flags&0x4000)!=0);}
    }
    for(unsigned i=0;i<6;++i)g_FieldFxTaskPrograms[commands[i]-85]=old_programs[i];
    g_PmTables=old_tables;g_GameState=old_game;g_GameStateFlags=old_flags;g_PlayerEntity=old_player;g_FieldActorListHead=old_head;
    memcpy(g_RoomM350HandPoints,old_points,sizeof(old_points));memcpy(g_RoomM350HandSweepTrapCorners,old_corners,sizeof(old_corners));
    g_RoomM350HandSweepRepeats=old_repeats;g_RoomM350HandSweepDone=old_done;g_RoomM350HandSweepHit=old_hit;g_FieldFxSpriteParams=old_params;
    g_RoomM350HandActor=old_hand;g_RoomM350OrbData=old_orb;g_RoomM350AreaData=old_area;g_RoomEffectTrailPositions=old_drop;g_RoomM350RushData=old_rush;
    g_FieldFxOwner=old_owner;g_FieldFxTaskContext=old_context;g_FieldFxActiveTaskSlot=old_task;g_FieldFxCurrentAge=old_age;
    D_800B0E38=old_buffers;g_ActiveDrawSlot=old_draw;g_DrawPacketOffset=old_offset;g_GeomScroll.words.viewMatrix=old_view;g_GeomScroll.words.projectionDistance=old_projection;
    srand(old_rng);*g_RoomFloorY=old_floor;memcpy(g_AkaoMessageQueue,old_audio,sizeof(old_audio));g_AkaoMessageQueueCount=old_count;
    if(ok)printf("Original M350 PM: %u authored cycles, six programs and both draw slots, particle rendering, natural expiry and packet/OT/pool guards passed\n",cycles);
    return ok;
}

static int original_m350_assets(PeDisc *disc,PeDiscFile *exe)
{
    SceneRoomSectorRange range;PeDiscFile archive;
    if(!pe_disc_find(disc,"PE.IMG",&archive) || !pe_disc_read(disc,exe,0x800ull+0x80093378u-0x80010000u+349*8,&range,8))return 0;
    size_t bytes=(size_t)range.roomSectors*2048;SceneAssetView *room=malloc(bytes);if(!room)return 0;
    int ok=bytes>0x45594 && pe_disc_read(disc,&archive,(u64)(range.start+range.scratchSectors+range.textureSectors)*2048,room,bytes);
    if(ok){
        SceneRoomDirectory *dir=SceneAsset_ResolveOffset(room,room->header.directoryOffset);SceneRoomRecord *records=SceneAsset_ResolveOffset(room,dir->scripts&0x3fffff);
        if(memcmp(g_RoomM350OrbTableFlashEntries,(u8 *)room+0x8019a4d4u-0x8018efe8u,52))ok=0;
        const unsigned slots[]={75,70,155,156,73,158,161,159,160};const u32 addresses[]={0x8019a350,0x8019a318,0x8019a468,0x8019a508,0x8019a388,0x8019a5a0,0x8019a73c,0x8019a638,0x8019a6a0};
        if(dir->scripts>>22!=9)ok=0;
        for(unsigned i=0;i<9 && ok;++i)if(records[i].source.bytes.slot!=slots[i] || records[i].u.handlerAddress!=addresses[i])ok=0;
        const struct {FieldFxTaskProgram *program;u32 address,script;size_t script_bytes;intptr_t (*init)(int,intptr_t,intptr_t,intptr_t);} programs[]={
            {&g_RoomM350HandEffects,0x8019a468,0x8019a3d4,60,RoomM350_HandConfigure},{&g_RoomM350OrbEffects,0x8019a508,0x8019a4a0,60,RoomM350_OrbConfigure},
            {&g_RoomM350AreaEffects,0x8019a5a0,0x8019a540,44,RoomM350_AreaConfigure},{&g_RoomM350DropEffects,0x8019a638,0x8019a5d8,60,RoomM350_DropConfigure},
            {&g_RoomM350RushEffects,0x8019a6a0,0x8019a670,32,RoomM350_RushConfigure},{&g_RoomM350BodyEffects,0x8019a73c,0x8019a710,36,NULL}};
        const struct {u32 address;FieldFxTaskCallback fn;} callbacks[]={
{0x80196e04,RoomM350_AreaController},{0x80197364,RoomM350_DropEmitter},{0x801976c8,RoomM350_DropHitEmitter},{0x8019784c,RoomM350_DropTrailEmitter},{0x80197b98,RoomM350_DropBurstEmitter},{0x80197da8,RoomM350_DropController},{0x80198400,RoomM350_RushFlareEmitter},{0x80198abc,RoomM350_RushCloudEmitter},{0x80198cf8,RoomM350_RushController},{0x801992a0,RoomM350_BodyDebrisEmitter},{0x8019988c,RoomM350_BodyRingEmitter},{0x80199d50,RoomM350_AreaPlayerPulseEmitter},{0x80199e50,RoomM350_DropFlareEmitter},{0x80199f2c,RoomM350_DropSplashEmitter},{0x80192508,RoomM350_HandSparkEmitter},{0x801928d4,RoomM350_HandPulseEmitter},{0x80192c34,RoomM350_HandGlowEmitter},{0x8019360c,RoomM350_HandSweepTrapEmitter},{0x801938e4,RoomM350_HandSweepHitEmitter},{0x80193bcc,RoomM350_HandFlashEmitter},{0x80193d6c,RoomM350_HandSweepController},{0x8019421c,RoomM350_OrbHandSparkEmitter},{0x80194654,RoomM350_OrbBurstEmitter},{0x80194cfc,RoomM350_OrbEmitter},{0x80195064,RoomM350_OrbImpactEmitter},{0x80195378,RoomM350_OrbTrailEmitter},{0x8019569c,RoomM350_OrbTableFlashEmitter},{0x801958dc,RoomM350_OrbController},{0x80195ce0,RoomM350_AreaSparkEmitter},{0x801960c4,RoomM350_AreaZoneEmitter},{0x80196a14,RoomM350_AreaDropletEmitter}};
        for(unsigned i=0;i<6;++i){
            u32 raw[14];memcpy(raw,(u8 *)room+programs[i].address-0x8018efe8,sizeof(raw));FieldFxTaskProgram *p=programs[i].program;
            if(raw[12]!=(programs[i].init?((u32[]){0x80199b0c,0x80199ca4,0x80199d2c,0x80199e3c,0x8019a120,0})[i]:0) || raw[13]!=programs[i].script || p->initialize!=programs[i].init || memcmp(p->script,(u8 *)room+programs[i].script-0x8018efe8,programs[i].script_bytes))ok=0;
            for(unsigned j=0;j<8;++j){
                FieldFxTaskCallback expected=NULL;for(unsigned k=0;k<sizeof(callbacks)/sizeof(callbacks[0]);++k)if(raw[j]==callbacks[k].address)expected=callbacks[k].fn;
                u16 sizes[8];memcpy(sizes,raw+8,sizeof(sizes));u16 size=sizes[j];if(i==2 && j==1)size=sizeof(void *)+16;
                if((raw[j] && !expected) || p->table.callbacks[j]!=expected || p->table.sizes[j]!=size)ok=0;
            }
        }
    }
    /* Compare every instruction with the already native theater actor library.
     * Only its local J/JAL targets and function-address low halves relocate. */
    if(ok){
        SceneRoomSectorRange canonical={0};
        if(!pe_disc_read(disc,exe,0x800ull+0x80093378u-0x80010000u+21*8,&canonical,8))ok=0;
        size_t size=(size_t)canonical.roomSectors*2048;u8 *data=ok?malloc(size):NULL;
        if(!data || size<0x35a8 || !pe_disc_read(disc,&archive,(u64)(canonical.start+canonical.scratchSectors+canonical.textureSectors)*2048,data,size))ok=0;
        unsigned jumps=0,addresses=0;
        for(unsigned at=0;ok && at<13084;at+=4){
            u32 a,b;memcpy(&a,data+0x1e4+at,4);memcpy(&b,(u8 *)room+0x204+at,4);
            if(a==b)continue;
            unsigned op=a>>26;u32 target=0x80000000u+((a&0x3ffffffu)<<2);
            if((op==2 || op==3) && b==a+8 && target>=0x8018f1ccu && target<0x801924e8u){++jumps;continue;}
            target=0x80190000u+(s16)a;
            if(op==9 && (a&0xffff0000u)==(b&0xffff0000u) && (u16)b==(u16)(a+32) && target>=0x8018f1ccu && target<0x801924e8u){++addresses;continue;}
            fprintf(stderr,"M350 shared instruction mismatch at %x: %08x/%08x\n",at,a,b);ok=0;
        }
        for(unsigned i=0;ok && i<42;++i){u32 a,b;memcpy(&a,data+0x3500+i*4,4);memcpy(&b,(u8 *)room+0xb330+i*4,4);if(!((!a && !b) || b==a+32))ok=0;}
        if(jumps!=111 || addresses!=20)ok=0;
        free(data);if(ok)puts("Original M350 shared actors: all 13084 code bytes and 42 descriptor pointers equal canonical USA theater library after exact local relocations");
    }
    if(ok)ok=original_m350_programs(room);
    free(room);if(ok)puts("Original M350 assets: all nine USA room descriptors, six complete native program tables and all six authored scripts match");return ok;
}

static int original_m349_states(void)
{
    const int modes[]={0,1,3},timers[]={0,7,8,31,32,47,48,32767,65535};
    const int velocities[]={-32768,-1,0,32767},floors[]={-100,0,100};
    const u32 seeds[]={0,1,0x12345678,0xffffffff};
    u64 hash=UINT64_C(14695981039346656037);unsigned count=0;
    RoomFloorY saved_floor=*g_RoomFloorY;u32 saved_random=Psyz_RandState();
    for(unsigned m=0;m<3;++m)for(unsigned state=0;state<4;++state)for(unsigned t=0;t<9;++t)
    for(unsigned v=0;v<4;++v)for(unsigned f=0;f<3;++f)for(unsigned seed=0;seed<4;++seed){
        _Alignas(void *) u8 data[36];memset(data,0xa5,sizeof(data));
        u16 words[]={123,(u16)-77,456,65530,(u16)velocities[v],(u16)velocities[v],(u16)velocities[v],8192,(u16)state,(u16)timers[t]};
        memcpy(data+8,words,20);g_RoomFloorY->y=floors[f];srand(seeds[seed]);
        u32 result=RoomM349_FlareSparkCallback(modes[m],data+8,0),random=Psyz_RandState();
        hash=m349_hash_bytes(hash,&result,4);hash=m349_hash_bytes(hash,data,sizeof(data));hash=m349_hash_bytes(hash,&random,4);++count;
    }
    if(count!=5184 || hash!=UINT64_C(0x136e4488b7700df5)){
        fprintf(stderr,"M349 spark CPU mismatch: %u %016llx\n",count,(unsigned long long)hash);return 0;}
    RoomM349FlareParams saved_params=g_RoomM349FlareParams;
    const int commands[]={0,1,2,3,255};const s32 xs[]={INT32_MIN,-32769,-1,0,1,2,32768,INT32_MAX},yz[]={-32769,0,32768};
    hash=UINT64_C(14695981039346656037);count=0;
    for(unsigned c=0;c<5;++c)for(unsigned x=0;x<8;++x)for(unsigned y=0;y<3;++y)for(unsigned z=0;z<3;++z){
        for(unsigned i=0;i<20;++i)((u8 *)&g_RoomM349FlareParams)[i]=i;
        if(RoomM349_SetFlareParams(commands[c],xs[x],yz[y],yz[z])!=(intptr_t)&g_RoomM349FlareParams)return 0;
        hash=m349_hash_bytes(hash,&g_RoomM349FlareParams,20);++count;
    }
    if(count!=360 || hash!=UINT64_C(0xc535a7a10a851f23))return 0;
    FieldFxObjectPrefix *saved_owner=g_FieldFxOwner;FieldFxTaskSlot *saved_slot=g_FieldFxActiveTaskSlot;
    FieldFxTaskContext *saved_context=g_FieldFxTaskContext;int saved_age=g_FieldFxCurrentAge;
    FieldActor actor={0};RenderMatrix matrix={0};FieldFxObjectPrefix owner={0};FieldFxTaskSlot task={0};
    actor.render_object.matrices=&matrix;matrix.translation[0]=123;matrix.translation[1]=-400;matrix.translation[2]=-77;
    owner.actor=&actor;g_FieldFxOwner=&owner;g_FieldFxActiveTaskSlot=&task;g_FieldFxTaskContext=NULL;
    const int periods[]={2,3,8},ages[]={0,1,2,3,4,8,15,16,17,24,40,41};
    const s32 radii[]={INT32_MIN,-1,250,INT32_MAX},last_frames[]={-1,40,INT32_MAX};
    hash=UINT64_C(14695981039346656037);count=0;
    for(unsigned seed=0;seed<4;++seed)for(unsigned period=0;period<3;++period)for(unsigned age=0;age<12;++age)for(unsigned floor=0;floor<2;++floor)
    for(unsigned radius=0;radius<4;++radius)for(unsigned last=0;last<3;++last){
        _Alignas(void *) char pool[4096];_Alignas(void *) u8 target[12],start_guard[16];
        memset(pool,0xa5,sizeof(pool));memset(target,0xa5,sizeof(target));memset(start_guard,0xa5,sizeof(start_guard));
        s16 sound=-1;memcpy(target+8,&sound,2);task.start=(char *)start_guard;task.end=pool;
        RoomM349FlareParams params={77,-250,-333,0,radii[radius],periods[period],last_frames[last]};
        g_RoomFloorY->y=floor?100:-100;g_FieldFxCurrentAge=ages[age];srand(seeds[seed]);
        int bytes=RoomM349_FlareRing(0,target,(intptr_t)&params);FieldFxCallbackList *list=(void *)pool;
        if(bytes<=0 || bytes>sizeof(pool) || list->count!=70 || list->callback!=RoomM349_FlareSparkCallback)return 0;
        u32 result=RoomM349_FlareRing(1,target,(intptr_t)&params),random=Psyz_RandState();
        hash=m349_hash_bytes(hash,&result,4);hash=m349_hash_bytes(hash,target,12);hash=m349_hash_bytes(hash,&random,4);
        for(unsigned i=0;i<70;++i){
            FieldFxCallbackListEntry *entry=(void *)(list->entries+i*list->stride);
            hash=m349_hash_bytes(hash,entry,4);hash=m349_hash_bytes(hash,entry->state,20);
        }
        for(unsigned i=0;i<sizeof(start_guard);++i)if(start_guard[i]!=0xa5)return 0;
        ++count;
    }
    g_FieldFxOwner=saved_owner;g_FieldFxActiveTaskSlot=saved_slot;g_FieldFxTaskContext=saved_context;g_FieldFxCurrentAge=saved_age;
    *g_RoomFloorY=saved_floor;srand(saved_random);g_RoomM349FlareParams=saved_params;
    if(count!=3456 || hash!=UINT64_C(0x3d361c0198251f29)){
        fprintf(stderr,"M349 ring CPU mismatch: %u %016llx\n",count,(unsigned long long)hash);return 0;}
    puts("Original M349: 5184 spark states with real BIOS RNG, 360 setter cases and 3456 ring/pool updates match unhooked USA CPU; full-width argument pointer and task-start guards passed");
    return 1;
}

static int original_m349_program(SceneAssetView *room)
{
    PmTables old_tables=g_PmTables;Pe1GameState old_game=g_GameState;u32 old_flags=g_GameStateFlags;
    FieldFxTaskProgram *old_program=g_FieldFxTaskPrograms[96];RoomM349FlareParams old_params=g_RoomM349FlareParams;
    FieldFxObjectPrefix *old_owner=g_FieldFxOwner;FieldFxTaskContext *old_context=g_FieldFxTaskContext;
    FieldFxTaskSlot *old_slot=g_FieldFxActiveTaskSlot;int old_age=g_FieldFxCurrentAge;
    RenderBufferPrefix old_buffers=D_800B0E38;int old_draw=g_ActiveDrawSlot,old_offset=g_DrawPacketOffset;
    GteMatrix *old_view=g_GeomScroll.words.viewMatrix;int *old_projection=g_GeomScroll.words.projectionDistance;
    u32 old_rng=Psyz_RandState();RoomFloorY old_floor=*g_RoomFloorY;
    AkaoQueueEntry old_audio[16];memcpy(old_audio,g_AkaoMessageQueue,sizeof(old_audio));int old_count=g_AkaoMessageQueueCount;
    static RenderGpuTag ordering[4096];static struct {_Alignas(void *) u8 data[262144];u8 guard[32];} packets;
    GteMatrix view={.m={{4096,0,0},{0,4096,0},{0,0,4096}}};int projection=256,ok=1;unsigned cycles=0;
    g_GeomScroll.words.viewMatrix=&view;g_GeomScroll.words.projectionDistance=&projection;g_FieldFxTaskPrograms[96]=&g_RoomM349FlareEffects;
    g_GameState.loaded_scene_archive=(void *)room;
    g_GameStateFlags=0x80;g_GameState.flags&=~8u;g_GameState.tim_load_state=0;g_RoomFloorY->y=0;
    FieldFx_InitTexturePageTable();Psyz_GteCtrlWrite(26,256);Psyz_GteCtrlWrite(29,1365);Psyz_GteCtrlWrite(30,1024);gte_SetGeomOffset(160,112);
    for(unsigned variant=0;ok && variant<2;++variant)for(unsigned draw=0;ok && draw<2;++draw){
        struct {PmPrimarySlot slots[11];u8 guard[32];} bank={0};memset(bank.guard,0xa5,32);
        FieldActor actor={0};RenderMatrix matrices[14]={0};actor.render_object.matrices=matrices;
        for(unsigned j=0;j<14;++j){for(unsigned k=0;k<3;++k)matrices[j].rotation[k][k]=4096;matrices[j].translation[2]=1024;}
        g_PmTables=(PmTables){g_PmCmdHandlerArray,bank.slots,NULL};srand(variant+1);
        if(Scene_LoadRoomAssets(181,&actor)!=0 || Pm_SendCmd(0,0,1,0,-100,1024) ||
           Pm_SendCmd(0,0,2,250,40,0) || Pm_SendCmd(0,0,0,variant?3:2,0,0)){ok=0;break;}
        unsigned frame=0,drawn=0,children=0,seen=0;
        for(;frame<240;++frame){
            for(unsigned j=0;j<4096;++j)ordering[j]=(RenderGpuTag){.address=0xffffff};
            memset(packets.guard,0xa5,32);g_ActiveDrawSlot=draw;g_DrawPacketOffset=0;
            D_800B0E38.ordering[draw]=ordering;D_800B0E38.packets[draw]=(char *)packets.data;
            if(Pm_Start(0) || Pm_Exec(0)){ok=0;break;}
            if(g_AkaoMessageQueueCount<0 || g_AkaoMessageQueueCount>16){ok=0;break;}
            /* This graphics fixture observes staged audio without an audio timer. */
            g_AkaoMessageQueueCount=0;drawn+=g_DrawPacketOffset!=0;FieldFxTaskContext *ctx=&bank.slots[0].task.tasks;
            for(unsigned k=0;k<8;++k)if(ctx->slots[k].id!=0xffff){
                if(ctx->slots[k].id!=0 || !ctx->slots[k].end){ok=0;break;}seen=1;
                FieldFxCallbackList *list=(void *)ctx->slots[k].end;
                for(unsigned j=0;j<(unsigned)list->count;++j)children+=((FieldFxCallbackListEntry *)(list->entries+j*list->stride))->active!=0;
            }
            if(ctx->used>sizeof(ctx->arena) || g_DrawPacketOffset<0 || g_DrawPacketOffset>sizeof(packets.data)){ok=0;break;}
            for(unsigned j=0;j<4096 && ok;++j){uintptr_t link=ordering[j].address;unsigned count=0;
                while(link && link!=0xffffff){if(link<(uintptr_t)packets.data || link+sizeof(RenderGpuTag)>(uintptr_t)packets.data+g_DrawPacketOffset || ++count>8192){ok=0;break;}link=((RenderGpuTag *)link)->address;}
            }
            for(unsigned j=0;j<32;++j)if(bank.guard[j]!=0xa5 || packets.guard[j]!=0xa5)ok=0;
            if(!ok || !bank.slots[0].header.state)break;
        }
        if(!ok || frame==240 || !drawn || !children || !seen || bank.slots[0].header.state || bank.slots[0].task.tasks.count){
            fprintf(stderr,"M349 PM %u/%u frames%u drawn%u children%u state%u\n",variant,draw,frame,drawn,children,bank.slots[0].header.state);ok=0;}
        if(ok)++cycles;
    }
    g_PmTables=old_tables;g_GameState=old_game;g_GameStateFlags=old_flags;g_FieldFxTaskPrograms[96]=old_program;g_RoomM349FlareParams=old_params;
    g_FieldFxOwner=old_owner;g_FieldFxTaskContext=old_context;g_FieldFxActiveTaskSlot=old_slot;g_FieldFxCurrentAge=old_age;
    D_800B0E38=old_buffers;g_ActiveDrawSlot=old_draw;g_DrawPacketOffset=old_offset;g_GeomScroll.words.viewMatrix=old_view;g_GeomScroll.words.projectionDistance=old_projection;
    srand(old_rng);*g_RoomFloorY=old_floor;memcpy(g_AkaoMessageQueue,old_audio,sizeof(old_audio));g_AkaoMessageQueueCount=old_count;
    if(ok)printf("Original M349 PM: %u complete authored cycles, both periods/draw slots, joint glows and particle rendering, natural expiry and packet/OT/pool guards passed\n",cycles);
    return ok;
}

static int original_m349_assets(PeDisc *disc,PeDiscFile *exe)
{
    SceneRoomSectorRange range;PeDiscFile archive;
    if(!pe_disc_find(disc,"PE.IMG",&archive) || !pe_disc_read(disc,exe,0x800ull+0x80093378u-0x80010000u+348*8,&range,8))return 0;
    size_t bytes=(size_t)range.roomSectors*2048;SceneAssetView *room=malloc(bytes);if(!room)return 0;
    int ok=bytes>0x398d4 && pe_disc_read(disc,&archive,(u64)(range.start+range.scratchSectors+range.textureSectors)*2048,room,bytes);
    if(ok){
        SceneRoomDirectory *dir=SceneAsset_ResolveOffset(room,room->header.directoryOffset);
        SceneRoomRecord *record=SceneAsset_ResolveOffset(room,dir->scripts&0x3fffff);
        ok=dir->scripts>>22==1 && record->source.bytes.slot==181 && record->u.handlerAddress==0x8019015c;
        const struct {u32 address;const void *data;size_t bytes;} data[]={
            {0x8018eff4,&g_RoomM349SparkRotationBase,8},{0x8018effc,&g_RoomM349FlareColor,4},
            {0x8018f000,&g_RoomM349JointGlowAnchorNegY,8},{0x8018f008,&g_RoomM349JointGlowAnchorPosY,8},
            {0x80190138,g_RoomM349FlareScript,36}};
        for(unsigned i=0;i<sizeof(data)/sizeof(data[0]);++i)
            if(memcmp((u8 *)room+data[i].address-0x8018efe8,data[i].data,data[i].bytes))ok=0;
        u32 original[14];memcpy(original,(u8 *)room+0x8019015c-0x8018efe8,sizeof(original));
        if(original[0]!=0x8018f7c4 || original[8]!=12 || original[12]!=0x801900d0 || original[13]!=0x80190138)ok=0;
        for(unsigned i=1;i<12;++i)if(i!=8 && original[i])ok=0;
        FieldFxTaskProgram *p=&g_RoomM349FlareEffects;
        if(p->table.callbacks[0]!=RoomM349_FlareRing || p->table.sizes[0]!=12 || p->initialize!=RoomM349_SetFlareParams || p->script!=g_RoomM349FlareScript)ok=0;
        for(unsigned i=1;i<8;++i)if(p->table.callbacks[i] || p->table.sizes[i])ok=0;
    }
    if(ok)ok=original_m349_program(room);
    free(room);
    if(ok)puts("Original M349 assets: actual USA room command181, all task callbacks/sizes, initializer, script and all four constants match");
    return ok;
}

static int original_m123_pulse_contact(void)
{
    FieldFxObjectPrefix *old_owner=g_FieldFxOwner;FieldFxTaskContext *old_context=g_FieldFxTaskContext;
    FieldActor *old_player=g_PlayerEntity;int old_age=g_FieldFxCurrentAge,valid=1;unsigned cases=0;
    FieldActor actor={0},player={0};FieldActorState core={0};Combatant player_core={0};
    FieldFxObjectPrefix owner={0};FieldFxTaskContext context={0};
    actor.state=&core;player.state=(void *)&player_core;owner.actor=&actor;
    g_FieldFxOwner=&owner;g_FieldFxTaskContext=&context;g_PlayerEntity=&player;
    const unsigned modes[]={1,3},ages[]={8,31,32,33,48};
    const u32 flags[]={0,0x1000000,0x1000042,0x11000000,0x21000000,0x81000000,0xc1000000,0xffffffff};
    const u32 player_flags[]={0,0x200,0xa5a55a5a};u64 hash=UINT64_C(14695981039346656037);
    for(unsigned m=0;m<2;++m)for(unsigned a=0;a<5;++a)for(unsigned held=0;held<2;++held)
    for(unsigned f=0;f<8;++f)for(unsigned p=0;p<3;++p){
        struct {_Alignas(void *) u8 bytes[32];} data;memset(data.bytes,0xa5,sizeof(data.bytes));
        g_FieldFxCurrentAge=ages[a];context.flags=held;core.core_flags=flags[f];player_core.stateFlags=player_flags[p];
        u32 result=RoomM123_PulseRing(modes[m],data.bytes+8,0);
        u32 words[]={result,core.core_flags,player_core.stateFlags};
        for(unsigned i=0;i<3;++i)for(unsigned b=0;b<4;++b)
            hash=(hash^((words[i]>>(8*b))&255))*UINT64_C(1099511628211);
        for(unsigned b=0;b<sizeof(data.bytes);++b)hash=(hash^data.bytes[b])*UINT64_C(1099511628211);
        ++cases;
    }
    if(cases!=480 || hash!=UINT64_C(0x92f9f09848a02f45)){
        fprintf(stderr,"M123 pulse contact CPU hash %u %016llx\n",cases,(unsigned long long)hash);valid=0;}
    for(unsigned missing=0;missing<2;++missing){
        u16 particle[6]={0};owner.actor=missing?NULL:&actor;actor.state=NULL;
        context.flags=1;g_FieldFxCurrentAge=32;core.core_flags=0xa5a55a5a;player_core.stateFlags=0x200;
        if(RoomM123_PulseRing(1,&particle,0)!=2 || context.flags!=1 ||
           core.core_flags!=0xa5a55a5a || player_core.stateFlags!=0x200)valid=0;
    }
    g_FieldFxOwner=old_owner;g_FieldFxTaskContext=old_context;g_PlayerEntity=old_player;g_FieldFxCurrentAge=old_age;
    if(valid)puts("Original M123 pulse contact: 480 unhooked USA CPU age/held/core/player states, full payload guards, plus absent actor/core preserve visual completion without combat writes passed");
    return valid;
}

static int original_m123_programs(SceneAssetView *room)
{
    AkaoQueueEntry old_audio[16];memcpy(old_audio,g_AkaoMessageQueue,sizeof(old_audio));
    int old_queued=g_AkaoMessageQueueCount;unsigned sound_messages=0;
    g_AkaoMessageQueueCount=0;
    PmTables old_tables=g_PmTables;Pe1GameState old_game=g_GameState;u32 old_flags=g_GameStateFlags;
    const unsigned indices[]={43,58,59};void *old_programs[3];for(unsigned i=0;i<3;++i)old_programs[i]=g_FieldFxTaskPrograms[indices[i]];
    FieldFxTaskContext *old_context=g_FieldFxTaskContext;FieldFxTaskSlot *old_task=g_FieldFxActiveTaskSlot;
    FieldFxObjectPrefix *old_owner=g_FieldFxOwner;FieldActor *old_player=g_PlayerEntity;
    RenderBufferPrefix old_buffers=D_800B0E38;GteMatrix *old_view=g_GeomScroll.words.viewMatrix;
    int old_age=g_FieldFxCurrentAge,old_draw=g_ActiveDrawSlot,old_offset=g_DrawPacketOffset,old_floor=g_RoomFloorY->y;
    u32 old_random=Psyz_RandState();s16 old_record=g_RoomScriptRecordWords[0];int old_arg=g_RoomScriptArg;
    u8 *old_models[]={g_RoomM123BeaconModel0,g_RoomM123BeaconModel1};void *old_selected=g_RoomM123SelectedColorTrack;
    RoomM123JointAngles old_angles=g_RoomM123GlowBurstAngles;GteShortVector old_center=g_RoomM123GlowBurstCenter;
    s16 old_speed=g_RoomM123SwirlSpeed,old_brightness=g_RoomM123SwirlBrightness;
    u8 old_swirl_angles[8];u16 old_origin[3];memcpy(old_swirl_angles,g_RoomM123SwirlAngles,8);memcpy(old_origin,g_RoomM123SwirlOrigin,6);
    struct {void *data;unsigned size;u8 saved[48];} tracks[]={
        {g_RoomM123GlowColorTrack,40},{g_RoomM123GlowBurstColorTrack,40},{g_RoomM123BeaconColorTrack,48},
        {g_RoomM123PulseColorTrack0,32},{g_RoomM123PulseColorTrack1,32},{g_RoomM123WaveColorTrack,40}};
    for(unsigned i=0;i<6;++i)memcpy(tracks[i].saved,tracks[i].data,tracks[i].size);
    static RenderGpuTag ordering[4096];static struct {_Alignas(void *) u8 bytes[524288];u8 guard[32];} packets;
    GteMatrix view={.m={{4096,0,0},{0,4096,0},{0,0,4096}}};int valid=1;unsigned cycles=0;
    g_GeomScroll.words.viewMatrix=&view;g_RoomFloorY->y=0;g_GameState.loaded_scene_archive=(void *)room;
    g_GameStateFlags=0x80;g_GameState.flags&=~8u;g_GameState.tim_load_state=0;
    FieldFxTaskProgram *programs[]={&g_RoomM123JointEffects,&g_RoomM123PulseRingEffects,&g_RoomM123WaveBurstEffects};
    for(unsigned i=0;i<3;++i)g_FieldFxTaskPrograms[indices[i]]=programs[i];
    FieldFx_InitTexturePageTable();Psyz_GteCtrlWrite(26,256);Psyz_GteCtrlWrite(29,1365);Psyz_GteCtrlWrite(30,1024);gte_SetGeomOffset(160,112);
    for(unsigned program=0;valid && program<3;++program)for(unsigned variant=0;valid && variant<(program?2u:1u);++variant)
    for(unsigned draw=0;valid && draw<2;++draw){
        struct {PmPrimarySlot slots[11];u8 guard[32];} bank={0};memset(bank.guard,0xa5,32);
        FieldActor actor={0},player={0};FieldActorState state={0};Combatant player_state={0};RenderMatrix matrices[48]={0};u8 action=1;
        actor.state=&state;actor.render_object.matrices=matrices;state.action_state=&action;state.core_flags=0x01000000;
        actor.pos_x=100*65536;actor.pos_y=-800*65536;actor.pos_z=1200*65536;
        player.state=(void *)&player_state;player.pos_x=8000*65536;player.pos_z=8000*65536;g_PlayerEntity=&player;
        for(unsigned j=0;j<48;++j){for(unsigned k=0;k<3;++k)matrices[j].rotation[k][k]=4096;matrices[j].translation[0]=100;matrices[j].translation[1]=-800;matrices[j].translation[2]=1200;}
        g_PmTables=(PmTables){g_PmCmdHandlerArray,bank.slots,NULL};srand(1);
        if(Scene_LoadRoomAssets(85+indices[program],&actor)!=0 || Pm_SendCmd(0,0,1,variant,0,0)){valid=0;break;}
        unsigned frames=0,draws=0,children=0,seen=0;
        for(;frames<360;++frames){
            for(unsigned j=0;j<4096;++j)ordering[j]=(RenderGpuTag){.address=0xffffff};
            memset(packets.guard,0xa5,32);g_ActiveDrawSlot=draw;g_DrawPacketOffset=0;
            D_800B0E38.ordering[draw]=ordering;D_800B0E38.packets[draw]=(char *)packets.bytes;
            if(Pm_Start(0) || Pm_Exec(0)){valid=0;break;}
            /* This graphics fixture has no AKAO timer or sample bank. Observe
             * and consume its staged commands each frame instead of allowing
             * hundreds of simulated frames to overflow the real audio queue. */
            if(g_AkaoMessageQueueCount<0 || g_AkaoMessageQueueCount>16){valid=0;break;}
            sound_messages+=(unsigned)g_AkaoMessageQueueCount;g_AkaoMessageQueueCount=0;
            draws+=g_DrawPacketOffset!=0;FieldFxTaskContext *ctx=&bank.slots[0].task.tasks;
            for(unsigned slot=0;slot<8;++slot)if(ctx->slots[slot].id!=0xffff){
                seen|=1u<<ctx->slots[slot].id;
                if(!ctx->slots[slot].end || (program==0 && ctx->slots[slot].id==2))continue;
                FieldFxCallbackList *list=(void *)ctx->slots[slot].end;
                for(unsigned j=0;j<(unsigned)list->count;++j)children+=((FieldFxCallbackListEntry *)(list->entries+j*list->stride))->active!=0;
            }
            if(ctx->used>sizeof(ctx->arena) || g_DrawPacketOffset<0 || g_DrawPacketOffset>sizeof(packets.bytes)){valid=0;break;}
            for(unsigned j=0;j<4096;++j){uintptr_t link=ordering[j].address;unsigned links=0;
                while(link && link!=0xffffff){if(link<(uintptr_t)packets.bytes || link+sizeof(RenderGpuTag)>(uintptr_t)packets.bytes+g_DrawPacketOffset || ++links>8192){valid=0;break;}link=((RenderGpuTag *)link)->address;}
                if(!valid)break;
            }
            for(unsigned j=0;j<32;++j)if(bank.guard[j]!=0xa5 || packets.guard[j]!=0xa5)valid=0;
            if(!valid || !bank.slots[0].header.state)break;
        }
        if(frames==360 || !draws || !children || seen!=(program?1u:15u) || bank.slots[0].header.state || bank.slots[0].task.tasks.count){fprintf(stderr,"M123 PM %u/%u/%u frames%u draws%u children%u seen%x state%u count%u\n",program,variant,draw,frames,draws,children,seen,bank.slots[0].header.state,bank.slots[0].task.tasks.count);valid=0;}
        if(valid)++cycles;
    }
    g_PmTables=old_tables;g_GameState=old_game;g_GameStateFlags=old_flags;for(unsigned i=0;i<3;++i)g_FieldFxTaskPrograms[indices[i]]=old_programs[i];
    g_FieldFxTaskContext=old_context;g_FieldFxActiveTaskSlot=old_task;g_FieldFxOwner=old_owner;g_PlayerEntity=old_player;
    D_800B0E38=old_buffers;g_GeomScroll.words.viewMatrix=old_view;g_FieldFxCurrentAge=old_age;g_ActiveDrawSlot=old_draw;g_DrawPacketOffset=old_offset;g_RoomFloorY->y=old_floor;srand(old_random);
    g_RoomScriptRecordWords[0]=old_record;g_RoomScriptArg=old_arg;g_RoomM123SelectedColorTrack=old_selected;
    g_RoomM123BeaconModel0=old_models[0];g_RoomM123BeaconModel1=old_models[1];g_RoomM123GlowBurstAngles=old_angles;g_RoomM123GlowBurstCenter=old_center;
    g_RoomM123SwirlSpeed=old_speed;g_RoomM123SwirlBrightness=old_brightness;memcpy(g_RoomM123SwirlAngles,old_swirl_angles,8);memcpy(g_RoomM123SwirlOrigin,old_origin,6);
    for(unsigned i=0;i<6;++i)memcpy(tracks[i].data,tracks[i].saved,tracks[i].size);
    memcpy(g_AkaoMessageQueue,old_audio,sizeof(old_audio));g_AkaoMessageQueueCount=old_queued;
    if(!sound_messages)valid=0;
    if(valid)printf("Original M123 PM: %u full authored cycles, three programs and both selector variants, all eleven callbacks, actual USA models, both draw slots, natural expiry and packet/OT/arena guards passed\n",cycles);
    return valid;
}

static int original_m123_ai(SceneAssetView *room,size_t bytes)
{
    const unsigned offset=228736,length=19076;
    if(bytes<offset+length || m349_hash_bytes(UINT64_C(14695981039346656037),(u8 *)room+offset,length)!=UINT64_C(0xe57fffe71a31afec))return 0;
    u8 *script=malloc(length);if(!script)return 0;memcpy(script,(u8 *)room+offset,length);
    /* Observe the chosen attack boundary, without executing the attack. Only
     * these post-decision entry points become the existing FREE-node opcode;
     * all decision bytes, dispatcher and opcode handlers remain unchanged. */
    const unsigned targets[]={10808,11624,12000};const u32 stop[]={32,0};
    for(unsigned i=0;i<3;++i)memcpy(script+targets[i],stop,sizeof(stop));
    FieldActor *old_current=g_CurrentEntity,*old_player=g_PlayerEntity,*old_head=g_FieldActorListHead;
    ScriptNode *old_node=g_ScriptNodePool;ScriptCursorValue old_cursor=g_ScriptCursorValue;
    extern int *g_DrawGpuReg2;int *old_rng_tail=g_DrawGpuReg2;u32 old_frame_rng[33];memcpy(old_frame_rng,g_DrawPrimGeneratorWords,sizeof(old_frame_rng));
    u32 old_flags=g_GameStateFlags,old_operands[16];int old_active[64];
    memcpy(old_operands,g_ScriptOperandTable,sizeof(old_operands));memcpy(old_active,g_ScriptNodeActiveFlags,sizeof(old_active));
    FieldActor actor={.type_id=1,.script_base=script},player={0};Combatant core={0};ScriptNode node={0};
    player.state=(void *)&core;actor.next=&player;g_CurrentEntity=&actor;g_PlayerEntity=&player;g_FieldActorListHead=&actor;g_GameStateFlags=2;
    const unsigned hps[]={1,25,26,76,94,95,97,98,100,189,195},maxima[]={189,195},atbs[]={0,4499,4500,4501,8999,9000};
    u64 hash=UINT64_C(14695981039346656037);unsigned cases=0,seen=0;int ok=1;
    for(unsigned test=0;ok && test<296;++test){
        unsigned entry,draws,hp,maximum,barrier,atb;
        if(test<32){entry=6984;draws=test;hp=1;maximum=195;barrier=1;atb=9000;}
        else {unsigned n=test-32;entry=9392;draws=0;atb=atbs[n%6];n/=6;barrier=n%2;n/=2;maximum=maxima[n%2];n/=2;hp=hps[n];}
        memset(actor.pad_0AC,0,sizeof(actor.pad_0AC));memset(g_ScriptOperandTable,0,sizeof(old_operands));memset(g_ScriptNodeActiveFlags,0,sizeof(old_active));
        g_ScriptNodeActiveFlags[21]=g_ScriptNodeActiveFlags[22]=1;
        core.curHP=hp;core.maxHP=maximum;core.hpAlive=atb;core.stateFlags=barrier?0x200:0;
        node=(ScriptNode){.current.script=(u32 *)(script+entry),.active=1};g_ScriptNodePool=&node;Draw_InitGpuHwRegs();for(unsigned draw=0;draw<draws;++draw)Draw_FlushPrimQueue();
        Script_RunQueue();
        uintptr_t destination=(uintptr_t)node.current.script-(uintptr_t)script-8;
        if(!(node.flags&0x10) || (entry==6984?(destination!=10808 && destination!=12000):destination!=(barrier || (2*hp>maximum && 2*atb>9000)?11624:12000)) ||
           core.curHP!=hp || core.maxHP!=maximum || core.hpAlive!=atb || core.stateFlags!=(barrier?0x200u:0u)){
            fprintf(stderr,"M123 AI case%u entry%u hp%u/%u barrier%u atb%u target%llu\n",test,entry,hp,maximum,barrier,atb,(unsigned long long)destination);ok=0;break;}
        if(entry==6984)seen|=destination==10808?1:2;
        u32 record[]={entry,draws,hp,maximum,barrier,atb,(u32)destination};hash=m349_hash_bytes(hash,record,sizeof(record));hash=m349_hash_bytes(hash,g_DrawPrimGeneratorWords,sizeof(old_frame_rng));++cases;
    }
    if(cases!=296 || seen!=3 || hash!=UINT64_C(0xbae565e87815a8ca)){fprintf(stderr,"M123 AI CPU mismatch %u/%016llx seen%u\n",cases,(unsigned long long)hash,seen);ok=0;}
    g_CurrentEntity=old_current;g_PlayerEntity=old_player;g_FieldActorListHead=old_head;g_ScriptNodePool=old_node;g_ScriptCursorValue=old_cursor;g_GameStateFlags=old_flags;
    memcpy(g_ScriptOperandTable,old_operands,sizeof(old_operands));memcpy(g_ScriptNodeActiveFlags,old_active,sizeof(old_active));memcpy(g_DrawPrimGeneratorWords,old_frame_rng,sizeof(old_frame_rng));g_DrawGpuReg2=old_rng_tail;free(script);
    if(ok)puts("Original M123 AI: 296 retail CPU decisions match through native dispatcher and unchanged handlers; last head3 chooses charge/beam, reactive tail uses Barrier or HP above half plus AT above half, post-decision sentinels only");
    return ok;
}

static int original_m123_assets(PeDisc *disc,PeDiscFile *exe)
{
    PeDiscFile archive;SceneRoomSectorRange ranges[2];SceneAssetView *rooms[2]={NULL,NULL};
    const unsigned ids[]={123,28};int ok=0;
    if(!pe_disc_find(disc,"PE.IMG",&archive))return 0;
    for(unsigned i=0;i<2;++i){
        if(!pe_disc_read(disc,exe,0x800+0x80093378u-0x80010000u+(ids[i]-1)*8,&ranges[i],8))goto done;
        size_t bytes=(size_t)ranges[i].roomSectors*2048;rooms[i]=malloc(bytes);
        if(!rooms[i] || !pe_disc_read(disc,&archive,(u64)(ranges[i].start+ranges[i].scratchSectors+ranges[i].textureSectors)*2048,rooms[i],bytes))goto done;
    }
    const u8 *module=(u8 *)rooms[0]+8,*reference=(u8 *)rooms[1]+8;
    u8 identity[32];
    if(!pe_disc_read(disc,exe,0x800+0x800c2270u-0x80010000u,identity,32) ||
       memcmp(identity,&g_FieldFxIdentityMatrix,32))goto done;
    SceneRoomDirectory *dir=SceneAsset_ResolveOffset(rooms[0],rooms[0]->header.directoryOffset);
    SceneRoomRecord *records=SceneAsset_ResolveOffset(rooms[0],dir->scripts&0x3fffff);
    const unsigned slots[]={73,143,128,144};const u32 descriptors[]={0x80195454,0x801955d4,0x8019553c,0x8019564c};
    if(dir->scripts>>22!=4)goto done;
    for(unsigned i=0;i<4;++i)if(records[i].source.bytes.slot!=slots[i] || records[i].u.handlerAddress!=descriptors[i] ||
        (records[i].source.offsetAndSlot&0xffffff)!=8 || records[i].reserved[0]!=0xd0 || records[i].reserved[1]!=0x66)goto done;
    const u32 methods[]={0x80191538,0x80191540,0x801915d0,0x80191818,0x80191820,0x801920c4,0x80192120};
    if(memcmp(module+0x80195454u-0x8018eff0u,methods,sizeof(methods)))goto done;
    unsigned relocated=0;
    for(unsigned offset=0;offset<0xbf0;offset+=4){
        u32 a,b;memcpy(&a,reference+0x80191514u-0x8018eff0u+offset,4);memcpy(&b,module+0x80191538u-0x8018eff0u+offset,4);
        if(a==b)continue;u32 expected=a;
        if(a>>26==2 || a>>26==3){u32 target=0x80000000u|((a&0x3ffffffu)<<2);
            if(target>=0x80191514u && target<0x80192104u)expected=(a&0xfc000000u)|(((target+0x24u)>>2)&0x3ffffffu);
        }else if(offset==0x3c || offset==0x200 || offset==0x244 || offset==0x370 || offset==0x710)
            expected=(a&0xffff0000u)|(u16)((u16)a+0x24u);
        if(b!=expected)goto done;++relocated;
    }
    if(relocated!=26)goto done;
    const struct {u32 address;const void *data;size_t size;} data[]={
        {0x8018f1cc,&g_RoomM123JointOffset,8},{0x8018f1d4,&g_RoomM123JointOrigin,8},
        {0x8018f1dc,&g_RoomM123TriangleColor,4},{0x8018f1e0,&g_RoomM123JointAnchor,8},
        {0x8018f1e8,&g_RoomM123BeaconAnchor,8},{0x8018f1f0,&g_RoomM123PulseRingTilt,8},
        {0x8018f1f8,&g_RoomM123WaveSpinBase,8},{0x801954bc,g_RoomM123GlowColorTrack,40},
        {0x801954e4,g_RoomM123GlowBurstColorTrack,40},{0x8019550c,g_RoomM123BeaconColorTrack,48},
        {0x80195594,g_RoomM123PulseColorTrack0,32},{0x801955b4,g_RoomM123PulseColorTrack1,32},
        {0x80195624,g_RoomM123WaveColorTrack,40},{0x8019548c,g_RoomM123JointScript,48},
        {0x80195574,g_RoomM123PulseRingScript,32},{0x8019560c,g_RoomM123WaveScript,24}};
    unsigned bytes=0;
    for(unsigned i=0;i<sizeof(data)/sizeof(data[0]);++i){if(memcmp(module+data[i].address-0x8018eff0u,data[i].data,data[i].size))goto done;bytes+=data[i].size;}
    FieldFxTaskProgram *programs[]={&g_RoomM123JointEffects,&g_RoomM123PulseRingEffects,&g_RoomM123WaveBurstEffects};
    const u32 tables[]={0x8019553c,0x801955d4,0x8019564c};
    const u32 addresses[3][8]={{0x801929fc,0x80192f0c,0x80193618,0x80194768},{0x80194c04},{0x80195114}};
    int (*callbacks[3][8])(int,void *,intptr_t)={{RoomM123_JointTriangleEmitter,RoomM123_GlowBurst,RoomM123_JointBeacon,RoomM123_JointSwirl},{RoomM123_PulseRing},{RoomM123_WaveBurst}};
    const u32 initializers[]={0x80195350,0x8019537c,0x801953d4},scripts[]={0x8019548c,0x80195574,0x8019560c};
    for(unsigned i=0;i<3;++i){const u8 *raw=module+tables[i]-0x8018eff0u;
        if(memcmp(raw,addresses[i],32) || memcmp(raw+32,programs[i]->table.sizes,16) ||
           memcmp(raw+48,&initializers[i],4) || memcmp(raw+52,&scripts[i],4))goto done;
        for(unsigned j=0;j<8;++j)if(programs[i]->table.callbacks[j]!=callbacks[i][j])goto done;
    }
    if(programs[0]->initialize!=RoomLib_SetTaskVar0 || programs[1]->initialize!=RoomM123_SelectSlotPointer ||
       programs[2]->initialize!=RoomLib_SetArg1)goto done;
    void *selected=g_RoomM123SelectedColorTrack;int old_arg=g_RoomScriptArg;
    const intptr_t values[]={0,1,2,255,-1,(intptr_t)UINT64_C(0x1234567800000001)};
    for(unsigned i=0;i<6;++i){
        intptr_t address=programs[1]->initialize(1,values[i],0,0);
        if((void **)address!=&g_RoomM123SelectedColorTrack || *(void **)address!=((u32)values[i]==1?(void *)g_RoomM123PulseColorTrack1:(void *)g_RoomM123PulseColorTrack0))goto done;
        address=programs[2]->initialize(1,values[i],0,0);
        if((int *)address!=&g_RoomScriptArg || *(int *)address!=(s32)values[i])goto done;
    }
    g_RoomM123SelectedColorTrack=selected;g_RoomScriptArg=old_arg;
    if(!original_m123_ai(rooms[0],(size_t)ranges[0].roomSectors*2048) || !original_m123_programs(rooms[0]))goto done;
    printf("Original M123 assets: four descriptors, complete class C with 26 relocations, %u original data bytes, three typed programs and native selector/argument round trips passed\n",bytes);ok=1;
done:
    for(unsigned i=0;i<2;++i)free(rooms[i]);return ok;
}

static int original_m028_assets(PeDisc *disc,PeDiscFile *exe)
{
    u8 header[0x800];u32 base;PeDiscFile archive;SceneAssetView *rooms[5]={NULL,NULL,NULL,NULL,NULL};
    if(!pe_disc_read(disc,exe,0,header,sizeof(header)) || !pe_disc_find(disc,"PE.IMG",&archive))return 0;
    memcpy(&base,header+0x18,4);const unsigned ids[]={12,28,32,34,348};int valid=1;
    for(unsigned i=0;valid && i<5;++i){
        SceneRoomSectorRange range;
        valid=pe_disc_read(disc,exe,0x800ull+0x80093378u-base+(ids[i]-1)*8,&range,sizeof(range));
        if(!valid)break;
        size_t bytes=(size_t)range.roomSectors*2048;
        if(!(rooms[i]=malloc(bytes))){valid=0;break;}
        valid=pe_disc_read(disc,&archive,(u64)(range.start+range.scratchSectors+range.textureSectors)*2048,rooms[i],bytes);
    }
    if(valid){
        SceneRoomDirectory *directory=SceneAsset_ResolveOffset(rooms[1],rooms[1]->header.directoryOffset);
        SceneRoomRecord *record=SceneAsset_ResolveOffset(rooms[1],directory->scripts&0x3fffff);
        valid=directory->scripts>>22==2 && record[0].source.bytes.slot==115 && record[0].u.handlerAddress==0x80193250u &&
            record[1].source.bytes.slot==73 && record[1].u.handlerAddress==0x801931dcu;
        const u32 descriptor[]={0x80192700,0,0,0,0,0,0,0,20,0,0,0,0x80193148,0x80193214};
        const u32 methods[]={0x80191514,0x8019151c,0x801915ac,0x801917f4,0x801917fc,0x801920a0,0x801920fc};
        const u8 *a=(u8 *)rooms[0]+8,*b=(u8 *)rooms[1]+8;
        valid=valid && !memcmp(b+0x80193250u-0x8018eff0u,descriptor,sizeof(descriptor)) &&
            !memcmp(b+0x801931dcu-0x8018eff0u,methods,sizeof(methods)) &&
            !memcmp(b+0x80193214u-0x8018eff0u,g_RoomOrbitFlareScript,sizeof(g_RoomOrbitFlareScript)) &&
            !memcmp(a+4,b+0x1dc,16) && g_RoomOrbitFlareEffects.table.callbacks[0]==RoomEffect_OrbitFlareController &&
            g_RoomOrbitFlareEffects.table.sizes[0]==20 && g_RoomOrbitFlareEffects.initialize==RoomLib_SetArgs3 &&
            g_RoomActorClassTable[4].initialize==RoomLib_InitHandlerC && g_RoomActorClassTable[4].execute==RoomLib_UpdateHandlerC;
        unsigned relocated=0;
        for(unsigned offset=0;valid && offset<0xa48;offset+=4){
            u32 reference,current;memcpy(&reference,a+0x8018f20cu-0x8018eff0u+offset,4);
            memcpy(&current,b+0x80192700u-0x8018eff0u+offset,4);
            if(reference==current)continue;
            u32 expected=reference,opcode=reference>>26;
            if(opcode==2 || opcode==3){
                u32 target=0x80000000u|((reference&0x3ffffffu)<<2);
                if(target>=0x8018f20cu && target<0x8018fc54u)
                    expected=(reference&0xfc000000u)|(((target+0x34f4u)>>2)&0x3ffffffu);
            }else if(offset==0x24 || offset==0x4c || offset==0x194){
                expected=(reference&0xffff0000u)|(u16)((u16)reference+(offset==0x194?0x34f4u:0x1d8u));
            }
            if(current!=expected)valid=0;
            ++relocated;
        }
        valid=valid && relocated==19;
        SceneRoomDirectory *sewer=SceneAsset_ResolveOffset(rooms[2],rooms[2]->header.directoryOffset);
        SceneRoomRecord *entry=SceneAsset_ResolveOffset(rooms[2],sewer->scripts&0x3fffff);
        const u8 *c=(u8 *)rooms[2]+8;
        valid=valid && sewer->scripts>>22==1 && entry->source.bytes.slot==73 && entry->u.handlerAddress==0x80192558u;
        for(unsigned method=0;valid && method<7;++method){
            u32 address;memcpy(&address,c+0x80192558u-0x8018eff0u+method*4,4);
            valid=address==methods[method]-0x10;
        }
        relocated=0;
        for(unsigned offset=0;valid && offset<0xbf0;offset+=4){
            u32 reference,current;memcpy(&reference,b+0x80191514u-0x8018eff0u+offset,4);
            memcpy(&current,c+0x80191504u-0x8018eff0u+offset,4);
            if(reference==current)continue;
            u32 expected=reference,opcode=reference>>26;
            if(opcode==2 || opcode==3){
                u32 target=0x80000000u|((reference&0x3ffffffu)<<2);
                if(target>=0x80191514u && target<0x80192104u)
                    expected=(reference&0xfc000000u)|(((target-0x10u)>>2)&0x3ffffffu);
            }else if(offset==0x3c || offset==0x200 || offset==0x244 || offset==0x370 || offset==0x710){
                expected=(reference&0xffff0000u)|(u16)((u16)reference-0x10u);
            }
            if(current!=expected)valid=0;
            ++relocated;
        }
        valid=valid && relocated==26;
        SceneRoomDirectory *boss=SceneAsset_ResolveOffset(rooms[3],rooms[3]->header.directoryOffset);
        SceneRoomRecord *orb=SceneAsset_ResolveOffset(rooms[3],boss->scripts&0x3fffff);const u8 *d=(u8 *)rooms[3]+8;
        const u32 orb_methods[]={0x8018f244,0x8018f00c,0x8018f0b8,0x8018f0e4,0x8018f12c,0x8018f1b8,0x8018f23c};
        const u32 orb_lists[]={0x8018f24c,0x8018f380,0x8018f434,0x8018fdd4,0xffffffff,
            0x8018f36c,0x8018f3bc,0x8018f830,0x8018fde4,0xffffffff,
            0x8018f374,0x8018f3c4,0x8018fc54,0x8018fee0,0xffffffff};
        const u32 seeds[]={0xc00,0,0x448,0x448,0x448,0};
        valid=valid && boss->scripts>>22==1 && orb->source.bytes.slot==8 && orb->u.handlerAddress==0x8018ff7cu &&
            !memcmp(d+0x8018ff7cu-0x8018eff0u,orb_methods,sizeof(orb_methods)) &&
            !memcmp(d+0x8018ff34u-0x8018eff0u,orb_lists,sizeof(orb_lists)) &&
            !memcmp(d+0x8018ff70u-0x8018eff0u,g_RoomSpawnLayout,12) &&
            !memcmp(d+0x8018ff98u-0x8018eff0u,g_RoomGlowOrbScript,sizeof(g_RoomGlowOrbScript)) &&
            !memcmp((u8 *)rooms[3]+12,seeds,sizeof(seeds)) &&
            g_RoomGlowOrbHandlers.initialize==RoomEffect_GlowOrbSetup && g_RoomGlowOrbHandlers.send==RoomLib_Spawn6 &&
            g_RoomInitList[0]==RoomEffect_GlowOrbCaptureOwner && g_RoomInitList[1]==RoomEffect_GlowOrbSpawnerInit &&
            g_RoomInitList[2]==RoomEffect_GlowOrbInit && g_RoomInitList[3]==RoomEffect_GlowOrbFlashInit &&
            g_RoomDrawList[0]==RoomEffect_GlowOrbControllerDraw && g_RoomDrawList[1]==RoomEffect_GlowOrbSpawnerDraw &&
            g_RoomDrawList[2]==RoomEffect_GlowOrbDraw && g_RoomDrawList[3]==RoomEffect_GlowOrbFlashDraw &&
            g_RoomUpdateList[0]==RoomEffect_GlowOrbControllerUpdate && g_RoomUpdateList[1]==RoomEffect_GlowOrbSpawnerUpdate &&
            g_RoomUpdateList[2]==RoomEffect_GlowOrbUpdate && g_RoomUpdateList[3]==RoomEffect_GlowOrbFlashUpdate;

    }
    if(valid){
        const u8 *d=(u8 *)rooms[4]+8;
        const u32 methods[]={0x8018f064,0x8018f06c,0x8018f098,0x8018f0c4,0x8018f128,0x8018f1b4,0x8018f238};
        SceneRoomDirectory *dir=SceneAsset_ResolveOffset(rooms[4],rooms[4]->header.directoryOffset);
        SceneRoomRecord *record=SceneAsset_ResolveOffset(rooms[4],dir->scripts&0x3fffff);
        valid=dir->scripts>>22==1 && record->source.bytes.slot==9 && record->u.handlerAddress==0x8019290c &&
            !memcmp(d+0x8019290c-0x8018eff0,methods,sizeof(methods)) &&
            !memcmp(d+0x80192928-0x8018eff0,g_RoomDropFieldScript,sizeof(g_RoomDropFieldScript)) &&
            !memcmp(d+0x801928f2-0x8018eff0,g_RoomDropFieldSpawnScript+1,26) &&
            !memcmp(d+0x801929e8-0x8018eff0,g_RoomDropFieldParticleFrames,20) &&
            g_RoomDropFieldSpawnScript[0]==sizeof(RoomDropFieldState);
    }
    /* The complete M028 actor/OrbitFlare library also owns M100's two
     * descriptors. Compare whole modules, all 53 actor functions and tables. */
    if(valid){
        const unsigned ids[]={14,16,28,100,110,121,298,308,311};
        const u8 *module=(u8 *)rooms[1]+8;
        uint64_t hash=UINT64_C(14695981039346656037);
        for(unsigned i=0;i<17060;++i)hash=(hash^module[i])*UINT64_C(1099511628211);
        valid=hash==UINT64_C(0xfb41fb4ec4a00c33);
        for(unsigned r=0;valid && r<sizeof(ids)/sizeof(*ids);++r){
            SceneRoomSectorRange range;
            if(!pe_disc_read(disc,exe,0x800ull+0x80093378u-base+(ids[r]-1)*8,&range,sizeof(range))){valid=0;break;}
            size_t bytes=(size_t)range.roomSectors*2048;SceneAssetView *room=malloc(bytes);
            if(!room){valid=0;break;}
            valid=bytes>=8+17060 && pe_disc_read(disc,&archive,(u64)(range.start+range.scratchSectors+range.textureSectors)*2048,room,bytes);
            if(valid){
                SceneRoomDirectory *dir=SceneAsset_ResolveOffset(room,room->header.directoryOffset);
                SceneRoomRecord *records=SceneAsset_ResolveOffset(room,dir->scripts&0x3fffff);
                unsigned mask=0,slot=ids[r]==14?70:ids[r]==16 || ids[r]==28?73:75;
                u32 handler=slot==70?0x8019316c:slot==73?0x801931dc:0x801931a4;
                valid=dir->scripts>>22==2 && !memcmp((u8 *)room+8,module,17060);
                for(unsigned i=0;valid && i<2;++i){
                    valid=records[i].reserved[0]==0xa4 && records[i].reserved[1]==0x42 && (records[i].source.offsetAndSlot&0xffffff)==8;
                    if(records[i].source.bytes.slot==115 && records[i].u.handlerAddress==0x80193250)mask|=1;
                    else if(records[i].source.bytes.slot==slot && records[i].u.handlerAddress==handler)mask|=2;
                    else valid=0;
                }
                valid=valid && mask==3;
            }
            free(room);
        }
        SceneRoomSectorRange ref;u8 reference[0x35a0];
        if(valid)valid=pe_disc_read(disc,exe,0x800ull+0x80093378u-base+58*8,&ref,sizeof(ref)) &&
            pe_disc_read(disc,&archive,(u64)(ref.start+ref.scratchSectors+ref.textureSectors)*2048+8,reference,sizeof(reference));
        const u16 low[]={0x218,0x408,0x624,0xf80,0x11ac,0x1364,0x1a24,0x1b5c,0x1b98,0x1d6c,
            0x2030,0x2550,0x2714,0x2758,0x2884,0x2c24,0x3128,0x31cc,0x3230,0x3288};
        for(unsigned o=0x1dc;valid && o<0x34f8;o+=4){
            u32 expected,actual;memcpy(&expected,reference+o,4);memcpy(&actual,module+o+16,4);
            u32 target=(expected&0x3ffffff)<<2;
            if((expected>>26==2 || expected>>26==3) && target>=0x18f1cc && target<0x1924e8)expected+=4;
            for(unsigned j=0;j<sizeof(low)/sizeof(*low);++j)if(o==low[j])expected=(expected&0xffff0000)|(u16)(expected+16);
            valid=expected==actual;
        }
        for(unsigned o=4;valid && o<0x1dc;o+=4){
            u32 expected,actual;memcpy(&expected,reference+o,4);memcpy(&actual,module+o,4);
            if(expected>=0x8018f1cc && expected<0x801924e8)expected+=16;
            valid=expected==actual;
        }
        for(unsigned i=0;valid && i<6*7;++i){
            u32 expected,actual;memcpy(&expected,reference+0x34f8+i*4,4);memcpy(&actual,module+0x417c+i*4,4);
            valid=actual==expected+16;
        }
        if(valid)puts("Original actor/OrbitFlare library: identical 17060 bytes in nine rooms, all descriptors, 53 actor functions, four Configure tables and six method rows passed");
    }
    /* Complete M174/M383 module, including all three native descriptors.
     * These rooms combine the already tested glow/drop sets and actor E. */
    for(unsigned r=0;valid && r<2;++r){
        const unsigned id=r?383:174;
        SceneRoomSectorRange range;
        valid=pe_disc_read(disc,exe,0x800ull+0x80093378u-base+(id-1)*8,&range,sizeof(range));
        if(!valid)break;
        size_t bytes=(size_t)range.roomSectors*2048;
        SceneAssetView *room=valid?malloc(bytes):NULL;
        if(!room){valid=0;break;}
        valid=bytes>=8+0x8510 && pe_disc_read(disc,&archive,
            (u64)(range.start+range.scratchSectors+range.textureSectors)*2048,room,bytes);
        if(valid){
            const u8 *module=(u8 *)room+8;
            uint64_t hash=UINT64_C(14695981039346656037);
            for(unsigned i=0;i<0x8510;++i)hash=(hash^module[i])*UINT64_C(1099511628211);
            SceneRoomDirectory *dir=SceneAsset_ResolveOffset(room,room->header.directoryOffset);
            SceneRoomRecord *records=SceneAsset_ResolveOffset(room,dir->scripts&0x3fffff);
            const unsigned slots[]={75,9,8};
            const u32 addresses[]={0x80196f58,0x80196e30,0x80196cc4};
            const u32 actor[]={0x801946d0,0x801946d8,0x80194754,0x801949cc,
                0x801949d4,0x80195114,0x8019516c};
            valid=hash==UINT64_C(0x08ac329d4522e0cc) && dir->scripts>>22==3 &&
                !memcmp(module+0x80196f58u-0x8018eff0u,actor,sizeof(actor)) &&
                !memcmp(module+0x80196ce0u-0x8018eff0u,g_RoomGlowOrbScript,sizeof(g_RoomGlowOrbScript)) &&
                !memcmp(module+0x80196e4cu-0x8018eff0u,g_RoomDropFieldScript,sizeof(g_RoomDropFieldScript));
            for(unsigned i=0;valid && i<3;++i)
                valid=records[i].source.bytes.slot==slots[i] && records[i].u.handlerAddress==addresses[i] &&
                    (records[i].source.offsetAndSlot&0xffffff)==8;
        }
        free(room);
    }
    if(valid)puts("Original M174/M383: complete identical 34064-byte USA module, all three descriptors, actor E row and authored glow/drop scripts match");
    free(rooms[0]);free(rooms[1]);free(rooms[2]);free(rooms[3]);free(rooms[4]);
    if(valid)puts("Original M028: class C slot73, orbit-flare slot115, full USA descriptors/script/seeds and 658 controller instructions with exactly 19 address relocations match M012; M032 class C slot73 descriptor and 764 instructions match M028 after exactly 26 address relocations; M034 glow-orb class slot8, all callback lists, sizes, 35-word script and rotation/flash seeds match USA");
    return valid;
}

static int original_m023_assets(PeDisc *disc,PeDiscFile *exe)
{
    u8 header[0x800];u32 base;SceneRoomSectorRange range;PeDiscFile archive;
    if(!pe_disc_read(disc,exe,0,header,sizeof(header)))return 0;
    memcpy(&base,header+0x18,4);
    if(!pe_disc_read(disc,exe,0x800ull+0x80093378u-base+22*8,&range,sizeof(range)) ||
       !pe_disc_find(disc,"PE.IMG",&archive))return 0;
    size_t bytes=(size_t)range.roomSectors*2048;SceneAssetView *room=malloc(bytes);if(!room)return 0;
    int valid=pe_disc_read(disc,&archive,(u64)(range.start+range.scratchSectors+range.textureSectors)*2048,room,bytes);
    if(valid){
        SceneRoomDirectory *directory=SceneAsset_ResolveOffset(room,room->header.directoryOffset);
        SceneRoomRecord *record=SceneAsset_ResolveOffset(room,directory->scripts&0x3fffff);
        valid=directory->scripts>>22==1 && record->source.bytes.slot==118 && record->u.handlerAddress==0x80190720u;
        const u32 expected[]={0x8018f3c8,0x8018f710,0x8018fc14,0,0,0,0,0,0x00080010,0x30,0,0,0x80190644,0x80190670};
        size_t payload=record->source.offsetAndSlot&0xffffff;
        valid=valid && !memcmp((u8 *)room+payload+0x80190720u-0x8018eff0u,expected,sizeof(expected)) &&
            !memcmp((u8 *)room+payload+0x80190670u-0x8018eff0u,g_RoomM023EffectScript,sizeof(g_RoomM023EffectScript)) &&
            !memcmp((u8 *)room+payload+4,&g_RoomM023JointAnchor,8) &&
            !memcmp((u8 *)room+payload+12,&g_RoomM023RingBandColor,4) &&
            !memcmp((u8 *)room+payload+16,&g_RoomM023StarFanColor,4) &&
            !memcmp((u8 *)room+payload+0x801906f0u-0x8018eff0u,g_RoomM023BeaconColorTrack,48) &&
            g_RoomM023Effects.table.callbacks[0]==RoomM023_ScatterEmitter &&
            g_RoomM023Effects.table.callbacks[1]==RoomM023_JointGlow &&
            g_RoomM023Effects.table.callbacks[2]==RoomM023_JointBeacon &&
            g_RoomM023Effects.table.sizes[0]==16 && g_RoomM023Effects.table.sizes[1]==8 &&
            g_RoomM023Effects.table.sizes[2]==48 && g_RoomM023Effects.initialize==RoomLib_SetTaskVar0 &&
            g_RoomM023Effects.script==g_RoomM023EffectScript;
    }
    if(valid)valid=authored_m023_effect_cycle(room);
    free(room);
    if(valid)puts("Original M023 module: USA slot 118 descriptor, three callbacks/sizes, complete task script, joint anchor and color tracks verified");
    return valid;
}

/* Original M075 CPU corpus; BIOS RNG and the opaque missing-image result
 * are the only hooks in /tmp/pe-motion-cpu.py. All tested procedures run
 * from the USA image, including the original field engine helpers. */
#include "pe1/room_motion_init.h"
#include "pe1/room_debris_triplet.h"
#include "pe1/room_three_particle_class.h"
#include "pe1/asset_key_record.h"
#include "../src/overlays/room_lib/motion_particles.h"
extern FieldFxHandler g_RoomInitList__motion[],g_RoomUpdateList__motion[],g_RoomDrawList__motion[];
extern RoomMotionParticle g_RoomMotionTrailA__motion[10];
extern int g_RoomScript__motion[],g_RoomSpawnLayout__motion[];
static int original_motion_states(void)
{
    FieldFxState *old_state=g_FieldFxScriptState,fx={0};
    unsigned old_rng=Psyz_RandState();uint64_t hash=UINT64_C(14695981039346656037);
    const int frames[]={-32768,-1,0,1,7,15,16,21,30,31,50,58,65,32767};
    const unsigned counts[]={0,1,3,6,8},depths[]={0,1,4,5,128,65532},seeds[]={1,0x12345678};
    unsigned cases=0;int ok=0;g_FieldFxScriptState=&fx;
#define MOTION_HASH(ptr,len) do {const u8 *p_=(const u8 *)(ptr);for(size_t b_=0;b_<(len);++b_)hash=(hash^p_[b_])*UINT64_C(1099511628211);}while(0)
#define MOTION_CHECK(expected,label) do {if(hash!=UINT64_C(expected)){fprintf(stderr,"Original motion %s: %u %016llx\n",label,cases,(unsigned long long)hash);goto done;}hash=UINT64_C(14695981039346656037);cases=0;}while(0)
    for(unsigned ci=0;ci<5;++ci)for(unsigned fi=0;fi<14;++fi)for(unsigned di=0;di<6;++di)for(unsigned si=0;si<2;++si){
        RoomMotionState state;u8 *bytes=(u8 *)&state;int frame=frames[fi];
        for(unsigned i=0;i<sizeof(state);++i)bytes[i]=(u8)(i*37+(u32)frame*3+seeds[si]);
        state.count=counts[ci];state.depth=depths[di];RoomStatePair control={0x13,0x7e,frame};srand(seeds[si]);
        g_RoomUpdateList__motion[3](NULL,&control,&state);unsigned random=Psyz_RandState();
        MOTION_HASH(&state,sizeof(state));MOTION_HASH(&control,sizeof(control));MOTION_HASH(&random,4);++cases;
    }
    MOTION_CHECK(0xc32734cffa10e78d,"update");
    const unsigned seed_values[]={0,1,0x12345678,0xffffffff};
    for(unsigned flag=0;flag<3;++flag)for(unsigned si=0;si<4;++si){
        RoomMotionState state;memset(&state,0xa5,sizeof(state));fx.regs[1]=flag;srand(seed_values[si]);
        g_RoomInitList__motion[3](NULL,NULL,&state);unsigned random=Psyz_RandState();
        MOTION_HASH(&state,sizeof(state));MOTION_HASH(&random,4);++cases;
    }
    MOTION_CHECK(0xee2f9a1f32bcf6dd,"seed");
    const int scales[]={-32768,-1,0,1,1024,32760};const unsigned sprite_depths[]={0,1,16,17,128,65520};
    const int sprite_frames[]={-32768,-1,0,1,7,15,16,21,30,31,50,51,58,65,32767};
    for(unsigned variant=0;variant<2;++variant){
        for(unsigned sc=0;sc<6;++sc)for(unsigned di=0;di<6;++di)for(unsigned fi=0;fi<15;++fi){
            RoomAnchoredSpriteState state;memset(&state,0xa5,sizeof(state));state.scale=scales[sc];state.depth=sprite_depths[di];
            RoomStatePair control={0x13,0x7e,sprite_frames[fi]};g_RoomUpdateList__motion[variant+1](NULL,&control,&state);
            MOTION_HASH(&state,20);MOTION_HASH(&control,4);++cases;
        }
        if(variant)MOTION_CHECK(0x55c8d1b51d80980d,"sprite pair step");else MOTION_CHECK(0xa2aa4b324364c76d,"timed fade");
    }
    const int reloads[]={-32768,-1,0,1,30,32767},timers[]={-32768,-1,0,1,2,30,32767};
    for(unsigned ri=0;ri<6;++ri)for(unsigned ti=0;ti<7;++ti)for(unsigned request=0;request<3;++request)for(unsigned halt=0;halt<2;++halt){
        RoomMotionInitState state;memset(&state,0xa5,sizeof(state));state.timer=reloads[ri];state.field66=timers[ti];state.field68=request;
        FieldFxObject owner={0};RoomStatePair control={0x13,0x7e,0};fx.slot_count=halt;fx.halted=halt;
        g_RoomUpdateList__motion[0](&owner,&control,&state);MOTION_HASH(&state.timer,6);MOTION_HASH(&control,4);++cases;
    }
    MOTION_CHECK(0x6e492949ecc52cb5,"timer");
    {
        FieldGlowSprite old_glyph=g_RoomMotionGlyph,old_glyphs[4];RoomMotionParticle old_a[10],old_b[10];
        memcpy(old_glyphs,g_RoomMotionGlyphs,sizeof(old_glyphs));memcpy(old_a,g_RoomMotionTrailA__motion,sizeof(old_a));memcpy(old_b,g_RoomMotionTrailB,sizeof(old_b));
        u32 empty_archive[32]={0};empty_archive[1]=8;
        void *archive=g_GameState.loaded_scene_archive;g_GameState.loaded_scene_archive=(struct SceneAssetBlob *)empty_archive;
        for(unsigned pattern=0;pattern<4;++pattern){
            RoomMotionInitState state;RoomMotionInitOwner owner={0};FieldActor actor={0};RenderMatrix matrices[14];
            memset(&state,0xa5,sizeof(state));owner.link=&actor;actor.render_object.matrices=matrices;
            for(unsigned i=0;i<sizeof(matrices);++i)((u8 *)matrices)[i]=(u8)(i*37+pattern*71);
            memset(&g_RoomMotionGlyph,0xa5,sizeof(g_RoomMotionGlyph));memset(g_RoomMotionGlyphs,0xa5,sizeof(g_RoomMotionGlyphs));
            memset(g_RoomMotionTrailA__motion,0xa5,sizeof(old_a));memset(g_RoomMotionTrailB,0xa5,sizeof(old_b));
            g_RoomInitList__motion[0](&owner,NULL,&state);
            if(state.link!=&actor || fx.current_context!=&state)goto init_done;
            MOTION_HASH(&state.base,102);MOTION_HASH(&state.allocation,4);MOTION_HASH(&g_RoomMotionGlyph,16);
            MOTION_HASH(g_RoomMotionTrailA__motion,sizeof(old_a));MOTION_HASH(g_RoomMotionTrailB,sizeof(old_b));MOTION_HASH(g_RoomMotionGlyphs,sizeof(old_glyphs));++cases;
        }
init_done:
        g_GameState.loaded_scene_archive=(struct SceneAssetBlob *)archive;g_RoomMotionGlyph=old_glyph;memcpy(g_RoomMotionGlyphs,old_glyphs,sizeof(old_glyphs));
        memcpy(g_RoomMotionTrailA__motion,old_a,sizeof(old_a));memcpy(g_RoomMotionTrailB,old_b,sizeof(old_b));
    }
    MOTION_CHECK(0xb34061a49b104785,"init and native actor matrices");
    for(unsigned pattern=0;pattern<4;++pattern){
        RoomAnchoredSpriteState state;memset(&state,0xa5,sizeof(state));
        for(unsigned i=0;i<0x114;++i)((u8 *)&state)[i]=(u8)(i*37+pattern*71);
        for(unsigned i=0;i<20;++i)((u8 *)&state.ring)[sizeof(void *)+i]=(u8)((0x118+i)*37+pattern*71);
        g_RoomInitList__motion[2](NULL,NULL,&state);
        if(state.ring.source!=state.sourceData)goto done;
        MOTION_HASH(&state,0x114);MOTION_HASH((u8 *)&state.ring+sizeof(void *),20);++cases;
    }
    MOTION_CHECK(0x395f99da3ab86c99,"pair ring init");
    ok=1;
done:
    g_FieldFxScriptState=old_state;srand(old_rng);
#undef MOTION_CHECK
#undef MOTION_HASH
    if(ok)puts("Original M075 motion: 2192 USA CPU update/seed/fade/timer/init states, native actor matrices and pointer-bearing ring passed");
    return ok;
}

/* Frozen pre-integration native corpus; no independent PS1 CPU claim. */
static int original_seeker_particle_lifetimes(void)
{
    const int velocities[]={-32768,-32767,-63,-1,0,1,63,32766,32767};
    uint64_t hash=UINT64_C(14695981039346656037);
    unsigned count=0;
    s16 saved_floor=g_RoomFloorY->y;
    g_RoomFloorY->y=0;
    for(int state=1;state<=2;++state)for(unsigned timer=0;timer<65536;++timer)
    for(unsigned v=0;v<9;++v)for(unsigned above=0;above<2;++above){
        RoomDampedSpark particle={.x=-32760,.y=above?32760:-32760,.z=32760,
            .vx=velocities[v],.vy=velocities[(v+3)%9],.vz=velocities[8-v],
            .state=state,.timer=timer};
        int result=RoomEffect_SeekerSparkCallback(1,&particle,0);
        for(unsigned i=0;i<sizeof(particle);++i)
            hash=(hash^((unsigned char *)&particle)[i])*UINT64_C(1099511628211);
        for(unsigned i=0;i<4;++i)
            hash=(hash^(((unsigned)result>>(i*8))&255))*UINT64_C(1099511628211);
        ++count;
    }
    g_RoomFloorY->y=saved_floor;
    if(count!=2359296 || hash!=UINT64_C(0x596df89ba7c17665)){
        fprintf(stderr,"Original seeker lifetime corpus: %u %016llx\n",
                count,(unsigned long long)hash);
        return 0;
    }
    puts("Original seeker: 2359296 lifetime states, all timers and signed velocity/floor boundaries passed");
    return 1;
}

static int original_motion_program(void)
{
    struct { _Alignas(16) u8 packets[32768]; u8 guard[32]; } arena;
    static RenderGpuTag ordering[4096];
    _Alignas(8) u8 archive[128]={0};
    ((SceneAssetBlob *)archive)->directoryOffset=8;
    ((SceneAssetDirectory *)(archive+8))->keyEntries=(1u<<22)|64;
    ((AssetU32KeyRecord *)(archive+64))->packedOffset=80;
    ((AssetU32KeyRecord *)(archive+64))->key=0x73decd80;
    ((TimPackedImage *)(archive+80))->key.bytes.type=12;
    ((TimPackedImage *)(archive+80))->geometry=(832u<<10)|(256u<<21);
    RenderBufferPrefix old_buffers=D_800B0E38;int old_slot=g_ActiveDrawSlot,old_offset=g_DrawPacketOffset;
    GteMatrix *old_view=g_GeomScroll.words.viewMatrix;void *old_archive=g_GameState.loaded_scene_archive;
    FieldActor *old_player=g_PlayerEntity;FieldFxState *old_state=g_FieldFxScriptState;
    FieldFxObject *old_object=g_FieldFxScriptObject;FieldFxSlot *old_slots=g_FieldFxSlotTable;
    char *old_data=g_FieldFxScriptData;int *old_commands=g_FieldFxCommandTable;
    unsigned old_rng=Psyz_RandState();s16 old_floor=g_RoomFloorY->y;int ok=0;
    GteMatrix view={.m={{4096,0,0},{0,4096,0},{0,0,4096}}};
    g_GeomScroll.words.viewMatrix=&view;g_GameState.loaded_scene_archive=(struct SceneAssetBlob *)archive;g_RoomFloorY->y=0;
    Psyz_GteCtrlWrite(26,256);Psyz_GteCtrlWrite(29,1365);Psyz_GteCtrlWrite(30,1024);gte_SetGeomOffset(160,120);
    for(unsigned draw_slot=0;draw_slot<2;++draw_slot){
        FieldFxObject object={0};FieldActor actor={0};FieldActorState actor_state={0};RenderMatrix matrices[14]={0};u8 action_state[4]={2};
        actor.state=&actor_state;actor.render_object.matrices=matrices;actor_state.control10.command_value=1;actor_state.action_state=action_state;
        actor.pos_x=8000*65536;actor.pos_z=8000*65536;object.prefix.actor=&actor;object.prefix.asset_type=12;g_PlayerEntity=&actor;
        for(unsigned i=0;i<14;++i){for(unsigned j=0;j<3;++j)matrices[i].rotation[j][j]=4096;matrices[i].translation[2]=640;}
        matrices[9].translation[0]=-96;matrices[13].translation[0]=96;
        g_ActiveDrawSlot=draw_slot;D_800B0E38.ordering[draw_slot]=ordering;D_800B0E38.packets[draw_slot]=(char *)arena.packets;srand(1);
        g_RoomMotionHandlers.init();g_RoomMotionHandlers.initialize((PmSlotHeader *)&object);
        intptr_t wide=sizeof(intptr_t)>4?(intptr_t)UINT64_C(0x1234567887654321):0x7654321;
        g_RoomMotionHandlers.send((PmSlotHeader *)&object,0,8,wide,0,0);
        if(object.state.commands!=g_RoomScript__motion || object.state.vars[8]!=wide)goto done;
        /* USA slot 12 waits while variant < 2 and target +0xAC is zero. */
        FieldFxObject *before_gate=malloc(sizeof(object));
        if(!before_gate)goto done;
        memcpy(before_gate,&object,sizeof(object));
        for(unsigned variant=0;variant<2;++variant){
            actor.mode=variant;
            g_RoomMotionHandlers.execute((PmSlotHeader *)&object);
            if(memcmp(before_gate,&object,sizeof(object))){
                free(before_gate);
                fprintf(stderr,"Motion registration ran through closed gate variant%u\n",variant);
                goto done;
            }
        }
        free(before_gate);
        if(draw_slot==0)actor.mode=2;
        else actor_state.reserveda8[4]=1;
        unsigned seen=0,drawn=0,frames=0;
        for(;frames<320;++frames){
            memset(&arena,0xa5,sizeof(arena));for(unsigned i=0;i<4096;++i)ordering[i]=(RenderGpuTag){.address=0xffffff};g_DrawPacketOffset=0;
            g_RoomMotionHandlers.start((PmSlotHeader *)&object);
            g_RoomMotionHandlers.execute((PmSlotHeader *)&object);
            for(unsigned i=0;i<64;++i)if(object.slots[i].flag){
                FieldFxSlot *slot=&object.slots[i];seen|=1u<<slot->handler_id;
                if(slot->handler_id>=5 || slot->data_offset<0 || (unsigned)slot->data_offset>=FIELDFX_DATA_SIZE ||
                   (uintptr_t)(object.data+slot->data_offset)%_Alignof(void *))goto done;
            }
            if(g_DrawPacketOffset<0 || g_DrawPacketOffset>(int)sizeof(arena.packets))goto done;
            drawn+=g_DrawPacketOffset!=0;
            for(unsigned i=0;i<sizeof(arena.guard);++i)if(arena.guard[i]!=0xa5)goto done;
            if(object.prefix.reserved00==4)break;
        }
        if(seen!=31 || !drawn || frames==320){fprintf(stderr,"Motion program slot%u seen%x frames%u drawn%u pc%d\n",draw_slot,seen,frames,drawn,object.state.pc);goto done;}
        actor_state.core_flags=0x3fabcdef;action_state[0]=2;
        g_RoomMotionHandlers.stop((PmSlotHeader *)&object);g_RoomMotionHandlers.unload();
        if(object.prefix.reserved00!=4 || actor_state.core_flags!=0x00abcdef || action_state[0]!=4)goto done;
    }
    ok=1;
done:
    D_800B0E38=old_buffers;g_ActiveDrawSlot=old_slot;g_DrawPacketOffset=old_offset;g_GeomScroll.words.viewMatrix=old_view;
    g_GameState.loaded_scene_archive=old_archive;g_PlayerEntity=old_player;g_RoomFloorY->y=old_floor;
    g_FieldFxScriptState=old_state;g_FieldFxScriptObject=old_object;g_FieldFxSlotTable=old_slots;g_FieldFxScriptData=old_data;g_FieldFxCommandTable=old_commands;srand(old_rng);
    if(ok)puts("Original M075 motion program: real PM/FieldFx lifecycle, all five object types, both draw slots, native pointer alignment and packet guards passed");
    return ok;
}

/* USA M063 CPU execution, with COP2 forwarded to PsyZ for this corpus. */
static int original_window_lifecycle(void)
{
    const s8 variants[]={-1,0,2},options[]={-1,0,3};const u8 modes[]={0,2},counts[]={0,1,4};
    const u16 lows[]={0,5},highs[]={0,2},frames[]={0,2,5};
    uint64_t hash=UINT64_C(14695981039346656037);unsigned cases=0;
    for(unsigned vi=0;vi<3;++vi)for(unsigned oi=0;oi<3;++oi)for(unsigned mi=0;mi<2;++mi)
    for(unsigned li=0;li<2;++li)for(unsigned hi=0;hi<2;++hi)for(unsigned ci=0;ci<3;++ci)for(unsigned fi=0;fi<3;++fi){
        RoomWindowObject object={0};FieldActor actor={0},anchor={0};FieldActorState marker={0};u8 marker_state[4]={0};
        object.actor=(RoomWindowActor *)&actor;actor.parent=&anchor;anchor.state=&marker;marker.action_state=marker_state;
        g_RoomWindowClassHandlers.initialize((PmSlotHeader *)&object);
        object.state.variant=variants[vi];object.state.optionB=options[oi];anchor.mode=modes[mi];anchor.action=counts[ci];
        anchor.anim.parts.integer=lows[li];anchor.anim_prev=(s32)((u32)highs[hi]<<16);
        object.state.callback=RoomLib_ArmWindowClass;RoomLib_ArmWindowClass(&object);
        unsigned armed=object.state.callback==RoomLib_WindowHandler;
        anchor.anim_prev=(s32)((u32)frames[fi]<<16);actor.flags=0xa5ffffff;actor.render_object.flags_9C=0xffff;
        RoomLib_WindowArrived(&object);
        u32 words[]={armed,object.status,object.enabled,actor.flags,actor.render_object.flags_9C,marker_state[0]};
        for(unsigned i=0;i<sizeof(words)/sizeof(words[0]);++i)for(unsigned byte=0;byte<4;++byte)
            hash=(hash^((words[i]>>(byte*8))&255))*UINT64_C(1099511628211);
        ++cases;
    }
    if(hash!=UINT64_C(0xd06378940ffec935)){fprintf(stderr,"Original window lifecycle differs: %u %016llx\n",cases,(unsigned long long)hash);return 0;}
    RoomWindowObject object={0};int enabled=-123;
    g_RoomWindowClassHandlers.initialize((PmSlotHeader *)&object);
    g_RoomWindowClassHandlers.send((PmSlotHeader *)&object,0,19,7,0,0);
    g_RoomWindowClassHandlers.send((PmSlotHeader *)&object,1,19,(intptr_t)&enabled,0,0);
    if(enabled!=7)return 0;
    puts("Original window lifecycle: 648 unhooked USA CPU arm/arrival/release states and native-width command output passed");return 1;
}

static int original_three_particle_program(void)
{
    RenderBufferPrefix old_buffers=D_800B0E38;int old_slot=g_ActiveDrawSlot,old_offset=g_DrawPacketOffset;
    GteMatrix *old_view=g_GeomScroll.words.viewMatrix;FieldFxState *old_state=g_FieldFxScriptState;
    FieldFxObject *old_object=g_FieldFxScriptObject;FieldFxSlot *old_slots=g_FieldFxSlotTable;char *old_data=g_FieldFxScriptData;int *old_commands=g_FieldFxCommandTable;
    void *old_archive=g_GameState.loaded_scene_archive;FieldActor *old_player=g_PlayerEntity;u32 old_random=Psyz_RandState();
    PolygonVertex *old_polygon=g_FloorPolygonVertices;u16 old_count=g_FloorPolygonVertexCount;RoomFloorY old_floor=*g_RoomFloorY;
    RoomFxSpritePacket old_packet=RoomLib_ThreeParticlePacket,old_shadow=RoomLib_ThreeParticleFloorPacket;
    s32 *old_projection=g_GeomScroll.words.projectionDistance,projection=320;
    GteMatrix view={.m={{4096,0,0},{0,4096,0},{0,0,4096}}};
    _Alignas(void *) static u8 arena[65536];static RenderGpuTag ordering[4096];
    FieldFxState script={0};RoomDebrisClock clock={0};script.current_context=&clock;g_FieldFxScriptState=&script;
    g_RoomFloorY->y=0;g_GeomScroll.words.viewMatrix=&view;g_GeomScroll.words.projectionDistance=&projection;
    const s16 frames[]={0,1,12,40,80,-1,32767};const unsigned masks[]={0,1,3,7};
    uint64_t hash=UINT64_C(14695981039346656037);unsigned cases=0;
#define THREE_FAIL() do {fprintf(stderr,"Three particle guard line %u case%u used%d\n",__LINE__,cases,g_DrawPacketOffset);return 0;}while(0)
#define THREE_PUT(value,bytes) do {uint64_t word_=(uint64_t)(value);for(unsigned byte_=0;byte_<(bytes);++byte_){hash=(hash^(u8)word_)*UINT64_C(1099511628211);word_>>=8;}}while(0)
    for(unsigned page=0;page<2;++page)for(unsigned draw=0;draw<2;++draw)for(unsigned camera=0;camera<2;++camera)
    for(unsigned variant=0;variant<2;++variant)for(unsigned mask=0;mask<4;++mask)for(unsigned frame=0;frame<7;++frame){
        for(unsigned i=0;i<32;++i){Psyz_GteCtrlWrite(i,0);Psyz_GteDataWrite(i,0);}
        Psyz_GteCtrlWrite(24,160u<<16);Psyz_GteCtrlWrite(25,112u<<16);Psyz_GteCtrlWrite(26,320);Psyz_GteCtrlWrite(29,1365);Psyz_GteCtrlWrite(30,1024);
        view.t[0]=camera?23:0;view.t[1]=camera?-17:0;view.t[2]=camera?250:0;
        memset(arena,0xa5,sizeof(arena));for(unsigned i=0;i<4096;++i)ordering[i]=(RenderGpuTag){.address=0xffffff,.length=0xab};
        g_ActiveDrawSlot=draw;g_DrawPacketOffset=0;D_800B0E38.ordering[draw]=ordering;D_800B0E38.packets[draw]=(char *)arena;
        clock.asset.index=page;RoomLib_ThreeParticlePacket=(RoomFxSpritePacket){.r=128,.g=128,.b=128,.offset=-100,.depth=128};
        RoomLib_ThreeParticleFloorPacket=(RoomFxSpritePacket){.r=128,.g=128,.b=128,.code=4,.mode=1,.offset=50,.depth=128};
        RoomDebrisTriplet data={0};data.reserved08=variant;data.jitter[0]=-3;data.jitter[1]=2;data.jitter[2]=4;
        for(unsigned i=0;i<3;++i){
            data.active[i]=masks[mask]>>i&1?0:-1;data.size[i]=(u8[]){0,64,128}[i];data.swing[i]=(u16[]){128,2048,32768}[i];data.phase[i]=100*i+10;data.life[i]=20+i*7;
            data.position[i].x=((int)i*160-160)*65536;data.position[i].y=(-(int)i*70-100)*65536;data.position[i].z=(900+(int)i*100)*65536;
            data.rotation[i]=(GteShortVector){16*i,64*i,32*i,0};
        }
        RoomFxControl control={1,1,frames[frame]};RoomLib_DrawThreeTransformParticles(NULL,&control,&data);
        if(g_DrawPacketOffset<0 || g_DrawPacketOffset%sizeof(FieldStripPacket))THREE_FAIL();
        unsigned count=g_DrawPacketOffset/sizeof(FieldStripPacket);if(count>9)THREE_FAIL();
        THREE_PUT(count,2);
        for(unsigned i=0;i<12;++i)THREE_PUT(((u8 *)&RoomLib_ThreeParticlePacket)[i],1);
        for(unsigned i=0;i<12;++i)THREE_PUT(((u8 *)&RoomLib_ThreeParticleFloorPacket)[i],1);
        for(unsigned i=0;i<count;++i){FieldStripPacket *packet=(void *)(arena+i*sizeof(*packet));
            if(packet->tag.length!=9)THREE_FAIL();
            for(unsigned j=0;j<36;++j)THREE_PUT(((u8 *)&packet->r0)[j],1);
        }
        for(unsigned layer=0;layer<4096;++layer){uintptr_t link=ordering[layer].address;unsigned links=0;if(link==0xffffff)continue;
            THREE_PUT(layer,2);
            while(link!=0xffffff){if(link<(uintptr_t)arena || link>=(uintptr_t)arena+g_DrawPacketOffset || (link-(uintptr_t)arena)%sizeof(FieldStripPacket) || ++links>9)THREE_FAIL();
                THREE_PUT((link-(uintptr_t)arena)/sizeof(FieldStripPacket),1);link=((RenderGpuTag *)link)->address;}
            THREE_PUT(links,1);
        }
        THREE_PUT(65535,2);++cases;
        for(unsigned i=g_DrawPacketOffset+sizeof(FieldStripPacket);i<sizeof(arena);++i)if(arena[i]!=0xa5)THREE_FAIL();
    }
#undef THREE_PUT
    if(hash!=UINT64_C(0x51e1deb8f951b445)){fprintf(stderr,"Three particle draw %u %016llx\n",cases,(unsigned long long)hash);THREE_FAIL();}
    _Alignas(8) u8 archive[128]={0};((SceneAssetBlob *)archive)->directoryOffset=8;
    ((SceneAssetDirectory *)(archive+8))->keyEntries=(1u<<22)|64;
    ((AssetU32KeyRecord *)(archive+64))->packedOffset=80;((AssetU32KeyRecord *)(archive+64))->key=0x73decd80;
    ((TimPackedImage *)(archive+80))->key.bytes.type=10;((TimPackedImage *)(archive+80))->geometry=(832u<<10)|(256u<<21);
    g_GameState.loaded_scene_archive=(struct SceneAssetBlob *)archive;
    PolygonVertex polygon[4]={{.x=-2000,.z=-2000},{.x=2000,.z=-2000},{.x=2000,.z=2000},{.x=-2000,.z=2000}};
    g_FloorPolygonVertices=polygon;g_FloorPolygonVertexCount=4;
    view.t[0]=view.t[1]=view.t[2]=0;
    for(unsigned draw=0;draw<2;++draw){
        FieldFxObject object={0};FieldActor actor={0};FieldActorState state={0};u8 action=2;
        RenderMatrix matrix={.rotation={{4096,0,0},{0,4096,0},{0,0,4096}},.translation={0,-100,1200}};
        actor.state=&state;state.control10.command_value=1;state.action_state=&action;actor.render_object.matrices=&matrix;
        actor.pos_x=8000*65536;actor.pos_z=8000*65536;g_PlayerEntity=&actor;object.prefix.actor=&actor;
        g_RoomThreeParticleHandlers.init();g_RoomThreeParticleHandlers.initialize((PmSlotHeader *)&object);
        const intptr_t vars[]={300,-2,40,512,0,1000,8};
        for(unsigned i=0;i<7;++i)g_RoomThreeParticleHandlers.send((PmSlotHeader *)&object,0,i,vars[i],0,0);
        intptr_t wide=sizeof(intptr_t)>4?(intptr_t)UINT64_C(0x1234567887654321):0x7654321;
        g_RoomThreeParticleHandlers.send((PmSlotHeader *)&object,0,8,wide,0,0);
        if(object.state.commands!=g_RoomThreeParticleScript || object.state.vars[6]!=8 || object.state.vars[8]!=wide)THREE_FAIL();
        unsigned seen=0,drawn=0,ticks=0;int double_seen=0,single_seen=0;
        for(;ticks<200;++ticks){
            memset(arena,0xa5,sizeof(arena));for(unsigned i=0;i<4096;++i)ordering[i]=(RenderGpuTag){.address=0xffffff};
            g_ActiveDrawSlot=draw;g_DrawPacketOffset=0;D_800B0E38.ordering[draw]=ordering;D_800B0E38.packets[draw]=(char *)arena;
            g_RoomThreeParticleHandlers.start((PmSlotHeader *)&object);g_RoomThreeParticleHandlers.execute((PmSlotHeader *)&object);
            for(unsigned i=0;i<64;++i)if(object.slots[i].flag){FieldFxSlot *slot=&object.slots[i];seen|=1u<<slot->handler_id;
                if(slot->handler_id>1 || slot->data_offset<0 || (uintptr_t)(object.data+slot->data_offset)%(slot->handler_id?_Alignof(RoomDebrisTriplet):_Alignof(RoomDebrisClock)))THREE_FAIL();
                if(slot->handler_id==0){RoomDebrisClock *data=(void *)(object.data+slot->data_offset);
                    if(data->actor!=&actor || memcmp(&data->view,&matrix,32) || data->reload!=8 || data->asset.index!=0)THREE_FAIL();}
                if(slot->handler_id==1){RoomDebrisTriplet *data=(void *)(object.data+slot->data_offset);if(data->reserved08==1)double_seen=1;else single_seen=1;}
            }
            if(g_DrawPacketOffset<0 || g_DrawPacketOffset>sizeof(arena))THREE_FAIL();drawn+=g_DrawPacketOffset!=0;
            for(unsigned i=g_DrawPacketOffset+sizeof(FieldStripPacket);i<sizeof(arena);++i)if(arena[i]!=0xa5)THREE_FAIL();
            if(object.prefix.reserved00==4)break;
        }
        if(ticks==200 || seen!=3 || !drawn || !double_seen || !single_seen){fprintf(stderr,"Three particle PM %u ticks%u seen%u draws%u variants%d/%d\n",draw,ticks,seen,drawn,double_seen,single_seen);THREE_FAIL();}
        state.core_flags=0x3fabcdef;g_RoomThreeParticleHandlers.stop((PmSlotHeader *)&object);g_RoomThreeParticleHandlers.unload();
        if(state.core_flags!=0x00abcdef || action!=4)THREE_FAIL();
    }
    D_800B0E38=old_buffers;g_ActiveDrawSlot=old_slot;g_DrawPacketOffset=old_offset;g_GeomScroll.words.viewMatrix=old_view;
    g_GeomScroll.words.projectionDistance=old_projection;g_FieldFxScriptState=old_state;g_FieldFxScriptObject=old_object;g_FieldFxSlotTable=old_slots;g_FieldFxScriptData=old_data;g_FieldFxCommandTable=old_commands;
    g_GameState.loaded_scene_archive=old_archive;g_PlayerEntity=old_player;g_FloorPolygonVertices=old_polygon;g_FloorPolygonVertexCount=old_count;*g_RoomFloorY=old_floor;
    RoomLib_ThreeParticlePacket=old_packet;RoomLib_ThreeParticleFloorPacket=old_shadow;srand(old_random);
    puts("Original three particles: 448 USA CPU/BIOS/GTE draw packets and OT chains, both buffers, color/brightness alias and full authored PM script with both particle passes and expiry passed");
    return 1;
#undef THREE_FAIL
}

static int original_m089_assets(PeDisc *disc,PeDiscFile *exe)
{
#define M089_ASSET_FAIL() do {fprintf(stderr,"M089 asset guard line %u\n",__LINE__);return 0;}while(0)
    u8 header[32],module[20864],reference[0xaa4];u32 base;PeDiscFile archive;SceneRoomSectorRange range,reference_range;
    if(!pe_disc_read(disc,exe,0,header,sizeof(header)) || !pe_disc_find(disc,"PE.IMG",&archive))M089_ASSET_FAIL();
    memcpy(&base,header+24,4);
    if(!pe_disc_read(disc,exe,0x800ull+0x80093378u-base+88*8,&range,sizeof(range)) ||
       !pe_disc_read(disc,exe,0x800ull+0x80093378u-base+58*8,&reference_range,sizeof(reference_range)))M089_ASSET_FAIL();
    u64 sector=(u64)(range.start+range.scratchSectors+range.textureSectors)*2048;
    if(!pe_disc_read(disc,&archive,sector+8,module,sizeof(module)) ||
       !pe_disc_read(disc,&archive,(u64)(reference_range.start+reference_range.scratchSectors+reference_range.textureSectors)*2048+8+0xf4c,reference,sizeof(reference)))M089_ASSET_FAIL();
    uint64_t hash=UINT64_C(14695981039346656037);for(unsigned j=0;j<sizeof(module);++j)hash=(hash^module[j])*UINT64_C(1099511628211);
    if(hash!=UINT64_C(0x3f2c0352d3a3caa9))M089_ASSET_FAIL();
    const u32 descriptor[]={0x80192a30,0x80192cd4,0x80193618,0,0,0,0,0,0x40008,12,0,0,0x80193ff0,0x801940cc};
    const u32 methods[]={0x8018ff4c,0x8018ff54,0x8018ffd0,0x80190248,0x80190250,0x80190990,0x801909e8};
    if(memcmp(module+0x5138,descriptor,sizeof(descriptor)) || memcmp(module+0x506c,methods,sizeof(methods)) ||
       memcmp(module+0x50dc,g_RoomM089Script,44) || memcmp(module+0x5108,g_RoomM089PulseColorTrack,48) ||
       memcmp(module+0x1dc,&g_RoomM089TrailHeadColor,4) || memcmp(module+0x1e0,&g_RoomM089TrailTailColor,4) ||
       memcmp(module+0x1e4,&g_RoomM089SpinGlowColor,4) || memcmp(module+0x1e8,&g_RoomM089SwirlSpriteColor,4))M089_ASSET_FAIL();
    for(unsigned j=0;j<sizeof(reference);j+=4){u32 expected,actual;memcpy(&expected,reference+j,4);memcpy(&actual,module+0xf5c+j,4);
        unsigned opcode=expected>>26,target=(expected&0x3ffffff)<<2;
        if((opcode==2 || opcode==3) && target>=0x18ff3c && target<0x1909e0)expected+=4;
        if(j==0x34 || j==0x260 || j==0x418)expected=(expected&0xffff0000)|(u16)(expected+16);
        if(actual!=expected)M089_ASSET_FAIL();
    }
    u32 jumps[33];if(!pe_disc_read(disc,&archive,(u64)(reference_range.start+reference_range.scratchSectors+reference_range.textureSectors)*2048+8+0x84,jumps,sizeof(jumps)))M089_ASSET_FAIL();
    for(unsigned j=0;j<33;++j){u32 actual;memcpy(&actual,module+0x84+j*4,4);if(actual!=jumps[j]+16)M089_ASSET_FAIL();}
    for(unsigned j=0x5170;j<sizeof(module);++j)if(module[j])M089_ASSET_FAIL();
    if(g_RoomM089Program.table.callbacks[0]!=RoomM089_SpiralTrail || g_RoomM089Program.table.callbacks[1]!=RoomM089_SpinModel ||
       g_RoomM089Program.table.callbacks[2]!=RoomM089_ModelPulse || g_RoomM089Program.table.sizes[0]!=8 ||
       g_RoomM089Program.table.sizes[1]!=4 || g_RoomM089Program.table.sizes[2]!=12 || g_RoomM089Program.initialize!=RoomM089_SetAnchor)M089_ASSET_FAIL();
    size_t bytes=(size_t)range.roomSectors*2048;SceneAssetView *room=malloc(bytes);if(!room)M089_ASSET_FAIL();
    int valid=pe_disc_read(disc,&archive,sector,room,bytes);
    if(valid){SceneRoomDirectory *directory=SceneAsset_ResolveOffset(room,room->header.directoryOffset);
        SceneRoomRecord *records=SceneAsset_ResolveOffset(room,directory->scripts&0x3fffff);unsigned mask=0;valid=directory->scripts>>22==2;
        for(unsigned j=0;valid && j<2;++j){valid=records[j].reserved[0]==0x80 && records[j].reserved[1]==0x51 && (records[j].source.offsetAndSlot&0xffffff)==8;
            if(records[j].source.bytes.slot==75 && records[j].u.handlerAddress==0x8019405c)mask|=1;
            else if(records[j].source.bytes.slot==119 && records[j].u.handlerAddress==0x80194128)mask|=2;
            else valid=0;
        }valid=valid && mask==3;
    }
    if(valid){
        PmTables old_tables=g_PmTables;Pe1GameState old_game=g_GameState;u32 old_flags=g_GameStateFlags;
        void *old_program=g_FieldFxTaskPrograms[34];
        FieldFxTaskContext *old_context=g_FieldFxTaskContext;FieldFxTaskSlot *old_task=g_FieldFxActiveTaskSlot;
        FieldFxObjectPrefix *old_owner=g_FieldFxOwner;FieldActor *old_player=g_PlayerEntity;
        RenderBufferPrefix old_buffers=D_800B0E38;GteMatrix *old_view=g_GeomScroll.words.viewMatrix;
        int old_age=g_FieldFxCurrentAge,old_draw=g_ActiveDrawSlot,old_offset=g_DrawPacketOffset,old_floor=g_RoomFloorY->y;
        u32 old_random=Psyz_RandState();u8 *old_model_a=g_RoomM089CoreModel,*old_model_b=g_RoomM089PulseModel;
        short old_anchor[3];u8 old_track[48];memcpy(old_anchor,g_RoomM089Anchor,6);memcpy(old_track,g_RoomM089PulseColorTrack,48);
        static RenderGpuTag ordering[4096];static struct {_Alignas(void *) u8 bytes[262144];u8 guard[32];} packets;
        GteMatrix view={.m={{4096,0,0},{0,4096,0},{0,0,4096}}};
        g_GeomScroll.words.viewMatrix=&view;g_RoomFloorY->y=0;g_GameState.loaded_scene_archive=(void *)room;
        g_GameStateFlags=0x80;g_GameState.flags&=~8u;g_GameState.tim_load_state=0;
        g_FieldFxTaskPrograms[34]=&g_RoomM089Program;
        FieldFx_InitTexturePageTable();Psyz_GteCtrlWrite(26,256);Psyz_GteCtrlWrite(29,1365);Psyz_GteCtrlWrite(30,1024);gte_SetGeomOffset(160,112);
        for(unsigned program=0;valid && program<1;++program)for(unsigned draw=0;valid && draw<2;++draw){
            struct {PmPrimarySlot slots[11];u8 guard[32];} bank={0};memset(bank.guard,0xa5,32);
            FieldActor actor={0},player={0};FieldActorState state={0};Combatant player_state={0};RenderMatrix matrices[8]={0};u8 action=1;
            actor.state=&state;actor.render_object.matrices=matrices;state.action_state=&action;state.core_flags=0x01000000;
            player.state=(void *)&player_state;player.pos_x=8000*65536;player.pos_z=8000*65536;g_PlayerEntity=&player;
            for(unsigned j=0;j<8;++j){for(unsigned k=0;k<3;++k)matrices[j].rotation[k][k]=4096;matrices[j].translation[1]=-800;matrices[j].translation[2]=1200;}
            g_PmTables=(PmTables){g_PmCmdHandlerArray,bank.slots,NULL};srand(1);
            if(Scene_LoadRoomAssets(119,&actor)!=0 || Pm_SendCmd(0,0,1,-100,-800,1200)){valid=0;break;}
            unsigned frames=0,draws=0,children=0,seen=0;
            for(;frames<260;++frames){
                for(unsigned j=0;j<4096;++j)ordering[j]=(RenderGpuTag){.address=0xffffff};
                memset(packets.guard,0xa5,32);g_ActiveDrawSlot=draw;g_DrawPacketOffset=0;
                D_800B0E38.ordering[draw]=ordering;D_800B0E38.packets[draw]=(char *)packets.bytes;
                if(Pm_Start(0) || Pm_Exec(0)){valid=0;break;}
                draws+=g_DrawPacketOffset!=0;FieldFxTaskContext *ctx=&bank.slots[0].task.tasks;
                for(unsigned slot=0;slot<8;++slot)if(ctx->slots[slot].id!=0xffff){
                    seen|=1u<<ctx->slots[slot].id;if(ctx->slots[slot].id==1 || !ctx->slots[slot].end)continue;
                    FieldFxCallbackList *list=(void *)ctx->slots[slot].end;
                    for(unsigned j=0;j<(unsigned)list->count;++j)children+=((FieldFxCallbackListEntry *)(list->entries+j*list->stride))->active!=0;
                }
                if(ctx->used>sizeof(ctx->arena) || g_DrawPacketOffset<0 || g_DrawPacketOffset>sizeof(packets.bytes)){valid=0;break;}
                for(unsigned j=0;j<4096;++j){uintptr_t link=ordering[j].address;unsigned links=0;
                    while(link && link!=0xffffff){if(link<(uintptr_t)packets.bytes || link+sizeof(RenderGpuTag)>(uintptr_t)packets.bytes+g_DrawPacketOffset || ++links>4096){valid=0;break;}link=((RenderGpuTag *)link)->address;}
                    if(!valid)break;
                }
                for(unsigned j=0;j<32;++j)if(bank.guard[j]!=0xa5 || packets.guard[j]!=0xa5)valid=0;
                if(!valid || !bank.slots[0].header.state)break;
            }
            if(frames==260 || !draws || !children || seen!=7 || bank.slots[0].header.state || bank.slots[0].task.tasks.count){fprintf(stderr,"M089 PM %u/%u frames%u draws%u children%u state%u count%u\n",program,draw,frames,draws,children,bank.slots[0].header.state,bank.slots[0].task.tasks.count);valid=0;}
        }
        g_PmTables=old_tables;g_GameState=old_game;g_GameStateFlags=old_flags;g_FieldFxTaskPrograms[34]=old_program;
        g_FieldFxTaskContext=old_context;g_FieldFxActiveTaskSlot=old_task;g_FieldFxOwner=old_owner;g_PlayerEntity=old_player;
        D_800B0E38=old_buffers;g_GeomScroll.words.viewMatrix=old_view;g_FieldFxCurrentAge=old_age;g_ActiveDrawSlot=old_draw;g_DrawPacketOffset=old_offset;g_RoomFloorY->y=old_floor;srand(old_random);
        g_RoomM089CoreModel=old_model_a;g_RoomM089PulseModel=old_model_b;memcpy(g_RoomM089Anchor,old_anchor,6);memcpy(g_RoomM089PulseColorTrack,old_track,48);
        if(valid)puts("Original M089: complete authored task program, all three top-level callbacks, actual USA models, both GPU buffers, complete nested pools, model/sprite draw chains, expiry and guards passed");
    }
    free(room);if(!valid)M089_ASSET_FAIL();
#undef M089_ASSET_FAIL
    puts("Original M089 assets: entire module, complete class E and Configure targets, all six functions, constants, script and native task119 passed");return 1;
}

static int original_line_burst_assets(PeDisc *disc,PeDiscFile *exe)
{
    u8 header[32];u32 base;PeDiscFile archive;SceneRoomSectorRange range;
    if(!pe_disc_read(disc,exe,0,header,32) || !pe_disc_find(disc,"PE.IMG",&archive))return 0;
    memcpy(&base,header+24,4);
    if(!pe_disc_read(disc,exe,0x800ull+0x80093378u-base+121*8,&range,sizeof(range)))return 0;
    size_t bytes=(size_t)range.roomSectors*2048;SceneAssetView *room=malloc(bytes);if(!room)return 0;
    int valid=pe_disc_read(disc,&archive,(u64)(range.start+range.scratchSectors+range.textureSectors)*2048,room,bytes);
    const u32 descriptor[]={0x80190444,0,0,0,0,0,0,0,36,0,0,0,0x80190e98,0x80190fcc};
    if(valid)valid=!memcmp((u8 *)room+8+0x1ffc,descriptor,sizeof(descriptor)) &&
        !memcmp((u8 *)room+8+0x1fdc,g_RoomLineBurstScript,sizeof(g_RoomLineBurstScript)) &&
        g_RoomLineBurstProgram.table.callbacks[0]==RoomEffect_LineBurstController &&
        g_RoomLineBurstProgram.table.sizes[0]==36 && g_RoomLineBurstProgram.initialize==RoomLib_SlotSet;
    const unsigned line_rooms[]={104,115,159};
    for(unsigned r=0;valid && r<3;++r){
        SceneRoomSectorRange variant_range;
        if(!pe_disc_read(disc,exe,0x800ull+0x80093378u-base+(line_rooms[r]-1)*8,&variant_range,sizeof(variant_range))){valid=0;break;}
        size_t variant_bytes=(size_t)variant_range.roomSectors*2048;SceneAssetView *variant=malloc(variant_bytes);if(!variant){valid=0;break;}
        valid=pe_disc_read(disc,&archive,(u64)(variant_range.start+variant_range.scratchSectors+variant_range.textureSectors)*2048,variant,variant_bytes);
        if(valid){
            const u8 *data=(u8 *)variant+8,*reference=(u8 *)room+8;
            uint64_t hash=UINT64_C(14695981039346656037);for(unsigned i=0;i<3876;++i)hash=(hash^data[i])*UINT64_C(1099511628211);
            SceneRoomDirectory *dir=SceneAsset_ResolveOffset(variant,variant->header.directoryOffset);
            SceneRoomRecord *record=SceneAsset_ResolveOffset(variant,dir->scripts&0x3fffff);
            valid=hash==UINT64_C(0xd5cf48761589413b) && dir->scripts>>22==1 && record->reserved[0]==0x24 && record->reserved[1]==0x0f &&
                record->source.offsetAndSlot==0x88000008u && record->u.handlerAddress==0x8018fe5cu;
            const u32 expected_descriptor[]={0x8018f3a4,0,0,0,0,0,0,0,36,0,0,0,0x8018fdf8,0x8018fe3c};
            valid=valid && !memcmp(data+4,reference+12,20) && !memcmp(data+0xe4c,g_RoomLineBurstScript,32) &&
                !memcmp(data+0xe6c,expected_descriptor,sizeof(expected_descriptor));
            for(unsigned o=0;valid && o<0xe34;o+=4){
                u32 expected,actual;memcpy(&expected,reference+0x10b8+o,4);memcpy(&actual,data+0x18+o,4);
                u32 target=(expected&0x3ffffff)<<2;
                if((expected>>26==2 || expected>>26==3) && target>=0x1900a8 && target<0x190edc)expected-=0x10a0/4;
                if(o==0x20 || o==0x3c8 || o==0x3f0)expected=(expected&0xffff0000)|(u16)(expected-8);
                if(o==0x524)expected=(expected&0xffff0000)|(u16)(expected-0x10a0);
                if(o==0xdf8)expected=(expected&0xffff0000)|(u16)(expected-0x1194);
                valid=expected==actual;
            }
            for(unsigned i=0xea4;valid && i<3876;++i)valid=data[i]==0;
        }
        free(variant);if(!valid)fprintf(stderr,"Line-only complete module failed for M%03u\n",line_rooms[r]);
    }
    /* The M116/M120 module includes the complete 53-function actor library
     * followed by the three original line-burst/parameter functions. */
    SceneRoomSectorRange actor_range;u8 actor_reference[0x35a0];
    if(valid)valid=pe_disc_read(disc,exe,0x800ull+0x80093378u-base+58*8,&actor_range,sizeof(actor_range)) &&
        pe_disc_read(disc,&archive,(u64)(actor_range.start+actor_range.scratchSectors+actor_range.textureSectors)*2048+8,actor_reference,sizeof(actor_reference));
    const unsigned actor_line_rooms[]={116,120,263,265,333};
    const u16 actor_low[]={0x218,0x408,0x624,0xf80,0x11ac,0x1364,0x1a24,0x1b5c,0x1b98,0x1d6c,
        0x2030,0x2550,0x2714,0x2758,0x2884,0x2c24,0x3128,0x31cc,0x3230,0x3288};
    for(unsigned r=0;valid && r<5;++r){
        SceneRoomSectorRange variant_range;
        if(!pe_disc_read(disc,exe,0x800ull+0x80093378u-base+(actor_line_rooms[r]-1)*8,&variant_range,sizeof(variant_range))){valid=0;break;}
        size_t variant_bytes=(size_t)variant_range.roomSectors*2048;SceneAssetView *variant=malloc(variant_bytes);if(!variant){valid=0;break;}
        valid=pe_disc_read(disc,&archive,(u64)(variant_range.start+variant_range.scratchSectors+variant_range.textureSectors)*2048,variant,variant_bytes);
        if(valid){
            const unsigned combined=r>=2,library_bytes=combined?20932:17600,code_delta=combined?36:20;
            const unsigned rows=combined?0x4fc4:0x4340,line_start=combined?0x4190:0x350c,line_seed=combined?0x1ec:0x1dc;
            const unsigned script_offset=combined?0x50e0:0x43e8,descriptor_offset=combined?0x5100:0x4408,params_offset=combined?0x5144:0x4440;
            const u8 *data=(u8 *)variant+8,*line=(u8 *)room+8;
            uint64_t hash=UINT64_C(14695981039346656037);for(unsigned i=0;i<library_bytes;++i)hash=(hash^data[i])*UINT64_C(1099511628211);
            SceneRoomDirectory *dir=SceneAsset_ResolveOffset(variant,variant->header.directoryOffset);
            SceneRoomRecord *record=SceneAsset_ResolveOffset(variant,dir->scripts&0x3fffff);unsigned mask=0;
            valid=hash==(combined?UINT64_C(0x0b0c7c878f4b80b4):UINT64_C(0x3a3ae4a6df997701)) && dir->scripts>>22==(combined?4:2);
            for(unsigned i=0;valid && i<(combined?4:2);++i){
                valid=(record[i].source.offsetAndSlot&0xffffff)==8 && record[i].reserved[0]==(combined?0xc4:0xc0) && record[i].reserved[1]==(combined?0x51:0x44);
                if(record[i].source.bytes.slot==75 && record[i].u.handlerAddress==(combined?0x80193fecu:0x80193368u))mask|=1;
                else if(record[i].source.bytes.slot==136 && record[i].u.handlerAddress==(combined?0x801940f0u:0x801933f8u))mask|=2;
                else if(combined && record[i].source.bytes.slot==70 && record[i].u.handlerAddress==0x80193fb4u)mask|=4;
                else if(combined && record[i].source.bytes.slot==115 && record[i].u.handlerAddress==0x80194098u)mask|=8;
                else valid=0;
            }
            valid=valid && mask==(combined?15:3);
            for(unsigned o=4;valid && o<0x1dc;o+=4){u32 expected,actual;memcpy(&expected,actor_reference+o,4);memcpy(&actual,data+o,4);
                if(expected>=0x8018f1ccu && expected<0x801924e8u)expected+=code_delta;valid=expected==actual;}
            for(unsigned o=0x1dc;valid && o<0x34f8;o+=4){u32 expected,actual;memcpy(&expected,actor_reference+o,4);memcpy(&actual,data+o+code_delta,4);
                u32 target=(expected&0x3ffffff)<<2;
                if((expected>>26==2 || expected>>26==3) && target>=0x18f1cc && target<0x1924e8)expected+=code_delta/4;
                for(unsigned j=0;j<20;++j)if(o==actor_low[j])expected=(expected&0xffff0000)|(u16)(expected+code_delta);
                valid=expected==actual;
            }
            for(unsigned i=0;valid && i<42;++i){u32 expected,actual;memcpy(&expected,actor_reference+0x34f8+i*4,4);memcpy(&actual,data+rows+i*4,4);valid=actual==expected+code_delta;}
            for(unsigned o=0;valid && o<0xe34;o+=4){u32 expected,actual;memcpy(&expected,line+0x10b8+o,4);memcpy(&actual,data+line_start+o,4);
                u32 target=(expected&0x3ffffff)<<2;
                if((expected>>26==2 || expected>>26==3) && target>=0x1900a8 && target<0x190edc)expected+=(line_start-0x10b8)/4;
                if(o==0x20 || o==0x3c8 || o==0x3f0)expected=(expected&0xffff0000)|(u16)(expected+(line_seed-0xc));
                if(o==0x524)expected=(expected&0xffff0000)|(u16)(expected+(line_start-0x10b8));
                if(o==0xdf8)expected=(expected&0xffff0000)|(u16)(expected+(params_offset-0x2038));
                valid=expected==actual;
            }
            const u32 expected_descriptor[]={0x8018eff0u+line_start+0x39c,0,0,0,0,0,0,0,36,0,0,0,
                0x8018eff0u+line_start+0xdf0,0x8018eff0u+script_offset};
            valid=valid && !memcmp(data+line_seed,line+12,20) && !memcmp(data+script_offset,g_RoomLineBurstScript,32) &&
                !memcmp(data+descriptor_offset,expected_descriptor,sizeof(expected_descriptor));
            for(unsigned i=combined?0x5138:0x4440;valid && i<library_bytes;++i)valid=data[i]==0;
        }
        free(variant);if(!valid)fprintf(stderr,"Actor/line complete module failed for M%03u\n",actor_line_rooms[r]);
    }
    SceneRoomSectorRange orbit_range;u8 orbit_reference[17060];
    if(valid)valid=pe_disc_read(disc,exe,0x800ull+0x80093378u-base+27*8,&orbit_range,sizeof(orbit_range)) &&
        pe_disc_read(disc,&archive,(u64)(orbit_range.start+orbit_range.scratchSectors+orbit_range.textureSectors)*2048+8,orbit_reference,sizeof(orbit_reference));
    const unsigned orbit_line_rooms[]={113,263,265,333};
    for(unsigned r=0;valid && r<4;++r){
        SceneRoomSectorRange variant_range;
        if(!pe_disc_read(disc,exe,0x800ull+0x80093378u-base+(orbit_line_rooms[r]-1)*8,&variant_range,sizeof(variant_range))){valid=0;break;}
        size_t variant_bytes=(size_t)variant_range.roomSectors*2048;SceneAssetView *variant=malloc(variant_bytes);if(!variant){valid=0;break;}
        valid=pe_disc_read(disc,&archive,(u64)(variant_range.start+variant_range.scratchSectors+variant_range.textureSectors)*2048,variant,variant_bytes);
        if(valid){
            const unsigned combined=r!=0,library_bytes=combined?20932:7208,seed=combined?0x1dc:4;
            const unsigned orbit_start=combined?0x351c:0x28,line_start=combined?0x4190:0xc9c;
            const unsigned orbit_script=combined?0x506c:0x1ad0,orbit_descriptor=combined?0x50a8:0x1b0c;
            const unsigned line_script=combined?0x50e0:0x1b44,line_descriptor=combined?0x5100:0x1b64;
            const unsigned args=combined?0x5138:0x1b9c,params=combined?0x5144:0x1ba8;
            const u8 *data=(u8 *)variant+8,*line=(u8 *)room+8;
            uint64_t hash=UINT64_C(14695981039346656037);for(unsigned i=0;i<library_bytes;++i)hash=(hash^data[i])*UINT64_C(1099511628211);
            valid=hash==(combined?UINT64_C(0x0b0c7c878f4b80b4):UINT64_C(0x238b3ab2185b7ea9));
            if(!combined){SceneRoomDirectory *dir=SceneAsset_ResolveOffset(variant,variant->header.directoryOffset);
                SceneRoomRecord *records=SceneAsset_ResolveOffset(variant,dir->scripts&0x3fffff);unsigned mask=0;
                valid=valid && dir->scripts>>22==2;
                for(unsigned i=0;valid && i<2;++i){valid=records[i].reserved[0]==0x28 && records[i].reserved[1]==0x1c && (records[i].source.offsetAndSlot&0xffffff)==8;
                    if(records[i].source.bytes.slot==115 && records[i].u.handlerAddress==0x80190afcu)mask|=1;
                    else if(records[i].source.bytes.slot==136 && records[i].u.handlerAddress==0x80190b54u)mask|=2;else valid=0;}
                valid=valid && mask==3;
            }
            for(unsigned o=0;valid && o<0xc74;o+=4){u32 expected,actual;memcpy(&expected,orbit_reference+0x3508+o,4);memcpy(&actual,data+orbit_start+o,4);
                u32 target=(expected&0x3ffffff)<<2;int delta=(int)orbit_start-0x3508;
                if((expected>>26==2 || expected>>26==3) && target>=0x1924f8 && target<0x19316c)expected+=delta/4;
                if(o==0x22c || o==0x254)expected=(expected&0xffff0000)|(u16)(expected+seed-0x1dc);
                if(o==0x39c)expected=(expected&0xffff0000)|(u16)(expected+delta);
                if(o==0xc54 || o==0xc60 || o==0xc68)expected=(expected&0xffff0000)|(u16)(expected+args-0x4298);
                valid=expected==actual;
            }
            for(unsigned o=0;valid && o<0xe34;o+=4){u32 expected,actual;memcpy(&expected,line+0x10b8+o,4);memcpy(&actual,data+line_start+o,4);
                u32 target=(expected&0x3ffffff)<<2;int delta=(int)line_start-0x10b8;
                if((expected>>26==2 || expected>>26==3) && target>=0x1900a8 && target<0x190edc)expected+=delta/4;
                if(o==0x20 || o==0x3c8 || o==0x3f0)expected=(expected&0xffff0000)|(u16)(expected+seed+16-12);
                if(o==0x524)expected=(expected&0xffff0000)|(u16)(expected+delta);
                if(o==0xdf8)expected=(expected&0xffff0000)|(u16)(expected+params-0x2038);
                valid=expected==actual;
            }
            const u32 orbit_desc[]={0x8018eff0u+orbit_start+0x208,0,0,0,0,0,0,0,20,0,0,0,
                0x8018eff0u+orbit_start+0xc50,0x8018eff0u+orbit_script};
            const u32 line_desc[]={0x8018eff0u+line_start+0x39c,0,0,0,0,0,0,0,36,0,0,0,
                0x8018eff0u+line_start+0xdf0,0x8018eff0u+line_script};
            valid=valid && !memcmp(data+seed,orbit_reference+0x1dc,16) && !memcmp(data+seed+16,line+12,20) &&
                !memcmp(data+orbit_script,g_RoomOrbitFlareScript,60) && !memcmp(data+line_script,g_RoomLineBurstScript,32) &&
                !memcmp(data+orbit_descriptor,orbit_desc,sizeof(orbit_desc)) && !memcmp(data+line_descriptor,line_desc,sizeof(line_desc));
            for(unsigned i=args;valid && i<library_bytes;++i)valid=data[i]==0;
        }
        free(variant);if(!valid)fprintf(stderr,"Orbit/line complete module failed for M%03u\n",orbit_line_rooms[r]);
    }
    if(valid){
        PmTables old_tables=g_PmTables;Pe1GameState old_game=g_GameState;u32 old_flags=g_GameStateFlags;
        void *old_program=g_FieldFxTaskPrograms[51];
        FieldFxTaskContext *old_context=g_FieldFxTaskContext;FieldFxTaskSlot *old_task=g_FieldFxActiveTaskSlot;
        FieldFxObjectPrefix *old_owner=g_FieldFxOwner;FieldActor *old_player=g_PlayerEntity;
        RenderBufferPrefix old_buffers=D_800B0E38;GteMatrix *old_view=g_GeomScroll.words.viewMatrix;
        int old_age=g_FieldFxCurrentAge,old_draw=g_ActiveDrawSlot,old_offset=g_DrawPacketOffset,old_floor=g_RoomFloorY->y;
        u32 old_random=Psyz_RandState();
        RoomSlotRec old_params[8];memcpy(old_params,RoomLib_SlotTable,sizeof(old_params));
        PolygonVertex *old_polygon=g_FloorPolygonVertices;u16 old_polygon_count=g_FloorPolygonVertexCount;
        PolygonVertex polygon[]={{0,-4000,0,-4000},{0,4000,0,-4000},{0,4000,0,4000},{0,-4000,0,4000}};
        g_FloorPolygonVertices=polygon;g_FloorPolygonVertexCount=4;
        static RenderGpuTag ordering[4096];static struct {_Alignas(void *) u8 bytes[262144];u8 guard[32];} packets;
        GteMatrix view={.m={{4096,0,0},{0,4096,0},{0,0,4096}}};
        g_GeomScroll.words.viewMatrix=&view;g_RoomFloorY->y=0;g_GameState.loaded_scene_archive=(void *)room;
        g_GameStateFlags=0x80;g_GameState.flags&=~8u;g_GameState.tim_load_state=0;
        g_FieldFxTaskPrograms[51]=&g_RoomLineBurstProgram;
        FieldFx_InitTexturePageTable();Psyz_GteCtrlWrite(26,256);Psyz_GteCtrlWrite(29,1365);Psyz_GteCtrlWrite(30,1024);gte_SetGeomOffset(160,112);
        for(unsigned program=0;valid && program<1;++program)for(unsigned draw=0;valid && draw<2;++draw){
            struct {PmPrimarySlot slots[11];u8 guard[32];} bank={0};memset(bank.guard,0xa5,32);
            FieldActor actor={0},player={0};FieldActorState state={0};Combatant player_state={0};RenderMatrix matrices[21]={0};u8 action=1;
            actor.state=&state;actor.render_object.matrices=matrices;state.action_state=&action;state.core_flags=0x01000000;
            player.state=(void *)&player_state;player.render_object.matrices=matrices;player.pos_x=0;player.pos_z=1200*65536;g_PlayerEntity=&player;
            for(unsigned j=0;j<21;++j){for(unsigned k=0;k<3;++k)matrices[j].rotation[k][k]=4096;matrices[j].translation[1]=-800;matrices[j].translation[2]=1200;}
            g_PmTables=(PmTables){g_PmCmdHandlerArray,bank.slots,NULL};srand(1);
            if(Scene_LoadRoomAssets(136,&actor)!=0 || Pm_SendCmd(0,0,1,0,48,8) || Pm_SendCmd(0,0,0,0,0,1200)){valid=0;break;}
            unsigned frames=0,draws=0,children=0,marked=0;
            for(;frames<260;++frames){
                for(unsigned j=0;j<4096;++j)ordering[j]=(RenderGpuTag){.address=0xffffff};
                memset(packets.guard,0xa5,32);g_ActiveDrawSlot=draw;g_DrawPacketOffset=0;
                D_800B0E38.ordering[draw]=ordering;D_800B0E38.packets[draw]=(char *)packets.bytes;
                if(Pm_Start(0) || Pm_Exec(0)){valid=0;break;}
                marked|=(state.core_flags&0x3f000000)==0x19000000;
                draws+=g_DrawPacketOffset!=0;FieldFxTaskContext *ctx=&bank.slots[0].task.tasks;
                for(unsigned slot=0;slot<8;++slot)if(ctx->slots[slot].id!=0xffff && ctx->slots[slot].end){
                    FieldFxCallbackList *list=(void *)ctx->slots[slot].end;
                    for(unsigned j=0;j<(unsigned)list->count;++j)children+=((FieldFxCallbackListEntry *)(list->entries+j*list->stride))->active!=0;
                }
                if(ctx->used>sizeof(ctx->arena) || g_DrawPacketOffset<0 || g_DrawPacketOffset>sizeof(packets.bytes)){valid=0;break;}
                for(unsigned j=0;j<4096;++j){uintptr_t link=ordering[j].address;unsigned links=0;
                    while(link && link!=0xffffff){if(link<(uintptr_t)packets.bytes || link+sizeof(RenderGpuTag)>(uintptr_t)packets.bytes+g_DrawPacketOffset || ++links>4096){valid=0;break;}link=((RenderGpuTag *)link)->address;}
                    if(!valid)break;
                }
                for(unsigned j=0;j<32;++j)if(bank.guard[j]!=0xa5 || packets.guard[j]!=0xa5)valid=0;
                if(!valid || !bank.slots[0].header.state)break;
            }
            if(frames==260 || !draws || !children || bank.slots[0].header.state || bank.slots[0].task.tasks.count || action!=4 || !(player_state.stateFlags&0x4000) || !marked || !(state.core_flags&0x80000000u)){fprintf(stderr,"line burst PM %u/%u frames%u draws%u children%u state%u count%u action%u flags%x mark%u hit%x\n",program,draw,frames,draws,children,bank.slots[0].header.state,bank.slots[0].task.tasks.count,action,state.core_flags,marked,player_state.stateFlags);valid=0;}
        }
        g_PmTables=old_tables;g_GameState=old_game;g_GameStateFlags=old_flags;g_FieldFxTaskPrograms[51]=old_program;
        g_FieldFxTaskContext=old_context;g_FieldFxActiveTaskSlot=old_task;g_FieldFxOwner=old_owner;g_PlayerEntity=old_player;
        D_800B0E38=old_buffers;g_GeomScroll.words.viewMatrix=old_view;g_FieldFxCurrentAge=old_age;g_ActiveDrawSlot=old_draw;g_DrawPacketOffset=old_offset;g_RoomFloorY->y=old_floor;srand(old_random);
        memcpy(RoomLib_SlotTable,old_params,sizeof(old_params));
        g_FloorPolygonVertices=old_polygon;g_FloorPolygonVertexCount=old_polygon_count;
        if(valid)puts("Original line burst: authored task136, original M122 archive and sound path, both GPU buffers, complete nested pools, model/sprite draw chains, expiry and guards passed");
    }
    free(room);if(valid)puts("Original line families M104/M113/M115/M116/M120/M159/M263/M265/M333: all complete modules, all 53 actor functions where present, orbit/line functions and initializers, every instruction of both callbacks and slot initializer after relocation, all descriptors, seeds, script and parameter data passed");return valid;
}

static int original_dropped_flare_assets(PeDisc *disc,PeDiscFile *exe)
{
#define FLARE_FAIL() do {fprintf(stderr,"Dropped flare asset guard line%u\n",__LINE__);return 0;}while(0)
    u8 header[32],module[3408];u32 base;PeDiscFile archive;SceneRoomSectorRange range;
    if(!pe_disc_read(disc,exe,0,header,sizeof(header)) || !pe_disc_find(disc,"PE.IMG",&archive))FLARE_FAIL();
    memcpy(&base,header+24,4);const unsigned rooms[]={126,129,139,149,326};
    SceneAssetView *room=NULL;int valid=1;
    for(unsigned index=0;index<5;++index){
        if(!pe_disc_read(disc,exe,0x800ull+0x80093378u-base+(rooms[index]-1)*8,&range,sizeof(range)))FLARE_FAIL();
        u64 sector=(u64)(range.start+range.scratchSectors+range.textureSectors)*2048;
        if(!pe_disc_read(disc,&archive,sector+8,module,sizeof(module)))FLARE_FAIL();
        uint64_t hash=UINT64_C(14695981039346656037);for(unsigned j=0;j<sizeof(module);++j)hash=(hash^module[j])*UINT64_C(1099511628211);
        if(hash!=UINT64_C(0x64f7d5eb8f98dfc6))FLARE_FAIL();
        const u32 descriptor[]={0x8018f9f8,0,0,0,0,0,0,0,16,0,0,0,0x8018fcc0,0x8018fcdc};
        const u8 seeds[]={0,4,0,0,0,0,1,0,200,8,0,0,0,0,0,0};
        const u8 rectangle[]={0,0,0,0,240,255,0,0};
        if(memcmp(module+0xd10,descriptor,sizeof(descriptor)) || memcmp(module+0xcec,g_RoomDroppedFlareScript,36) ||
           memcmp(module+4,seeds,16) || memcmp(module+0x40,rectangle,8))FLARE_FAIL();
        for(unsigned j=0xd48;j<sizeof(module);++j)if(module[j])FLARE_FAIL();
        size_t bytes=(size_t)range.roomSectors*2048;SceneAssetView *variant=malloc(bytes);if(!variant)FLARE_FAIL();
        valid=pe_disc_read(disc,&archive,sector,variant,bytes);
        if(valid){SceneRoomDirectory *directory=SceneAsset_ResolveOffset(variant,variant->header.directoryOffset);
            SceneRoomRecord *record=SceneAsset_ResolveOffset(variant,directory->scripts&0x3fffff);
            valid=directory->scripts>>22==1 && record->reserved[0]==0x50 && record->reserved[1]==0x0d &&
                record->source.offsetAndSlot==0x7a000008 && record->u.handlerAddress==0x8018fd00;
        }
        if(index==0)room=variant;else free(variant);if(!valid){free(room);FLARE_FAIL();}
    }
    if(g_RoomDroppedFlareProgram.table.callbacks[0]!=RoomEffect_DroppedFlareEmitter ||
       g_RoomDroppedFlareProgram.table.sizes[0]!=16 || g_RoomDroppedFlareProgram.initialize!=RoomEffect_DroppedFlareSetArgs)FLARE_FAIL();
    if(valid){
        PmTables old_tables=g_PmTables;Pe1GameState old_game=g_GameState;u32 old_flags=g_GameStateFlags;
        void *old_program=g_FieldFxTaskPrograms[37];
        FieldFxTaskContext *old_context=g_FieldFxTaskContext;FieldFxTaskSlot *old_task=g_FieldFxActiveTaskSlot;
        FieldFxObjectPrefix *old_owner=g_FieldFxOwner;FieldActor *old_player=g_PlayerEntity;
        RenderBufferPrefix old_buffers=D_800B0E38;GteMatrix *old_view=g_GeomScroll.words.viewMatrix;
        int old_age=g_FieldFxCurrentAge,old_draw=g_ActiveDrawSlot,old_offset=g_DrawPacketOffset,old_floor=g_RoomFloorY->y;
        u32 old_random=Psyz_RandState();
        int old_params[2];memcpy(old_params,g_RoomDroppedFlareArgs,8);
        static RenderGpuTag ordering[4096];static struct {_Alignas(void *) u8 bytes[262144];u8 guard[32];} packets;
        GteMatrix view={.m={{4096,0,0},{0,4096,0},{0,0,4096}}};
        g_GeomScroll.words.viewMatrix=&view;g_RoomFloorY->y=0;g_GameState.loaded_scene_archive=(void *)room;
        g_GameStateFlags=0x80;g_GameState.flags&=~8u;g_GameState.tim_load_state=0;
        g_FieldFxTaskPrograms[37]=&g_RoomDroppedFlareProgram;
        FieldFx_InitTexturePageTable();Psyz_GteCtrlWrite(26,256);Psyz_GteCtrlWrite(29,1365);Psyz_GteCtrlWrite(30,1024);gte_SetGeomOffset(160,112);
        for(unsigned phase=0;valid && phase<2;++phase){
            FieldActor actor={0},player={0};FieldActorState marker={0};RenderMatrix matrices[21]={0};u8 action=1;
            FieldFxObjectPrefix owner={.actor=&actor};FieldFxTaskContext context={.flags=1};
            _Alignas(void *) u8 pool[1024];FieldFxTaskSlot task={.end=(char *)pool};
            struct {RoomDroppedFlareEmitter state;u8 guard[16];} emitter;
            memset(&emitter,0xa5,sizeof(emitter));memset(pool,0xa5,sizeof(pool));
            for(unsigned j=0;j<21;++j){for(unsigned k=0;k<3;++k)matrices[j].rotation[k][k]=4096;
                matrices[j].translation[0]=j*20;matrices[j].translation[1]=-800;matrices[j].translation[2]=1200;}
            actor.state=&marker;marker.action_state=&action;actor.render_object.matrices=matrices;
            player.render_object.matrices=matrices;g_PlayerEntity=&player;context.variables[0]=phase;
            g_FieldFxOwner=&owner;g_FieldFxTaskContext=&context;g_FieldFxActiveTaskSlot=&task;
            int used=RoomEffect_DroppedFlareEmitter(0,&emitter.state,(intptr_t)g_RoomDroppedFlareArgs);
            FieldFxCallbackList *list=(void *)pool;
            if(used<0 || used>sizeof(pool) || list->count!=24 || list->callback!=RoomEffect_DroppedFlareParticle ||
               emitter.state.x!=380 || emitter.state.y!=-800 || emitter.state.z!=1184 || action!=2 ||
               (!phase && (emitter.state.angles.x || emitter.state.angles.y!=2048 || emitter.state.angles.z)) ||
               (phase && emitter.state.angles.x!=0x180))valid=0;
            for(unsigned j=0;j<16;++j)if(emitter.guard[j]!=0xa5)valid=0;
            for(unsigned j=used;j<sizeof(pool);++j)if(pool[j]!=0xa5)valid=0;
        }
        for(unsigned program=0;valid && program<1;++program)for(unsigned draw=0;valid && draw<2;++draw){
            struct {PmPrimarySlot slots[11];u8 guard[32];} bank={0};memset(bank.guard,0xa5,32);
            FieldActor actor={0},player={0};FieldActorState state={0};Combatant player_state={0};RenderMatrix matrices[21]={0};u8 action=1;
            actor.state=&state;actor.render_object.matrices=matrices;state.action_state=&action;state.core_flags=0x01000000;
            player.state=(void *)&player_state;player.render_object.matrices=matrices;player.pos_x=8000*65536;player.pos_z=8000*65536;g_PlayerEntity=&player;
            for(unsigned j=0;j<21;++j){for(unsigned k=0;k<3;++k)matrices[j].rotation[k][k]=4096;matrices[j].translation[1]=-800;matrices[j].translation[2]=1200;}
            g_PmTables=(PmTables){g_PmCmdHandlerArray,bank.slots,NULL};srand(1);
            if(Scene_LoadRoomAssets(122,&actor)!=0 || Pm_SendCmd(0,0,0,128,4096,0)){valid=0;break;}
            unsigned frames=0,draws=0,children=0;
            for(;frames<260;++frames){
                for(unsigned j=0;j<4096;++j)ordering[j]=(RenderGpuTag){.address=0xffffff};
                memset(packets.guard,0xa5,32);g_ActiveDrawSlot=draw;g_DrawPacketOffset=0;
                D_800B0E38.ordering[draw]=ordering;D_800B0E38.packets[draw]=(char *)packets.bytes;
                if(Pm_Start(0) || Pm_Exec(0)){valid=0;break;}
                draws+=g_DrawPacketOffset!=0;FieldFxTaskContext *ctx=&bank.slots[0].task.tasks;
                for(unsigned slot=0;slot<8;++slot)if(ctx->slots[slot].id!=0xffff && ctx->slots[slot].end){
                    FieldFxCallbackList *list=(void *)ctx->slots[slot].end;
                    for(unsigned j=0;j<(unsigned)list->count;++j)children+=((FieldFxCallbackListEntry *)(list->entries+j*list->stride))->active!=0;
                }
                if(ctx->used>sizeof(ctx->arena) || g_DrawPacketOffset<0 || g_DrawPacketOffset>sizeof(packets.bytes)){valid=0;break;}
                for(unsigned j=0;j<4096;++j){uintptr_t link=ordering[j].address;unsigned links=0;
                    while(link && link!=0xffffff){if(link<(uintptr_t)packets.bytes || link+sizeof(RenderGpuTag)>(uintptr_t)packets.bytes+g_DrawPacketOffset || ++links>4096){valid=0;break;}link=((RenderGpuTag *)link)->address;}
                    if(!valid)break;
                }
                for(unsigned j=0;j<32;++j)if(bank.guard[j]!=0xa5 || packets.guard[j]!=0xa5)valid=0;
                if(!valid || !bank.slots[0].header.state)break;
            }
            if(frames==260 || !draws || !children || bank.slots[0].header.state || bank.slots[0].task.tasks.count){fprintf(stderr,"dropped flare PM %u/%u frames%u draws%u children%u state%u count%u\n",program,draw,frames,draws,children,bank.slots[0].header.state,bank.slots[0].task.tasks.count);valid=0;}
        }
        g_PmTables=old_tables;g_GameState=old_game;g_GameStateFlags=old_flags;g_FieldFxTaskPrograms[37]=old_program;
        g_FieldFxTaskContext=old_context;g_FieldFxActiveTaskSlot=old_task;g_FieldFxOwner=old_owner;g_PlayerEntity=old_player;
        D_800B0E38=old_buffers;g_GeomScroll.words.viewMatrix=old_view;g_FieldFxCurrentAge=old_age;g_ActiveDrawSlot=old_draw;g_DrawPacketOffset=old_offset;g_RoomFloorY->y=old_floor;srand(old_random);
        memcpy(g_RoomDroppedFlareArgs,old_params,8);
        if(valid)puts("Original dropped flare: authored task122, original M126 archive and sound path, both GPU buffers, complete nested pools, model/sprite draw chains, expiry and guards passed");
    }
    free(room);if(!valid)FLARE_FAIL();
#undef FLARE_FAIL
    puts("Original dropped flare assets: five entire identical modules, descriptor, all script words, seed data and native task122 passed");return 1;
}

static int original_m086_assets(PeDisc *disc,PeDiscFile *exe)
{
    u8 header[32],module[7080];u32 base;PeDiscFile archive;SceneRoomSectorRange range;
    if(!pe_disc_read(disc,exe,0,header,sizeof(header)) || !pe_disc_find(disc,"PE.IMG",&archive))return 0;
    memcpy(&base,header+24,4);
    if(!pe_disc_read(disc,exe,0x800ull+0x80093378u-base+85*8,&range,sizeof(range)))return 0;
    u64 sector=(u64)(range.start+range.scratchSectors+range.textureSectors)*2048;
    if(!pe_disc_read(disc,&archive,sector+8,module,sizeof(module)))return 0;
    uint64_t hash=UINT64_C(14695981039346656037);for(unsigned j=0;j<sizeof(module);++j)hash=(hash^module[j])*UINT64_C(1099511628211);
    if(hash!=UINT64_C(0x20b7a2d44a6de28b))return 0;
    const u32 seeker[]={0x8018f734,0,0,0,0,0,0,0,28,0,0,0,0x80190a44,0x80190acc};
    const u32 glint[]={0x80190574,0,0,0,0,0,0,0,8,0,0,0,0x80190a90,0x80190b24};
    if(memcmp(module+0x1afc,seeker,sizeof(seeker)) || memcmp(module+0x1b4c,glint,sizeof(glint)) ||
       memcmp(module+0x1adc,g_RoomM086SeekerScript,32) || memcmp(module+0x1b34,g_RoomM086GlintScript,24) ||
       memcmp(module+4,&g_RoomM086JointAnchor,8) || memcmp(module+12,&g_RoomM086LaunchVelocity,8))return 0;
    for(unsigned j=0x1b84;j<sizeof(module);++j)if(module[j])return 0;
    if(g_RoomM086SeekerProgram.table.callbacks[0]!=RoomM086_SeekerController || g_RoomM086SeekerProgram.table.sizes[0]!=28 ||
       g_RoomM086GlintProgram.table.callbacks[0]!=RoomM086_BounceGlint || g_RoomM086GlintProgram.table.sizes[0]!=8 ||
       g_RoomM086SeekerProgram.initialize!=RoomM086_SetControllerParams || g_RoomM086GlintProgram.initialize!=RoomM086_SetBounceGlintParams)return 0;
    size_t bytes=(size_t)range.roomSectors*2048;SceneAssetView *room=malloc(bytes);if(!room)return 0;
    int valid=pe_disc_read(disc,&archive,sector,room,bytes);
    if(valid){SceneRoomDirectory *directory=SceneAsset_ResolveOffset(room,room->header.directoryOffset);
        SceneRoomRecord *records=SceneAsset_ResolveOffset(room,directory->scripts&0x3fffff);unsigned mask=0;valid=directory->scripts>>22==2;
        for(unsigned j=0;valid && j<2;++j){valid=records[j].reserved[0]==0xa8 && records[j].reserved[1]==0x1b && (records[j].source.offsetAndSlot&0xffffff)==8;
            if(records[j].source.bytes.slot==120 && records[j].u.handlerAddress==0x80190aec)mask|=1;
            else if(records[j].source.bytes.slot==121 && records[j].u.handlerAddress==0x80190b3c)mask|=2;
            else valid=0;
        }valid=valid && mask==3;
    }
    if(valid){
        PmTables old_tables=g_PmTables;Pe1GameState old_game=g_GameState;u32 old_flags=g_GameStateFlags;
        void *old_programs[]={g_FieldFxTaskPrograms[35],g_FieldFxTaskPrograms[36]};
        FieldFxTaskContext *old_context=g_FieldFxTaskContext;FieldFxTaskSlot *old_task=g_FieldFxActiveTaskSlot;
        FieldFxObjectPrefix *old_owner=g_FieldFxOwner;FieldActor *old_player=g_PlayerEntity;
        RenderBufferPrefix old_buffers=D_800B0E38;GteMatrix *old_view=g_GeomScroll.words.viewMatrix;
        int old_age=g_FieldFxCurrentAge,old_draw=g_ActiveDrawSlot,old_offset=g_DrawPacketOffset,old_floor=g_RoomFloorY->y;
        u32 old_random=Psyz_RandState();u8 *old_model_a=g_RoomM086SeekerModelA,*old_model_b=g_RoomM086SeekerModelB;
        int old_params[7];memcpy(old_params,g_RoomM086ControllerParams,16);memcpy(old_params+4,g_RoomM086BounceGlintParams,12);
        static RenderGpuTag ordering[4096];static struct {_Alignas(void *) u8 bytes[262144];u8 guard[32];} packets;
        GteMatrix view={.m={{4096,0,0},{0,4096,0},{0,0,4096}}};
        g_GeomScroll.words.viewMatrix=&view;g_RoomFloorY->y=0;g_GameState.loaded_scene_archive=(void *)room;
        g_GameStateFlags=0x80;g_GameState.flags&=~8u;g_GameState.tim_load_state=0;
        g_FieldFxTaskPrograms[35]=&g_RoomM086SeekerProgram;g_FieldFxTaskPrograms[36]=&g_RoomM086GlintProgram;
        FieldFx_InitTexturePageTable();Psyz_GteCtrlWrite(26,256);Psyz_GteCtrlWrite(29,1365);Psyz_GteCtrlWrite(30,1024);gte_SetGeomOffset(160,112);
        for(unsigned program=0;valid && program<2;++program)for(unsigned draw=0;valid && draw<2;++draw){
            struct {PmPrimarySlot slots[11];u8 guard[32];} bank={0};memset(bank.guard,0xa5,32);
            FieldActor actor={0},player={0};FieldActorState state={0};Combatant player_state={0};RenderMatrix matrices[8]={0};u8 action=1;
            actor.state=&state;actor.render_object.matrices=matrices;state.action_state=&action;state.core_flags=0x01000000;
            player.state=(void *)&player_state;player.pos_x=8000*65536;player.pos_z=8000*65536;g_PlayerEntity=&player;
            for(unsigned j=0;j<8;++j){for(unsigned k=0;k<3;++k)matrices[j].rotation[k][k]=4096;matrices[j].translation[1]=-800;matrices[j].translation[2]=1200;}
            g_PmTables=(PmTables){g_PmCmdHandlerArray,bank.slots,NULL};srand(1);
            if(Scene_LoadRoomAssets(120+program,&actor)!=0 || Pm_SendCmd(0,0,1,4096,100,256) || (!program && Pm_SendCmd(0,0,2,4,0,0))){valid=0;break;}
            unsigned frames=0,draws=0,children=0;
            for(;frames<260;++frames){
                for(unsigned j=0;j<4096;++j)ordering[j]=(RenderGpuTag){.address=0xffffff};
                memset(packets.guard,0xa5,32);g_ActiveDrawSlot=draw;g_DrawPacketOffset=0;
                D_800B0E38.ordering[draw]=ordering;D_800B0E38.packets[draw]=(char *)packets.bytes;
                if(Pm_Start(0) || Pm_Exec(0)){valid=0;break;}
                draws+=g_DrawPacketOffset!=0;FieldFxTaskContext *ctx=&bank.slots[0].task.tasks;
                for(unsigned slot=0;slot<8;++slot)if(ctx->slots[slot].id!=0xffff && ctx->slots[slot].end){
                    FieldFxCallbackList *list=(void *)ctx->slots[slot].end;
                    for(unsigned j=0;j<(unsigned)list->count;++j)children+=((FieldFxCallbackListEntry *)(list->entries+j*list->stride))->active!=0;
                }
                if(ctx->used>sizeof(ctx->arena) || g_DrawPacketOffset<0 || g_DrawPacketOffset>sizeof(packets.bytes)){valid=0;break;}
                for(unsigned j=0;j<4096;++j){uintptr_t link=ordering[j].address;unsigned links=0;
                    while(link && link!=0xffffff){if(link<(uintptr_t)packets.bytes || link+sizeof(RenderGpuTag)>(uintptr_t)packets.bytes+g_DrawPacketOffset || ++links>4096){valid=0;break;}link=((RenderGpuTag *)link)->address;}
                    if(!valid)break;
                }
                for(unsigned j=0;j<32;++j)if(bank.guard[j]!=0xa5 || packets.guard[j]!=0xa5)valid=0;
                if(!valid || !bank.slots[0].header.state)break;
            }
            if(frames==260 || !draws || !children || bank.slots[0].header.state || bank.slots[0].task.tasks.count){fprintf(stderr,"M086 PM %u/%u frames%u draws%u children%u state%u count%u\n",program,draw,frames,draws,children,bank.slots[0].header.state,bank.slots[0].task.tasks.count);valid=0;}
        }
        g_PmTables=old_tables;g_GameState=old_game;g_GameStateFlags=old_flags;g_FieldFxTaskPrograms[35]=old_programs[0];g_FieldFxTaskPrograms[36]=old_programs[1];
        g_FieldFxTaskContext=old_context;g_FieldFxActiveTaskSlot=old_task;g_FieldFxOwner=old_owner;g_PlayerEntity=old_player;
        D_800B0E38=old_buffers;g_GeomScroll.words.viewMatrix=old_view;g_FieldFxCurrentAge=old_age;g_ActiveDrawSlot=old_draw;g_DrawPacketOffset=old_offset;g_RoomFloorY->y=old_floor;srand(old_random);
        g_RoomM086SeekerModelA=old_model_a;g_RoomM086SeekerModelB=old_model_b;memcpy(g_RoomM086ControllerParams,old_params,16);memcpy(g_RoomM086BounceGlintParams,old_params+4,12);
        if(valid)puts("Original M086: both authored task programs, actual USA models, both GPU buffers, complete nested pools, model/sprite draw chains, expiry and guards passed");
    }
    free(room);if(!valid)return 0;
    puts("Original M086 assets: entire 7080-byte module, both descriptors, all script words, native callback ABI, zero initial work data and both joint vectors passed");return 1;
}

static int original_three_particle_assets(PeDisc *disc,PeDiscFile *exe)
{
    u8 header[32],module[0x10b4];u32 base;PeDiscFile archive;
    if(!pe_disc_read(disc,exe,0,header,sizeof(header)) || !pe_disc_find(disc,"PE.IMG",&archive))return 0;
    memcpy(&base,header+24,4);
    const unsigned rooms[]={65,85};
    const u32 methods[]={0x8018f1b4,0x8018effc,0x8018f028,0x8018f054,0x8018f09c,0x8018f128,0x8018f1ac};
    const u32 lists[]={0x8018f1bc,0x8018f388,0xffffffff,0x8018f300,0x8018f828,0xffffffff,0x8018f308,0x8018fbcc,0xffffffff,0xb40030,0xffff};
    for(unsigned i=0;i<2;++i){
        SceneRoomSectorRange range;if(!pe_disc_read(disc,exe,0x800ull+0x80093378u-base+(rooms[i]-1)*8,&range,sizeof(range)))return 0;
        size_t bytes=(size_t)range.roomSectors*2048;SceneAssetView *room=malloc(bytes);if(!room)return 0;
        int valid=pe_disc_read(disc,&archive,(u64)(range.start+range.scratchSectors+range.textureSectors)*2048,room,bytes);
        if(valid){SceneRoomDirectory *directory=SceneAsset_ResolveOffset(room,room->header.directoryOffset);
            SceneRoomRecord *record=SceneAsset_ResolveOffset(room,directory->scripts&0x3fffff);
            valid=directory->scripts>>22==1 && record->reserved[0]==0xb4 && record->reserved[1]==0x10 && record->source.offsetAndSlot==0x0a000008 && record->u.handlerAddress==0x80190024;
            if(valid){memcpy(module,room->bytes+8,sizeof(module));uint64_t hash=UINT64_C(14695981039346656037);
                for(unsigned j=0;j<sizeof(module);++j)hash=(hash^module[j])*UINT64_C(1099511628211);
                valid=hash==UINT64_C(0x469c7423e8424e94) && !memcmp(module+0x1034,methods,sizeof(methods)) && !memcmp(module+0x1008,lists,sizeof(lists)) &&
                    !memcmp(module+0x1050,g_RoomThreeParticleScript,56) && !memcmp(module+0x1088,RoomLib_ThreeParticleTable,16);
            }
        }
        free(room);if(!valid)return 0;
    }
    /* M087 links this entire original class with relocated tables/work data,
     * and the same complete class E already verified in M075. */
    SceneRoomSectorRange range,reference_range;
    u8 relocated[0x4654],reference_e[0xaa4];u32 reference_jump[33];
    if(!pe_disc_read(disc,exe,0x800ull+0x80093378u-base+86*8,&range,sizeof(range)) ||
       !pe_disc_read(disc,exe,0x800ull+0x80093378u-base+74*8,&reference_range,sizeof(reference_range)))return 0;
    u64 sector=(u64)(range.start+range.scratchSectors+range.textureSectors)*2048;
    u64 reference_sector=(u64)(reference_range.start+reference_range.scratchSectors+reference_range.textureSectors)*2048;
    if(!pe_disc_read(disc,&archive,sector+8,relocated,sizeof(relocated)) ||
       !pe_disc_read(disc,&archive,reference_sector+8+0x2984,reference_e,sizeof(reference_e)) ||
       !pe_disc_read(disc,&archive,reference_sector+8+0x9c,reference_jump,sizeof(reference_jump)))return 0;
    uint64_t hash=UINT64_C(14695981039346656037);
    for(unsigned j=0;j<sizeof(relocated);++j)hash=(hash^relocated[j])*UINT64_C(1099511628211);
    if(hash!=UINT64_C(0x0d8472ecf5bd2d50) || memcmp(relocated,module,12))return 0;
    size_t bytes=(size_t)range.roomSectors*2048;SceneAssetView *room=malloc(bytes);if(!room)return 0;
    int valid=pe_disc_read(disc,&archive,sector,room,bytes);
    if(valid){SceneRoomDirectory *directory=SceneAsset_ResolveOffset(room,room->header.directoryOffset);
        SceneRoomRecord *record=SceneAsset_ResolveOffset(room,directory->scripts&0x3fffff);unsigned mask=0;
        valid=directory->scripts>>22==2;
        for(unsigned j=0;valid && j<2;++j){
            valid=record[j].reserved[0]==0x54 && record[j].reserved[1]==0x46 && (record[j].source.offsetAndSlot&0xffffff)==8;
            if(record[j].source.bytes.slot==10 && record[j].u.handlerAddress==0x80193518)mask|=1;
            else if(record[j].source.bytes.slot==75 && record[j].u.handlerAddress==0x801935b4)mask|=2;
            else valid=0;
        }
        valid=valid && mask==3;
    }
    free(room);if(!valid)return 0;
    const u16 data_reloc[]={0x14,0x7c,0xc8,0xdc,0xe4,0x980};
    const u16 work_reloc[]={0x264,0x270,0x27c,0x288,0x290,0x298,0x2a0,0x2a8,0x2b0,0x2b8,0x2c0,0x2c8,0x2d0,0x2d8,0x2e0,0x2e8,0x988,0x994,0x9b8,0xa94,0xb60};
    for(unsigned j=0;j<0xffc;j+=4){u32 expected,actual;memcpy(&expected,module+12+j,4);memcpy(&actual,relocated+0x1e4+j,4);
        unsigned opcode=expected>>26,target=(expected&0x3ffffff)<<2;
        if((opcode==2 || opcode==3) && target>=0x18effc && target<0x18fff8)expected+=0x1d8/4;
        for(unsigned k=0;k<sizeof(data_reloc)/sizeof(data_reloc[0]);++k)if(j==data_reloc[k])expected=(expected&0xffff0000)|(u16)(expected+0x34f4);
        for(unsigned k=0;k<sizeof(work_reloc)/sizeof(work_reloc[0]);++k)if(j==work_reloc[k])expected=(expected&0xffff0000)|(u16)(expected+0x35a0);
        if(expected!=actual)return 0;
    }
    for(unsigned j=0;j<7;++j){u32 actual;memcpy(&actual,relocated+0x4528+j*4,4);if(actual!=methods[j]+0x1d8)return 0;}
    for(unsigned j=0;j<11;++j){u32 actual,expected=lists[j];memcpy(&actual,relocated+0x44fc+j*4,4);
        if(j<9 && expected!=0xffffffff)expected+=0x1d8;
        if(actual!=expected)return 0;
    }
    if(memcmp(relocated+0x4544,g_RoomThreeParticleScript,56) || memcmp(relocated+0x457c,RoomLib_ThreeParticleTable,16))return 0;
    for(unsigned j=0x4638;j<0x4654;++j)if(relocated[j])return 0;
    const u32 class_e[]={0x80190f40,0x80190f48,0x80190fc4,0x8019123c,0x80191244,0x80191984,0x801919dc};
    if(memcmp(relocated+0x45c4,class_e,sizeof(class_e)))return 0;
    for(unsigned j=0;j<sizeof(reference_e);j+=4){u32 expected,actual;memcpy(&expected,reference_e+j,4);memcpy(&actual,relocated+0x1f50+j,4);
        unsigned opcode=expected>>26,target=(expected&0x3ffffff)<<2;
        if((opcode==2 || opcode==3) && target>=0x191974 && target<0x192418)expected-=0xa34/4;
        if(j==0x34 || j==0x260 || j==0x418)expected=(expected&0xffff0000)|(u16)(expected-0xa34);
        if(j==0xa4)expected=(expected&0xffff0000)|0xf07c;
        if(actual!=expected)return 0;
    }
    for(unsigned j=0;j<33;++j){u32 actual;memcpy(&actual,relocated+0x8c+j*4,4);if(actual!=reference_jump[j]-0xa34)return 0;}
    puts("Original M087 assets: complete library, all 1023 three-particle instructions with only audited relocations, identical script/color/seed, zero work templates, entire class E and all 33 Configure targets passed");
    if(g_RoomThreeParticleSpawnLayout[0]!=sizeof(RoomDebrisClock) || g_RoomThreeParticleSpawnLayout[1]!=180 || g_RoomThreeParticleSpawnLayout[2]!=65535)return 0;
    puts("Original M065/M085 assets: entire identical 4276-byte module, seven methods, six callbacks, both PS1 allocation sizes, complete script and color table passed");
    return 1;
}

static int original_motion_assets(PeDisc *disc,PeDiscFile *exe)
{
    u8 header[0x800];u32 base;PeDiscFile archive;SceneRoomSectorRange range;
    if(!pe_disc_read(disc,exe,0,header,sizeof(header)) || !pe_disc_find(disc,"PE.IMG",&archive))return 0;
    memcpy(&base,header+0x18,4);
    const unsigned rooms[]={75,80,82};
    for(unsigned room_index=0;room_index<3;++room_index){
        if(!pe_disc_read(disc,exe,0x800ull+0x80093378u-base+(rooms[room_index]-1)*8,&range,sizeof(range)))return 0;
        size_t bytes=(size_t)range.roomSectors*2048;u8 *blob=malloc(bytes);if(!blob)return 0;
        int valid=bytes>=8+0x5664 && pe_disc_read(disc,&archive,(u64)(range.start+range.scratchSectors+range.textureSectors)*2048,blob,bytes);
        if(valid){
            SceneAssetView *room=(void *)blob;SceneRoomDirectory *directory=SceneAsset_ResolveOffset(room,room->header.directoryOffset);
            SceneRoomRecord *records=SceneAsset_ResolveOffset(room,directory->scripts&0x3fffff);unsigned descriptors=0;
            valid=directory->scripts>>22==2;
            for(unsigned i=0;valid && i<2;++i){
                valid=(records[i].source.offsetAndSlot&0xffffff)==8;
                if(records[i].source.bytes.slot==75 && records[i].u.handlerAddress==0x80194048)descriptors|=1;
                else if(records[i].source.bytes.slot==12 && records[i].u.handlerAddress==0x80193f74)descriptors|=2;
                else valid=0;
            }
            valid=valid && descriptors==3;
            uint64_t hash=UINT64_C(14695981039346656037);for(unsigned i=0;i<0x5664;++i)hash=(hash^blob[8+i])*UINT64_C(1099511628211);
            valid=valid && hash==UINT64_C(0x9b37f5c19cfede02);
            const u32 init[]={0x8018f3dc,0x8018f79c,0x8018fbe8,0x8018ff74,0x80190884,0xffffffff};
            const u32 draw[]={0x8018f714,0x8018f814,0x8018fc60,0x80190410,0x80190af4,0xffffffff};
            const u32 update[]={0x8018f71c,0x8018fb44,0x8018ff10,0x801906d0,0x80190b54,0xffffffff};
            const u32 methods[]={0x8018f3d4,0x8018f1e4,0x8018f210,0x8018f23c,0x8018f284,0x8018f348,0x8018f3cc};
            const u32 class_e_methods[]={0x80191974,0x8019197c,0x801919f8,0x80191c70,0x80191c78,0x801923b8,0x80192410};
            u8 *code=blob+8;
            valid=valid && !memcmp(code+0x4f30,init,sizeof(init)) && !memcmp(code+0x4f48,draw,sizeof(draw)) &&
                !memcmp(code+0x4f60,update,sizeof(update)) && !memcmp(code+0x4f84,methods,sizeof(methods)) &&
                !memcmp(code+0x5058,class_e_methods,sizeof(class_e_methods)) &&
                !memcmp(code+0x4fa0,g_RoomScript__motion,30*4) && !memcmp(code+0x5018,g_RoomMotionLookupCodes,8);
            /* Compare the entire already-ported actor class, including its
             * 33-way Configure table, allowing only these actual relocations. */
            u32 reference[0xaa4/4],jump[33];SceneRoomSectorRange ref;
            if(!pe_disc_read(disc,exe,0x800ull+0x80093378u-base+58*8,&ref,sizeof(ref)) ||
               !pe_disc_read(disc,&archive,(u64)(ref.start+ref.scratchSectors+ref.textureSectors)*2048+8+0xf4c,reference,sizeof(reference)) ||
               !pe_disc_read(disc,&archive,(u64)(ref.start+ref.scratchSectors+ref.textureSectors)*2048+8+0x84,jump,sizeof(jump)))valid=0;
            for(unsigned i=0;valid && i<sizeof(reference)/4;++i){
                u32 expected=reference[i],actual;memcpy(&actual,code+0x2984+i*4,4);
                if((expected>>26==2 || expected>>26==3) && ((expected&0x3ffffff)<<2)>=0x18ff3c && ((expected&0x3ffffff)<<2)<0x1909e0)expected+=0x1a38/4;
                if(i*4==0x34 || i*4==0x260 || i*4==0x418)expected+=0x1a38;
                if(i*4==0xa4)expected=(expected&0xffff0000)|0xf08c;
                valid=expected==actual;
            }
            for(unsigned i=0;valid && i<33;++i){u32 actual;memcpy(&actual,code+0x9c+i*4,4);valid=actual==jump[i]+0x1a38;}
        }
        free(blob);
        if(!valid)return 0;
    }
    puts("Original M075/M080/M082 assets: complete 22116-byte library, fifteen motion functions, PM descriptor, five callbacks per list, 30-word program and relocated entire class E passed");
    return 1;
}

static int original_window_assets(PeDisc *disc,PeDiscFile *exe)
{
    u8 header[0x800],module[0xe08],candidate[0xe08];u32 base;PeDiscFile archive;
    if(!pe_disc_read(disc,exe,0,header,sizeof(header)) || !pe_disc_find(disc,"PE.IMG",&archive))return 0;
    memcpy(&base,header+0x18,4);
    const unsigned rooms[]={63,83};const u32 methods[]={0x8018f054,0x8018f05c,0x8018f0d0,0x8018f248,0x8018f250,0x8018fd5c,0x8018fdd4};
    for(unsigned i=0;i<2;++i){
        SceneRoomSectorRange range;
        if(!pe_disc_read(disc,exe,0x800ull+0x80093378u-base+(rooms[i]-1)*8,&range,sizeof(range)))return 0;
        size_t bytes=(size_t)range.roomSectors*2048;SceneAssetView *room=malloc(bytes);if(!room)return 0;
        int valid=pe_disc_read(disc,&archive,(u64)(range.start+range.scratchSectors+range.textureSectors)*2048,room,bytes);
        if(valid){
            SceneRoomDirectory *directory=SceneAsset_ResolveOffset(room,room->header.directoryOffset);
            SceneRoomRecord *record=SceneAsset_ResolveOffset(room,directory->scripts&0x3fffff);
            size_t offset=record->source.offsetAndSlot&0xffffff;
            valid=directory->scripts>>22==1 && record->source.bytes.slot==74 && record->u.handlerAddress==0x8018fddc && offset<=bytes && sizeof(candidate)<=bytes-offset;
            if(valid){memcpy(candidate,(u8 *)room+offset,sizeof(candidate));
                valid=!memcmp(candidate+0xdec,methods,sizeof(methods));
                if(i)valid=valid && !memcmp(module,candidate,sizeof(module));else memcpy(module,candidate,sizeof(module));
            }
        }
        free(room);if(!valid)return 0;
    }
    /* The combined library moves the window jump table before the actor
     * code. Compare every instruction, allowing only the audited relocations,
     * and compare the entire class-E body and both method tables. */
    u32 class_e[0xaa4/4];
    SceneRoomSectorRange reference_range;
    if(!pe_disc_read(disc,exe,0x800ull+0x80093378u-base+58*8,&reference_range,sizeof(reference_range)) ||
       !pe_disc_read(disc,&archive,(u64)(reference_range.start+reference_range.scratchSectors+reference_range.textureSectors)*2048+8+0xf4c,class_e,sizeof(class_e)))return 0;
    /* Audit every row, including the five actor classes not exported by
     * these rooms' two descriptors. Their shared native handlers are linked
     * through actor_class_table.c; no separate replacement table is needed. */
    u32 actor_methods[6][7];
    SceneRoomSectorRange actor_range;
    if(!pe_disc_read(disc,exe,0x800ull+0x80093378u-base+21*8,&actor_range,sizeof(actor_range)) ||
       !pe_disc_read(disc,&archive,(u64)(actor_range.start+actor_range.scratchSectors+actor_range.textureSectors)*2048+
                     8+0x801924e8u-0x8018eff0u,actor_methods,sizeof(actor_methods)))return 0;
    const unsigned combined_rooms[]={358,384,387};
    const u32 e_methods[]={0x8018ff9c,0x8018ffa4,0x80190020,0x80190298,0x801902a0,0x801909e0,0x80190a38};
    for(unsigned room_index=0;room_index<3;++room_index){
        SceneRoomSectorRange range;
        if(!pe_disc_read(disc,exe,0x800ull+0x80093378u-base+(combined_rooms[room_index]-1)*8,&range,sizeof(range)))return 0;
        size_t bytes=(size_t)range.roomSectors*2048;SceneAssetView *room=malloc(bytes);if(!room)return 0;
        int valid=pe_disc_read(disc,&archive,(u64)(range.start+range.scratchSectors+range.textureSectors)*2048,room,bytes);
        if(valid){
            SceneRoomDirectory *directory=SceneAsset_ResolveOffset(room,room->header.directoryOffset);
            SceneRoomRecord *records=SceneAsset_ResolveOffset(room,directory->scripts&0x3fffff);
            valid=directory->scripts>>22==2 && bytes>=8+0x43a4;
            unsigned descriptors=0;
            for(unsigned i=0;valid && i<2;++i){
                valid=records[i].source.offsetAndSlot%0x1000000==8;
                if(records[i].source.bytes.slot==74 && records[i].u.handlerAddress==0x80193378)descriptors|=1;
                else if(records[i].source.bytes.slot==75 && records[i].u.handlerAddress==0x80193308)descriptors|=2;
                else valid=0;
            }
            valid=valid && descriptors==3;
            u8 *code=(u8 *)room+8;
            for(unsigned offset=0x64;valid && offset<0xdec;offset+=4){
                u32 expected,actual;memcpy(&expected,module+offset,4);memcpy(&actual,code+0x34f4+offset,4);
                if((expected>>26==2 || expected>>26==3) &&
                   ((expected&0x3ffffff)<<2)>=0x18eff0 && ((expected&0x3ffffff)<<2)<0x18fddc)
                    expected+=0x34f4/4;
                if(offset==0x98 || offset==0x210 || offset==0x320 || offset==0x904)
                    expected=(expected&0xffff0000)|((expected+0x34f4)&0xffff);
                if(offset==0x100)expected=(expected&0xffff0000)|0xf1cc;
                valid=actual==expected;
            }
            for(unsigned i=0;valid && i<24;++i){
                u32 expected,actual;memcpy(&expected,module+4+i*4,4);memcpy(&actual,code+0x1dc+i*4,4);
                valid=actual==expected+0x34f4;
            }
            for(unsigned i=0;valid && i<7;++i){
                u32 actual;memcpy(&actual,code+0x4388+i*4,4);valid=actual==methods[i]+0x34f4;
            }
            for(unsigned row=0;valid && row<6;++row)for(unsigned method=0;valid && method<7;++method){
                u32 actual;memcpy(&actual,code+0x42e0+(row*7+method)*4,4);
                valid=actual==actor_methods[row][method]+0x60;
            }
            valid=valid && !memcmp(code+0x4318,e_methods,sizeof(e_methods));
            for(unsigned i=0;valid && i<sizeof(class_e)/sizeof(*class_e);++i){
                u32 expected=class_e[i],actual;memcpy(&actual,code+0xfac+i*4,4);
                if((expected>>26==2 || expected>>26==3) &&
                   ((expected&0x3ffffff)<<2)>=0x18ff3c && ((expected&0x3ffffff)<<2)<0x1909e0)
                    expected+=0x60/4;
                if(i*4==0x34 || i*4==0x260 || i*4==0x418)expected+=0x60;
                valid=actual==expected;
            }
        }
        free(room);if(!valid)return 0;
    }
    puts("Original window assets: M063/M083 and combined M358/M384/M387 window/class-E bodies, all 49 combined class methods, jump tables and descriptors passed");return 1;
}

static int original_window_motion(void)
{
    const s32 travels[]={65536,-65536}, steps[]={0,8192}, scales[]={1024,4096}, spins[]={0,0x9000000};
    const u16 yaws[]={0,1024,3072};
    uint64_t hash=UINT64_C(14695981039346656037);unsigned cases=0;
    FieldActor *saved=g_PlayerEntity;
    for(unsigned started=0;started<2;++started)for(unsigned yi=0;yi<3;++yi)
    for(unsigned ti=0;ti<2;++ti)for(unsigned di=0;di<2;++di)
    for(unsigned si=0;si<2;++si)for(unsigned pi=0;pi<2;++pi)for(unsigned target=0;target<2;++target){
        RoomWindowObject object={0};FieldActor actor={0},anchor={0},player={0};
        FieldActorState marker={0},core={0};u8 marker_state[4]={0};
        object.actor=(RoomWindowActor *)&actor;actor.parent=&anchor;anchor.state=&marker;
        marker.action_state=marker_state;player.state=&core;g_PlayerEntity=&player;
        RoomWindowState *state=&object.state;state->started=started;
        anchor.rot_y=yaws[yi];anchor.pos_x=100*65536;anchor.pos_z=-200*65536;
        state->target[0]=900*65536;state->target[1]=-30*65536;state->target[2]=200*65536;
        state->travel=travels[ti];state->step=steps[di];state->scale=scales[si];state->spinSpeed=0x1200000;state->spin=spins[pi];
        state->offset=(GteShortVector){-30,20,15,0};
        for(unsigned i=0;i<3;++i)state->rotation.m[i][i]=4096;
        if(target)state->targetNode=(RoomWindowTarget *)&player;
        for(unsigned i=0;i<32;++i){Psyz_GteDataWrite(i,0);Psyz_GteCtrlWrite(i,0);}
        RoomLib_WindowHandler(&object);
        u32 words[40];unsigned count=0;
        for(unsigned i=0;i<3;++i)words[count++]=state->pos[i].value;
        for(unsigned i=0;i<3;++i)words[count++]=state->target[i];
        for(unsigned i=0;i<3;++i)words[count++]=state->origin[i].value;
        words[count++]=state->travel;words[count++]=state->step;words[count++]=state->scale;words[count++]=state->spinSpeed;words[count++]=state->spin;
        words[count++]=(u16)state->angles.x;words[count++]=(u16)state->angles.y;words[count++]=(u16)state->angles.z;words[count++]=(u16)state->touched;
        words[count++]=actor.pos_x;words[count++]=actor.pos_y;words[count++]=actor.pos_z;
        for(unsigned i=0;i<3;++i)words[count++]=actor.render_object.model_matrix.translation[i];
        for(unsigned i=0;i<9;++i)words[count++]=(u16)actor.render_object.model_matrix.rotation[i/3][i%3];
        words[count++]=actor.flags;words[count++]=actor.render_object.flags_9C;words[count++]=marker.core_flags;words[count++]=core.flags;words[count++]=state->started;words[count++]=state->callback==RoomLib_WindowArrived;
        for(unsigned i=0;i<count;++i)for(unsigned byte=0;byte<4;++byte)hash=(hash^((words[i]>>(byte*8))&255))*UINT64_C(1099511628211);
        ++cases;
    }
    g_PlayerEntity=saved;
    if(hash!=UINT64_C(0xb938fce337189cd5)){fprintf(stderr,"Original window motion differs: %u %016llx\n",cases,(unsigned long long)hash);return 0;}
    puts("Original window motion: 192 USA CPU/COP2 bridge states passed");return 1;
}

static int original_room_actor_assets(PeDisc *disc,PeDiscFile *exe)
{
    u8 header[0x800];u32 base;SceneRoomSectorRange range;PeDiscFile archive;
    if(!pe_disc_read(disc,exe,0,header,sizeof(header)))return 0;
    memcpy(&base,header+0x18,4);
    s16 trig[4096][2];
    if(!pe_disc_read(disc,exe,0x800ull+0x800966ecu-base,trig,sizeof(trig)))return 0;
    for(unsigned angle=0;angle<4096;++angle)
        if(rsin(angle)!=trig[angle][0] || rcos(angle)!=trig[angle][1])return 0;
    if(!pe_disc_read(disc,exe,0x800ull+0x80093378u-base+21*8,&range,sizeof(range)) ||
       !pe_disc_find(disc,"PE.IMG",&archive))return 0;
    size_t bytes=(size_t)range.roomSectors*2048;SceneAssetView *room=malloc(bytes);
    if(!room)return 0;
    int valid=pe_disc_read(disc,&archive,(u64)(range.start+range.scratchSectors+range.textureSectors)*2048,room,bytes);
    if(valid){
        SceneRoomDirectory *directory=SceneAsset_ResolveOffset(room,room->header.directoryOffset);
        SceneRoomRecord *record=SceneAsset_ResolveOffset(room,directory->scripts&0x3fffff);
        valid=directory->scripts>>22==1 && record->source.bytes.slot==70 &&
            record->u.handlerAddress==0x801924e8u;
        static const u32 methods[6][7]={
            {0x8018f1cc,0x8018f1d4,0x8018f274,0x8018f4f8,0x8018f500,0x8018fe98,0x8018ff34},
            {0x8018f1cc,0x8018f1d4,0x8018f274,0x8018f4f8,0x8018f500,0x8018fe98,0x8018ff34},
            {0x8018ff3c,0x8018ff44,0x8018ffc0,0x80190238,0x80190240,0x80190980,0x801909d8},
            {0x801909e0,0x801909e8,0x80190a84,0x80190ccc,0x80190cd4,0x801914a0,0x801914fc},
            {0x80191504,0x8019150c,0x8019159c,0x801917e4,0x801917ec,0x80192090,0x801920ec},
            {0x801920f4,0x801920fc,0x80192138,0x801921d0,0x801921d8,0x801924c4,0x801924e0}};
        size_t offset=(record->source.offsetAndSlot&0xffffff)+0x801924e8u-0x8018eff0u;
        valid=valid && offset<=bytes && sizeof(methods)<=bytes-offset &&
            !memcmp((u8 *)room+offset,methods,sizeof(methods));
        const struct {u16 room;u8 classes;} libraries[]={
            {22,1}, {32,16}, {59,4}, {61,4}, {62,4}, {66,4}, {67,4},
            {78,4}, {81,4}, {197,5}, {198,5}, {199,5}, {201,4}, {206,5},
            {207,5}, {210,5}, {214,5}, {217,5}, {218,4}, {219,5}, {220,5},
            {226,5}, {227,5}, {232,4}, {386,4}, {405,21}, {407,17}, {408,21},
            {409,21}, {436,5}
        };
        unsigned descriptors=0;
        for(unsigned m=0;m<sizeof(libraries)/sizeof(libraries[0]) && valid;++m){
            SceneRoomSectorRange linked_range;
            valid=pe_disc_read(disc,exe,0x800ull+0x80093378u-base+(libraries[m].room-1)*8,&linked_range,sizeof(linked_range));
            if(!valid)break;
            size_t linked_bytes=(size_t)linked_range.roomSectors*2048;
            SceneAssetView *linked=malloc(linked_bytes);
            if(!linked){free(room);return 0;}
            valid=bytes>=0x35a8 && linked_bytes>=0x35a8 &&
                pe_disc_read(disc,&archive,(u64)(linked_range.start+linked_range.scratchSectors+linked_range.textureSectors)*2048,linked,linked_bytes);
            if(valid){
                SceneRoomDirectory *linked_directory=SceneAsset_ResolveOffset(linked,linked->header.directoryOffset);
                SceneRoomRecord *linked_record=SceneAsset_ResolveOffset(linked,linked_directory->scripts&0x3fffff);
                unsigned found=0;
                valid=!memcmp((u8 *)room+8,(u8 *)linked+8,0x35a0);
                for(unsigned j=0;j<linked_directory->scripts>>22 && valid;++j){
                    unsigned class_id;
                    switch(linked_record[j].source.bytes.slot){case 70:class_id=0;break;case 75:class_id=2;break;case 73:class_id=4;break;default:valid=0;continue;}
                    valid=!(found&(1u<<class_id)) && linked_record[j].u.handlerAddress==0x801924e8u+class_id*28;
                    found|=1u<<class_id;++descriptors;
                }
                valid=valid && found==libraries[m].classes;
            }
            free(linked);
        }
        valid=valid && descriptors==50;
    }
    free(room);
    if(valid)puts("Original room actor assets: 29 identical complete actor modules, all 48 class descriptors, all 42 class methods and 4096 USA sine/cosine entries verified");
    return valid;
}

static int original_scene_object_workspace_assets(PeDisc *disc, PeDiscFile *exe)
{
    u8 header[0x800];u32 base;u16 sectors[81];PeDiscFile archive;
    if(!pe_disc_read(disc,exe,0,header,sizeof(header)))return 0;
    memcpy(&base,header+0x18,4);
    if(!pe_disc_read(disc,exe,0x800ull+0x800930d8u-base,sectors,sizeof(sectors)) ||
       !pe_disc_find(disc,"PE.IMG",&archive))return 0;
    Pe1GameState saved=g_GameState;Boot_InitMemoryLayout();
    size_t capacity=(u8 *)g_GameState.render_buffers.ordering[0]-g_GameState.scene_object_work;
    g_GameState=saved;
    u8 *workspace=malloc(capacity+0x1000*sizeof(RenderNativeTag));
    if(!workspace)return 0;
    unsigned checked=0;size_t maximum=0;int valid=1;
    for(unsigned bank=23;bank<=38 && valid;++bank){
        size_t bytes=(size_t)(sectors[bank+1]-sectors[bank])*2048;
        SceneAssetView *view=malloc(bytes);
        if(!view){valid=0;break;}
        valid=sectors[bank+1]>sectors[bank] && pe_disc_read(disc,&archive,(u64)sectors[bank]*2048,view,bytes);
        if(valid){
            SceneAssetDirectory *directory=SceneAsset_ResolveOffset(view,view->header.directoryOffset);
            SceneRoomRecord *root=SceneAsset_ResolveOffset(view,directory->bankRootEntries&0x3fffff);
            valid=directory->bankRootEntries>>22==1;
            RenderObjectHeader *model=SceneAsset_ResolveOffset(view,root->source.offsetAndSlot&0xffffff);
            size_t needed=2*(model->packet34_count*sizeof(RenderPacket34)+model->packet28_count*sizeof(RenderPacket28)+
                model->packet24_count*sizeof(RenderPacket24)+model->packet1c_count*sizeof(RenderPacket1C))+
                model->animation_entry_count*sizeof(RenderAnimationLookupEntry)+model->part_count*sizeof(RenderMatrix);
            if(needed>capacity){fprintf(stderr,"Scene backdrop bank %u needs %zu bytes before OT, only %zu reserved\n",bank,needed,capacity);valid=0;}
            if(valid){
                RenderObjectEntity object={0};s8 *texture=NULL;
                memset(workspace,0xa5,capacity+0x1000*sizeof(RenderNativeTag));
                valid=Render_SetupEntityPrims(&object,model,workspace,0x2c0,0x80,0,0x1c2,0,&texture,1)==1 &&
                    (u8 *)(object.matrices+model->part_count)<=workspace+capacity;
                for(size_t i=capacity;i<capacity+0x1000*sizeof(RenderNativeTag);++i)
                    if(workspace[i]!=0xa5){valid=0;break;}
                if(needed>maximum)maximum=needed;
                ++checked;
            }
        }
        free(view);
    }
    free(workspace);
    if(valid && checked==16)printf("Original scene backdrop workspace: all 16 USA banks fit before native OT, max %zu/%zu bytes, whole OT guards intact\n",maximum,capacity);
    return valid && checked==16;
}

static int original_effect_sequence_assets(PeDisc *disc, PeDiscFile *exe)
{
    const unsigned rooms[]={49,50,90,344,374};
    const u32 methods[]={0x8018f034,0x8018f03c,0x8018f068,0x8018f094,0x8018f0bc,0x8018f12c,0x8018f134};
    const u32 lists[3][8]={
        {0x8018f13c,0x801903a8,0x80190bf8,0x801914f0,0x80191c94,0x80192494,0x80192688,0xffffffff},
        {0x8018f36c,0x80190428,0x80190cb0,0x801915a8,0x80191d44,0x8019249c,0x80192794,0xffffffff},
        {0x8018f374,0x80190868,0x80191140,0x80191958,0x80192178,0x80192680,0x801929b0,0xffffffff}};
    const u16 sizes[]={0x28,0x38,0x38,0x38,0x38,0x38,0x38,0xffff};
    const u32 script[]={0x10000,0xffffffff};
    u8 header[0x800],baseline[0x3c54];u32 base;PeDiscFile archive;
    if(!pe_disc_read(disc,exe,0,header,sizeof(header)) || !pe_disc_find(disc,"PE.IMG",&archive))return 0;
    memcpy(&base,header+0x18,4);
    for(unsigned i=0;i<5;++i){
        SceneRoomSectorRange range;
        if(!pe_disc_read(disc,exe,0x800ull+0x80093378u-base+(rooms[i]-1)*8,&range,sizeof(range)))return 0;
        size_t bytes=(size_t)range.roomSectors*2048;SceneAssetView *room=malloc(bytes);
        if(!room)return 0;
        int valid=bytes>=sizeof(baseline)+8 && pe_disc_read(disc,&archive,
            (u64)(range.start+range.scratchSectors+range.textureSectors)*2048,room,bytes);
        if(valid){
            const u8 *body=(const u8 *)room+8;
            if(!i)memcpy(baseline,body,sizeof(baseline));
            SceneRoomDirectory *directory=SceneAsset_ResolveOffset(room,room->header.directoryOffset);
            SceneRoomRecord *record=SceneAsset_ResolveOffset(room,directory->scripts&0x3fffff);
            valid=!memcmp(body,baseline,sizeof(baseline)) && directory->scripts>>22==1 &&
                record->source.bytes.slot==14 && record->u.handlerAddress==0x80192bcc &&
                !memcmp(body+0x80192bcc-0x8018eff0,methods,sizeof(methods)) &&
                !memcmp(body+0x80192b5c-0x8018eff0,lists,sizeof(lists)) &&
                !memcmp(body+0x80192bbc-0x8018eff0,sizes,sizeof(sizes)) &&
                !memcmp(body+0x80192be8-0x8018eff0,script,sizeof(script)) &&
                !memcmp(body+0x80192bf0-0x8018eff0,g_EffectSequenceAmberGlowRamp,12) &&
                !memcmp(body+0x80192bfc-0x8018eff0,g_EffectSequenceLimeGlowRamp,12);
        }
        free(room);if(!valid)return 0;
    }
    if(g_RoomEffectSequenceSizes[0]<sizeof(void *)+36 || g_RoomEffectSequenceSizes[0]%sizeof(void *))return 0;
    for(unsigned i=1;i<8;++i)if(g_RoomEffectSequenceSizes[i]!=sizes[i])return 0;
    puts("Original effect sequence: all five USA modules, seven PM methods, 21 callbacks, sizes, script and colour ramps verified");
    return 1;
}

static int original_effect_sequence_initializers(void)
{
    const u32 seeds[]={0,1,0x12345678,0xffffffff};
    const u8 fills[]={0,1,0xa5,0xff};
    const int values[]={-32768,-1,0,1,13,32767,65539};
    FieldFxState state={0};FieldFxState *saved=g_FieldFxScriptState;
    u32 random=Psyz_RandState();uint64_t hash=UINT64_C(14695981039346656037);unsigned cases=0;
    _Alignas(void *) u8 data[512];g_FieldFxScriptState=&state;
    for(unsigned handler=1;handler<=4;++handler)for(unsigned seed=0;seed<4;++seed)
    for(unsigned fill=0;fill<4;++fill)for(unsigned index=0;index<7;++index)
    for(unsigned selector=0;selector<7;++selector){
        srand(seeds[seed]);state.regs[1]=values[index];state.regs[2]=values[selector];
        memset(data,fills[fill],sizeof(data));
        g_RoomEffectSequenceInitList[handler](NULL,NULL,data);
        for(unsigned i=0;i<sizeof(data);++i)hash=(hash^data[i])*UINT64_C(1099511628211);
        ++cases;
    }
    srand(random);g_FieldFxScriptState=saved;
    if(cases!=3136 || hash!=UINT64_C(0xa5f62be2508a87fd)){
        fprintf(stderr,"Effect sequence initializer CPU hash: %u %016llx\n",cases,(unsigned long long)hash);return 0;}
    puts("Original effect sequence: 3136 unhooked USA MIPS initializer states, seeded randomness, signed register narrowing and whole-buffer guards passed");
    return 1;
}

static int original_effect_sequence_updates(void)
{
    const u32 seeds[]={0,1,0x12345678,0xffffffff};const u8 fills[]={0,1,0xa5,0xff};
    const s16 ages[]={-32768,-1,0,1,29,30,299,300,32767};
    const int coordinates[]={-32768,32767,40000};
    FieldFxState script={0};FieldFxState *saved=g_FieldFxScriptState;
    RoomFxTransform matrices[14]={0};RoomFxTransformOwner actor={.transforms=matrices};
    RoomFxTransformOwner *owner=&actor;script.current_context=&owner;
    u32 random=Psyz_RandState();uint64_t hash=UINT64_C(14695981039346656037);unsigned cases=0;
    _Alignas(void *) u8 data[512];RoomFxControl control;
    g_FieldFxScriptState=&script;
    for(unsigned handler=1;handler<=3;++handler)for(unsigned seed=0;seed<4;++seed)
    for(unsigned fill=0;fill<4;++fill)for(unsigned age=0;age<9;++age)
    for(unsigned index=0;index<=13;index+=13)for(unsigned coordinate=0;coordinate<3;++coordinate){
        srand(seeds[seed]);memset(data,fills[fill],sizeof(data));
        ((RoomFxPairedSpriteState *)data)->transformIndex=index;
        memset(&control,0xa5,sizeof(control));control.frame=ages[age];
        matrices[index].x=coordinates[coordinate];matrices[index].y=coordinates[coordinate]+1;
        matrices[index].z=coordinates[coordinate]+2;
        g_RoomEffectSequenceUpdateList[handler](NULL,&control,data);
        for(unsigned i=0;i<sizeof(data);++i)hash=(hash^data[i])*UINT64_C(1099511628211);
        for(unsigned i=0;i<sizeof(control);++i)hash=(hash^((u8 *)&control)[i])*UINT64_C(1099511628211);
        ++cases;
    }
    srand(random);g_FieldFxScriptState=saved;
    if(cases!=2592 || hash!=UINT64_C(0xf7d6259fe0896699)){
        fprintf(stderr,"Effect sequence update CPU hash: %u %016llx\n",cases,(unsigned long long)hash);return 0;}
    puts("Original effect sequence: 2592 unhooked USA MIPS sprite update states, wide native owners, matrix indices 0/13, signed timers and whole-buffer guards passed");
    return 1;
}

static int original_contact_action_callback(void)
{
    static const s8 patterns[5][4]={{3,-1,7,1},{0,1,3,7},{7,3,-1,1},{-1,3,3,3},{1,1,1,3}};
    const int health[]={-1,0,1,100},actions[]={0,1,3,7,255,65539,-65533};
    uint64_t hash=UINT64_C(14695981039346656037);unsigned cases=0;
    for(unsigned effect=0;effect<6;++effect)for(unsigned kind=0;kind<8;++kind)
    for(unsigned hp=0;hp<4;++hp)for(unsigned pattern=0;pattern<5;++pattern)
    for(unsigned action=0;action<7;++action){
        EnemyCombatant enemy;Combatant player;EnemyActionEffect event={0};
        memset(&enemy,0xa5,sizeof(enemy));memset(&player,0x5a,sizeof(player));
        enemy.coreFlags=(0x105a55aau&~0xe00000u)|(kind<<21);enemy.hpAlive=health[hp];
        enemy.effect=effect?&event:NULL;event.state=effect?effect-1:0;
        for(unsigned group=0;group<3;++group)memcpy(enemy.contactActionIds+group*4,patterns[pattern],4);
        player.stateFlags=0xa5a55a5a;
        EnemyCombatant expected_enemy=enemy;Combatant expected_player=player;
        FieldActor self={.state=(FieldActorState *)&player,.contact_callback=Entity_CheckActionIdMatch};
        FieldActor other={.state=effect==5?NULL:(FieldActorState *)&enemy};
        self.contact_callback(&self,27,&other,actions[action]);
        const u32 words[]={player.stateFlags,enemy.coreFlags};
        for(unsigned i=0;i<2;++i)for(unsigned byte=0;byte<4;++byte)
            hash=(hash^((words[i]>>(byte*8))&255))*UINT64_C(1099511628211);
        expected_enemy.coreFlags=enemy.coreFlags;expected_player.stateFlags=player.stateFlags;
        if(memcmp(&enemy,&expected_enemy,sizeof(enemy)) || memcmp(&player,&expected_player,sizeof(player)))return 0;
        ++cases;
    }
    if(hash!=UINT64_C(0xa5fd542478aeafa5)){
        fprintf(stderr,"Contact action CPU hash differs: %u %016llx\n",cases,(unsigned long long)hash);return 0;}
    puts("Original contact action callback: 6720 USA MIPS states, null core/effect, inactive/dead enemies, facing groups, terminators and signed part IDs passed");
    return 1;
}

static int original_room_d_update(void)
{
    const u32 patterns[]={0,1,0xffffffff,65536,0xffff0000,0x100000,0xfff00000,0x7fffffff,0x80000000,0x12345678,0xfedcba98,0x08000000};
    const u32 phases[]={0,0x4000000,0x8000000,0xc010000,0x7fffffff,0x80000000};
    const u32 motions[]={0,65536,0xffff0000,0x7fffffff,0x80000000,0x12345678};
    const s16 matrices[][9]={{4096,0,0,0,4096,0,0,0,4096},{0,0,4096,0,4096,0,-4096,0,0},
        {-4096,0,0,0,4096,0,0,0,-4096}};
    uint64_t hash=UINT64_C(14695981039346656037);unsigned cases=0;
    for(unsigned phase=0;phase<6;++phase)for(unsigned motion=0;motion<6;++motion)
    for(unsigned pattern=0;pattern<12;++pattern)for(unsigned matrix=0;matrix<3;++matrix){
        struct {u64 before;PmSecondarySlot storage;u64 after;} guarded={.before=UINT64_C(0xa55aa55aa55aa55a),.after=UINT64_C(0x5aa55aa55aa55aa5)};
        RoomEnt *object=(void *)&guarded.storage;RoomLibHandlerDState *state=(void *)&object->sub;RoomLink link={0};
        object->link=&link;object->flag3=1;object->t1A=1;state->lockY=1;
        memcpy(state->matrix,matrices[matrix],sizeof(state->matrix));
        state->motion[0]=(s32)motions[motion];state->motion[2]=(s32)motions[(motion+1)%6];
        for(unsigned i=0;i<3;++i){state->anchor[i]=(s32)patterns[(pattern+i)%12];link.pos[i]=0x12345678;}
        state->value88=(s32)patterns[pattern];state->value8C=(s32)patterns[(pattern+3)%12];
        state->value90=(s32)patterns[(pattern+6)%12];state->speed=(s32)patterns[(pattern+9)%12];
        state->progress.word=(s32)phases[phase];
        for(unsigned i=0;i<32;++i){Psyz_GteDataWrite(i,0);Psyz_GteCtrlWrite(i,0);}
        RoomLib_HandlerD(object);
        const u32 words[]={(u32)state->motion[0],(u32)state->motion[2],(u32)state->value88,(u32)state->progress.word,
            (u32)link.pos[0],(u32)link.pos[1],(u32)link.pos[2],(u32)link.modelTranslation[0],(u32)link.modelTranslation[1],
            (u32)link.modelTranslation[2],object->state,object->flag3};
        for(unsigned word=0;word<12;++word)for(unsigned byte=0;byte<4;++byte)
            hash=(hash^((words[word]>>(byte*8))&255))*UINT64_C(1099511628211);
        if(guarded.before!=UINT64_C(0xa55aa55aa55aa55a) || guarded.after!=UINT64_C(0x5aa55aa55aa55aa5))return 0;
        ++cases;
    }
    if(cases!=1296 || hash!=UINT64_C(0xc3b4322592dc574c)){
        fprintf(stderr,"Room D update CPU/GTE hash %u %016llx\n",cases,(unsigned long long)hash);return 0;}
    puts("Original room D update: 1296 USA CPU/GTE-bridge states, negative motion, wrapped speed/progress/coordinates, release and native arena guards passed");
    return 1;
}

static int original_room_d_motion(void)
{
    const int targets[][3]={{0,0,10},{10,0,0},{0,0,-10},{-10,0,0},{7,-3,11}};
    const int anchors[][3]={{0,0,0},{1,2,3}},modes[]={0,16,-16};
    const s16 steps[][3]={{4096,0,0},{-4096,0,0},{0,0,4096},{0,0,-4096},{17,-29,31}};
    uint64_t hash=UINT64_C(14695981039346656037);unsigned cases=0;
    for(unsigned t=0;t<5;++t)for(unsigned a=0;a<2;++a)for(unsigned m=0;m<3;++m)
    for(unsigned linked=0;linked<2;++linked)for(unsigned step=0;step<5;++step){
        RoomLibHandlerDState state={0};RoomLibMotionWork work={0};RoomLink target={0};
        for(unsigned i=0;i<3;++i){state.target[i]=targets[t][i]*65536;
            state.anchor[i]=anchors[a][i]*65536;state.rotation[i]=steps[step][i];target.pos[i]=state.target[i];}
        state.target[3]=modes[m]*65536;state.targetLink=linked?&target:NULL;
        for(unsigned i=0;i<32;++i){Psyz_GteDataWrite(i,0);Psyz_GteCtrlWrite(i,0);}
        RoomLib_HandlerDTransformTarget(&state,&work);
        for(unsigned i=0;i<16;++i){u32 value=i<4?(u32)state.target[i]:i<7?(u32)work.rotated[i-4]:(u16)work.matrix[i-7];
            for(unsigned byte=0;byte<4;++byte)hash=(hash^((value>>(byte*8))&255))*UINT64_C(1099511628211);}
        ++cases;
    }
    if(hash!=UINT64_C(0x996cffcbf9215269)){
        fprintf(stderr,"Room D motion CPU/GTE-bridge hash differs: %u %016llx\n",cases,(unsigned long long)hash);return 0;}
    puts("Original room D motion: 300 USA CPU/GTE-bridge states, all directions, signed height, target actors and native state storage passed");
    return 1;
}

static int original_room_e_init(void)
{
    const int starts[][3]={{0,0,0},{1,-2,3}},targets[][3]={{10,0,20},{-10,3,-20},{20,-7,0},{0,0,10}};
    const int offsets[][3]={{0,0,0},{1,0,0},{-1,3,2}},speeds[]={32768,65536,131072};
    uint64_t hash=UINT64_C(14695981039346656037);unsigned cases=0;
    for(unsigned start=0;start<2;++start)for(unsigned target=0;target<4;++target)
    for(unsigned offset=0;offset<3;++offset)for(unsigned speed=0;speed<3;++speed)
    for(unsigned duration=0;duration<=10;duration+=10)for(unsigned eased=0;eased<2;++eased)
    for(unsigned lock=0;lock<2;++lock){
        struct {u64 before;PmSecondarySlot object;u64 after;} guarded={.before=0xa55aa55aa55aa55a,.after=0x5aa55aa55aa55aa5};
        RoomEnt *object=(void *)&guarded.object;RoomLibHandlerEState *state=(void *)&object->sub;RoomLink link={0};
        object->link=&link;object->flag3=1;
        for(unsigned i=0;i<3;++i){link.pos[i]=starts[start][i]*65536;
            state->target[i]=targets[target][i]*65536;state->localOffset[i]=offsets[offset][i]*65536;}
        link.flags=0x123cffff;state->speed=speeds[speed];state->duration=duration;state->eased=eased;
        state->lockY=lock;state->copyPosition=1;
        for(unsigned i=0;i<32;++i){Psyz_GteDataWrite(i,0);Psyz_GteCtrlWrite(i,0);}
        RoomLib_HandlerE(object);
        const u32 words[]={(u32)state->start[0],(u32)state->start[1],(u32)state->start[2],
            (u32)state->target[0],(u32)state->target[1],(u32)state->target[2],
            (u32)state->delta[0],(u32)state->delta[1],(u32)state->delta[2],
            (u32)link.pos[0],(u32)link.pos[1],(u32)link.pos[2],
            (u32)link.posMirror[0],(u32)link.posMirror[1],(u32)link.posMirror[2],(u32)link.posMirror[3],
            (u16)state->duration,(u16)state->heading,(u16)state->frame,(u16)state->phase,
            link.flags,object->state,object->flag3,object->t1A};
        for(unsigned word=0;word<24;++word)for(unsigned byte=0;byte<4;++byte)
            hash=(hash^((words[word]>>(byte*8))&255))*UINT64_C(1099511628211);
        if(guarded.before!=UINT64_C(0xa55aa55aa55aa55a) || guarded.after!=UINT64_C(0x5aa55aa55aa55aa5))return 0;
        ++cases;
    }
    if(cases!=576 || hash!=UINT64_C(0x100939fc059de009)){
        fprintf(stderr,"Room E initialization CPU/GTE hash: %u %016llx\n",cases,(unsigned long long)hash);return 0;}
    puts("Original room E: 576 USA CPU/GTE-bridge first-frame states, signed offsets, automatic duration, easing, Y lock, mirrors and native arena guards passed");
    return 1;
}

static int original_room_e_update(void)
{
    const u32 patterns[]={0,65536,0xffff0000,0x7fffffff,0x80000000,0x12345678};
    const s16 phases[]={-32768,-1,0,1,30,32767},durations[]={1,3,100,32767};
    uint64_t hash=UINT64_C(14695981039346656037);unsigned cases=0;
    for(unsigned phase=0;phase<6;++phase)for(unsigned duration=0;duration<4;++duration)
    for(unsigned eased=0;eased<2;++eased)for(unsigned lock=0;lock<2;++lock)
    for(unsigned copy=0;copy<2;++copy)for(unsigned frame=0;frame<=3;frame+=3)
    for(unsigned pattern=0;pattern<6;++pattern){
        struct {u64 before;PmSecondarySlot object;u64 after;} guarded={.before=0xa55aa55aa55aa55a,.after=0x5aa55aa55aa55aa5};
        RoomEnt *object=(void *)&guarded.object;RoomLibHandlerEState *state=(void *)&object->sub;RoomLink link={0};
        object->link=&link;object->flag3=1;object->t1A=1;
        for(unsigned i=0;i<3;++i){state->start[i]=(s32)patterns[(pattern+i)%6];state->delta[i]=(s32)patterns[(pattern+2*i)%6];
            link.pos[i]=0x12345678;link.posMirror[i]=(s32)0xabcdef12;}
        link.posMirror[3]=(s32)0xa5a5a5a5;
        state->duration=durations[duration];state->eased=eased;state->frameLimit=3;state->frame=frame;
        state->phase=phases[phase];state->phaseFrame[0]=0;state->phaseFrame[1]=2;state->phaseFrame[2]=5;state->phaseFrame[3]=10;
        state->lockY=lock;state->copyPosition=copy;
        RoomLib_HandlerE(object);
        const u32 words[]={(u32)link.pos[0],(u32)link.pos[1],(u32)link.pos[2],(u32)link.posMirror[0],
            (u32)link.posMirror[1],(u32)link.posMirror[2],(u32)link.posMirror[3],(u16)state->frame,(u16)state->phase,object->state,object->flag3};
        for(unsigned word=0;word<11;++word)for(unsigned byte=0;byte<4;++byte)
            hash=(hash^((words[word]>>(byte*8))&255))*UINT64_C(1099511628211);
        if(guarded.before!=UINT64_C(0xa55aa55aa55aa55a) || guarded.after!=UINT64_C(0x5aa55aa55aa55aa5))return 0;
        ++cases;
    }
    if(cases!=2304 || hash!=UINT64_C(0xab6562b990f5c842)){
        fprintf(stderr,"Room E update CPU hash: %u %016llx\n",cases,(unsigned long long)hash);return 0;}
    puts("Original room E: 2304 unhooked USA CPU linear/eased motion states, signed phase, Y lock, position mirrors, release and native arena guards passed");
    return 1;
}

static int original_room_arc_offsets(void)
{
    const s32 offsets[]={-32768,-7958,-4096,-1,0,1,4096,7958,32767};
    const s32 targets[]={65536,0x40000000,0x7fff0000,0x7fffffff};
    u64 hash=UINT64_C(14695981039346656037);unsigned cases=0;
    for(unsigned kind=0;kind<3;++kind)for(unsigned x=0;x<9;++x)
    for(unsigned z=0;z<9;++z)for(unsigned target=0;target<4;++target){
        struct {u64 before;PmSecondarySlot slot;u64 after;} guarded={0};
        guarded.before=UINT64_C(0xa55aa55aa55aa55a);
        guarded.after=UINT64_C(0x5aa55aa55aa55aa5);
        RoomEnt *object=(RoomEnt *)&guarded.slot;RoomLink link={0};
        object->link=&link;link.variant=1;link.pos[2]=targets[target]-65536;
        s32 *destination,*velocity;
        if(kind<2){
            RoomLibHandlerCState *state=(RoomLibHandlerCState *)&object->sub;
            state->target[2]=targets[target];state->localOffset[0]=offsets[x]*4096;
            state->localOffset[2]=offsets[z]*4096;state->rate=16;state->mode=65536;
            if(kind==0)RoomLib_HandlerC(object);else RoomLib_AdvanceArcToTargetY(object);
            destination=state->arcTarget;velocity=state->horizontalVelocity;
        }else{
            RoomLibHandlerBState *state=(RoomLibHandlerBState *)&object->sub;
            state->target[2]=targets[target];state->localOffset[0]=offsets[x]*4096;
            state->localOffset[2]=offsets[z]*4096;state->mode=65536;
            RoomLib_AdvanceArcToTarget(object);
            destination=state->target;velocity=state->horizontalVelocity;
        }
        const u32 words[]={kind,(u32)offsets[x],(u32)offsets[z],(u32)targets[target],
            (u32)destination[0],(u32)destination[1],(u32)destination[2],
            (u32)velocity[0],(u32)velocity[1]};
        for(unsigned word=0;word<9;++word)for(unsigned byte=0;byte<4;++byte)
            hash=(hash^((words[word]>>(byte*8))&255))*UINT64_C(1099511628211);
        if(guarded.before!=UINT64_C(0xa55aa55aa55aa55a) ||
           guarded.after!=UINT64_C(0x5aa55aa55aa55aa5) || link.pos[0] || link.pos[1] ||
           link.pos[2]!=targets[target]-65536)return 0;
        ++cases;
    }
    if(cases!=972 || hash!=UINT64_C(0xcf98d09938562bbb)){
        fprintf(stderr,"Room arc offset CPU hash: %u %016llx\n",cases,(unsigned long long)hash);return 0;}
    puts("Original room arcs: 972 signed GTE offset and wrapped destination/delta cases, USA CPU arithmetic blocks, three full native initializers and slot guards passed");
    return 1;
}

static int native_room_actor_class_table(void)
{
    PmSlotBanks banks={0};PmTables saved_tables=g_PmTables;
    u32 saved_flags=g_GameStateFlags;g_GameStateFlags|=0x80;
    PmCommand *saved_handler=g_PmCmdHandlerTable[70];
    FieldActor actor={0};RoomLink *view=(RoomLink *)&actor;
    view->variant=3;
    unsigned saved_count=room_callback_count;
    RoomEnt *saved_context=expected_room_context;
    g_PmTables.primary=banks.primary;g_PmTables.secondary=banks.secondary;
    void (*callbacks[])(RoomEnt *)={RoomLib_HandlerD,RoomLib_HandlerD,RoomLib_HandlerE,
        RoomLib_HandlerB,RoomLib_HandlerC,RoomLib_HandlerA};
    for(unsigned row=0;row<6;++row){
        memset(&banks,0,sizeof(banks));g_PmCmdHandlerTable[70]=&g_RoomActorClassTable[row];
        PmCommand *handler=g_PmCmdHandlerTable[70];
        handler->init();
        int slot=Scene_LoadRoomAssets(70,&actor);
        if(slot!=11){fprintf(stderr,"Room actor class row %u line %d failed\n",row,__LINE__);return 0;}
        RoomEnt *object=(RoomEnt *)&banks.secondary[0];
        if(object->link!=view || object->t16!=-1 || object->t17!=-1 || object->t18!=(row==3 || row==4?0:-1) ||
           object->t19!=(row==3 || row==4?7:3) || object->active || object->sub.signal ||
           object->sub.cb!=callbacks[row] || object->t1A){fprintf(stderr,"Room actor class row %u line %d failed\n",row,__LINE__);return 0;}
        if(row<2){
            RoomLibHandlerDState *state=(RoomLibHandlerDState *)&object->sub;
            if(state->value90!=0x400 || state->duration!=0x100 || state->progress.word ||
               Pm_SendCmd(slot,0,23,1,0,1)!=0 || object->t19!=5)return 0;
            object->t19=3;
        }
        if(row<2){
            RoomLibHandlerDState *state=(RoomLibHandlerDState *)&object->sub;
            RoomLibMotionWork work={0};
            state->target[0]=0;state->target[1]=0;state->target[2]=10*65536;state->target[3]=0;
            memset(state->anchor,0,sizeof(state->anchor));
            state->rotation[0]=4096;state->rotation[1]=state->rotation[2]=0;
            RoomLib_HandlerDTransformTarget(state,&work);
            if(state->target[0]!=4096*65536 || state->target[1] || state->target[2]!=10*65536 ||
               state->target[3] || state->anchor[0] || state->targetLink)return 0;
            state->target[0]=0;state->target[2]=10*65536;state->rotation[0]=-4096;
            RoomLib_HandlerDTransformTarget(state,&work);
            if(state->target[0]!=-4096*65536 || state->target[1] || state->target[2]!=10*65536)return 0;
        }
        if(row==4){RoomLibHandlerCState *state=(RoomLibHandlerCState *)&object->sub;
            if(state->mode!=0x10000 || state->rate!=0x10 || state->targetLink || state->secondaryLink ||
               state->localOffset[0] || state->localOffset[1] || state->localOffset[2])return 0;}
        if(row<5){
            if(Pm_SendCmd(slot,0,19,7,0,0)!=0 || object->flag3!=7){fprintf(stderr,"Room actor class row %u line %d failed\n",row,__LINE__);return 0;}
            int output=-1,progress=-1;
            if(Pm_SendCmd(slot,1,19,(intptr_t)&output,(intptr_t)&progress,0)!=0 || output!=7){fprintf(stderr,"Room actor class row %u line %d failed\n",row,__LINE__);return 0;}
        }
        expected_room_context=object;room_callback_count=0;object->sub.cb=record_room_callback;
        if(Pm_Start(slot)!=0 || Pm_Exec(slot)!=0 || room_callback_count!=1 || object->w04!=8){fprintf(stderr,"Room actor class row %u line %d failed\n",row,__LINE__);return 0;}
        if(Pm_Stop(slot,&actor,0)!=0 || object->state){fprintf(stderr,"Room actor class row %u line %d failed\n",row,__LINE__);return 0;}
        if(Pm_Exec(slot)!=0 || object->state){fprintf(stderr,"Room actor class row %u line %d failed\n",row,__LINE__);return 0;}
        handler->unload();
        for(unsigned i=1;i<11;++i){PmSecondarySlot zero={0};
            if(memcmp(&banks.secondary[i],&zero,sizeof(zero))){fprintf(stderr,"Room actor class row %u line %d failed\n",row,__LINE__);return 0;}}
    }
    g_PmCmdHandlerTable[70]=saved_handler;g_PmTables=saved_tables;g_GameStateFlags=saved_flags;
    room_callback_count=saved_count;expected_room_context=saved_context;
    puts("Original room actor classes: six typed PM rows, allocation, native defaults, scalar query, callbacks, release and slot guards passed");
    return 1;
}

static int room_callback_context(void)
{
    RoomEnt object={0};
    expected_room_context=&object;
    object.sub.cb=record_room_callback;
    int (*updates[])(PmSlotHeader *)={RoomLib_UpdateHandlerA,RoomLib_UpdateHandlerB,RoomLib_UpdateHandlerC};
    for(unsigned method=0;method<3;++method) {
        if(updates[method]((PmSlotHeader *)&object)!=0 || room_callback_count!=method+1 || object.w04!=(int)(method+1)*7)
            return 0;
    }
    puts("Original room actor classes: A/B/C update callbacks received their native object context");
    return 1;
}

/* USA Save_SelectMetadataWindow selects C0DE0 or C20A4 (+10 for armor). */

/* Exercise real sweep timing and GTE velocities with the loaded M273 constants.
 * These ranges vary a fixed source bone; they do not claim full animated-bone coverage. */
extern int RoomM273_SweepController(int,void *,intptr_t);
extern int RoomM273_SweepDropEmitter(int,void *,intptr_t);
extern int RoomM273_SweepSplashEmitter(int,void *,intptr_t);
extern intptr_t RoomM273_SweepConfigure(int,intptr_t,intptr_t,intptr_t);
static int original_m273_sweep_timing(void){
 unsigned peak=0,cases=0;int heights[]={-128,-256,-512,-768,-1024,-1536,-2048},distances[]={64,128,256,512,1024,2048,4096};
 int rates[]={16384,32768,65536,131072};
 for(unsigned side=0;side<2;++side)for(unsigned loops=0;loops<4;++loops)
 for(unsigned h=0;h<7;++h)for(unsigned d=0;d<7;++d)for(unsigned r=0;r<4;++r){
  FieldActor boss={0},player={0};FieldActorState bs={0},ps={0};RenderMatrix matrices[23]={0};
  boss.state=&bs;player.state=&ps;boss.mode=13;boss.render_object.matrices=matrices;
  for(unsigned i=0;i<3;++i)matrices[22].rotation[i][i]=4096;
  matrices[22].translation[1]=heights[h];player.pos_x=distances[d]*65536;player.render_object.model_matrix.translation[0]=distances[d];
  RoomM273BossActor owner={0};owner.instance=&boss;g_FieldFxOwner=(void *)&owner;g_PlayerEntity=&player;
  FieldFxTaskContext context={0};g_FieldFxTaskContext=&context;
  _Alignas(void *) char drops[4096],splashes[4096];
  FieldFxTaskSlot drop_slot={.start=drops,.end=drops},splash_slot={.start=splashes,.end=splashes};
  memset(&g_RoomM273SweepState,0,sizeof(g_RoomM273SweepState));g_RoomFloorY->y=0;
  RoomM273_SweepConfigure(0,loops,side,0);RoomM273_SweepController(0,NULL,0);RoomM273_SweepController(1,NULL,0);
  g_FieldFxActiveTaskSlot=&drop_slot;RoomM273_SweepDropEmitter(0,NULL,0);
  g_FieldFxActiveTaskSlot=&splash_slot;RoomM273_SweepSplashEmitter(0,NULL,0);
  for(unsigned tick=0;tick<800;++tick){
   boss.anim_prev=boss.anim.fixed;boss.anim.fixed+=rates[r];RoomM273_SweepController(1,NULL,0);
   g_FieldFxActiveTaskSlot=&drop_slot;g_FieldFxCurrentAge=tick;RoomM273_SweepDropEmitter(1,NULL,0);
   FieldFxCallbackList *list=(void *)drops;unsigned landing=0;
   for(unsigned i=0;i<(unsigned)list->count;++i){
    FieldFxCallbackListEntry *entry=(void *)(list->entries+i*list->stride);RoomM273Drop *drop=(void *)entry->state;
    if(entry->active && drop->position.pad==0 && (s16)(drop->position.y+drop->vy)>=0)++landing;
   }
   unsigned queued=g_RoomM273SweepState.count;
   if(queued+landing>peak)peak=queued+landing;
   if(queued+landing>12){printf("QUEUE OVERFLOW side=%u loops=%u height=%d distance=%d rate=%d tick=%u landings=%u\n",side,loops,heights[h],distances[d],rates[r],tick,queued+landing);return 0;}
   FieldFx_UpdateCallbackList(drops);
   if((unsigned)g_RoomM273SweepState.count!=queued+landing){printf("queue count mismatch %d/%u\n",g_RoomM273SweepState.count,landing);return 0;}
   g_FieldFxActiveTaskSlot=&splash_slot;RoomM273_SweepSplashEmitter(1,NULL,0);FieldFx_UpdateCallbackList(splashes);
   unsigned active=0;for(unsigned i=0;i<(unsigned)list->count;++i)active+=((FieldFxCallbackListEntry *)(list->entries+i*list->stride))->active!=0;
   if(g_RoomM273SweepState.done && !active)break;
  }
  ++cases;
 }
 printf("Sweep timing: %u cases, peak %u queued landings (queue capacity 12)\n",cases,peak);g_FieldFxTaskContext=NULL;g_FieldFxActiveTaskSlot=NULL;g_FieldFxOwner=NULL;g_PlayerEntity=NULL;return cases==1568 && peak==12;
}

/* Use real RNG seeding, hand-trail physics and paired-pool updates. Like
 * the sweep grid above, bone transforms are prescribed, not a full boss replay. */
extern int RoomM273_HandTrailController(int,void *,intptr_t);
extern int RoomM273_HandTrailEmitter(int,void *,intptr_t);
extern int RoomM273_HandTrailLandingEmitter(int,void *,intptr_t);
extern intptr_t RoomM273_HandTrailConfigure(int,intptr_t,intptr_t,intptr_t);
extern void *g_RoomM273HandTrailLandingFanPool;
static int original_m273_hand_timing(void){
 unsigned peak=0,cases=0;int heights[]={-128,-256,-512,-768,-1024,-1536,-2048},rates[]={16384,32768,65536,131072};
 for(unsigned mode=0;mode<3;++mode)for(unsigned h=0;h<7;++h)for(unsigned r=0;r<4;++r)for(unsigned seed=0;seed<8;++seed){
  g_GameTimers[0].current=seed*7919*60;Inv_SetDefaultItemId();
  FieldActor boss={0},player={0};FieldActorState bs={0},ps={0};RenderMatrix matrices[23]={0};
  boss.state=&bs;player.state=&ps;boss.mode=11;boss.action=40;boss.render_object.matrices=matrices;
  matrices[13].translation[0]=-200;matrices[18].translation[0]=200;
  player.pos_x=4096*65536;player.render_object.model_matrix.translation[0]=4096;
  RoomM273BossActor owner={0};owner.instance=&boss;g_FieldFxOwner=(void *)&owner;g_PlayerEntity=&player;
  FieldFxTaskContext context={0};g_FieldFxTaskContext=&context;
  _Alignas(void *) char trails[4096],pulses[4096];
  FieldFxTaskSlot trail_slot={.start=trails,.end=trails},pulse_slot={.start=pulses,.end=pulses};
  memset(&g_RoomM273HandTrailState,0,sizeof(g_RoomM273HandTrailState));g_RoomFloorY->y=0;
  RoomM273_HandTrailConfigure(0,mode,0,0);RoomM273_HandTrailController(0,NULL,0);
  g_FieldFxActiveTaskSlot=&trail_slot;RoomM273_HandTrailEmitter(0,NULL,0);
  g_FieldFxActiveTaskSlot=&pulse_slot;RoomM273_HandTrailLandingEmitter(0,NULL,0);
  for(unsigned tick=0;tick<1800;++tick){
   matrices[13].translation[1]=matrices[18].translation[1]=heights[h]+boss.pos_y/65536;
   boss.anim_prev=boss.anim.fixed;boss.anim.fixed+=rates[r];RoomM273_HandTrailController(1,NULL,0);
   g_FieldFxActiveTaskSlot=&trail_slot;g_FieldFxCurrentAge=tick;RoomM273_HandTrailEmitter(1,NULL,0);
   FieldFx_UpdateCallbackList(trails);
   if((unsigned)g_RoomM273HandTrailState.landing_count>peak)peak=g_RoomM273HandTrailState.landing_count;
   if((unsigned)g_RoomM273HandTrailState.landing_count>8){printf("QUEUE OVERFLOW mode=%u height=%d rate=%d seed=%u tick=%u pending=%d\n",mode,heights[h],rates[r],seed,tick,g_RoomM273HandTrailState.landing_count);return 0;}
   g_FieldFxActiveTaskSlot=&pulse_slot;RoomM273_HandTrailLandingEmitter(1,NULL,0);FieldFx_UpdateCallbackList(pulses);
   FieldFxCallbackList *primary=(void *)pulses,*secondary=g_RoomM273HandTrailLandingFanPool;
   for(unsigned i=0;i<(unsigned)secondary->count;++i){
    FieldFxCallbackListEntry *entry=(void *)(secondary->entries+i*secondary->stride);if(!entry->active)continue;
    void *point;memcpy(&point,entry->state,sizeof(point));unsigned found=0;
    for(unsigned j=0;j<(unsigned)primary->count;++j){FieldFxCallbackListEntry *p=(void *)(primary->entries+j*primary->stride);if(p->active && point==p->state)++found;}
    if(found!=1){printf("STALE POINTER mode=%u h=%u rate=%d tick=%u\n",mode,h,rates[r],tick);return 0;}
   }
   for(unsigned j=0;j<(unsigned)primary->count;++j){
    FieldFxCallbackListEntry *p=(void *)(primary->entries+j*primary->stride);unsigned references=0;
    for(unsigned i=0;i<(unsigned)secondary->count;++i){
     FieldFxCallbackListEntry *entry=(void *)(secondary->entries+i*secondary->stride);void *point;
     if(!entry->active)continue;memcpy(&point,entry->state,sizeof(point));references+=point==p->state;
    }
    if(references!=(unsigned)(p->active!=0)){printf("PAIRED POOL OWNERSHIP mode=%u tick=%u refs=%u\n",mode,tick,references);return 0;}
   }
   FieldFxCallbackList *list=(void *)trails;unsigned active=0;for(unsigned i=0;i<(unsigned)list->count;++i)active+=((FieldFxCallbackListEntry *)(list->entries+i*list->stride))->active!=0;
   if(g_RoomM273HandTrailState.done && !active)break;
  }
  ++cases;
 }
 printf("Hand timing: %u cases, peak %u pending landings (capacity 8), all secondary pointers refer to live primary entries\n",cases,peak);g_FieldFxTaskContext=NULL;g_FieldFxActiveTaskSlot=NULL;g_FieldFxOwner=NULL;g_PlayerEntity=NULL;return cases==672 && peak==6;
}
