/* Read every predicate independently. An earlier mismatch must not hide the
 * later observations needed to separate persistence from binding/fixture gaps. */
static int sr_loadout_backing(PTR world,PTR actor) {
    int ok=1;PTR dictionary=cf_object(world,"upgrades",&ok);
    if(!ok||!dictionary){s_note("reopen-loadout-backing","unavailable-dictionary");return 0;}
    PTR method=get_method(object_class(dictionary),"TryGetValue",2),record=0;
    void *args[]={actor,&record};PTR found=method?call(method,dictionary,args,&ok):0;
    if(!ok||!found){s_note("reopen-loadout-backing","unknown-lookup");return 0;}
    if(!*(unsigned char *)unbox(found)||!record){s_note("reopen-loadout-backing","absent-player");return 0;}
    DRoot root=dhold(record);
    if(!root){s_note("reopen-loadout-backing","unrooted-player");return 0;}
    PTR player=cf_object(dget(root),"player",&ok),slots=cf_object(dget(root),"slots",&ok);int selected=-999;
    int fields=cf_read(dget(root),"selectedSlot",&selected);
    int ready=ok&&fields&&player&&s_same_string(player,actor)&&slots&&da.array_length(slots)==6&&selected>=0&&selected<6;
    s_note("reopen-loadout-backing",ready?"existing-player-and-six-slots":"unsafe-player-or-slots");
    ddrop(&root);return ready;
}
static int sr_read_string(TSState *state,const char *label,PTR value,PTR expected,int ok) {
    s_note_string(label,value);
    return vc_observation(state,label,ok&&value&&expected&&s_same_string(value,expected),ok?(value!=0):-1,1);
}
static int sr_read_mass(TSState *state,const char *label,const char *status,double value,double expected,int ok) {
    int valid=ok&&value>=0&&value<=1e9;
    s_note(status,!ok?"unknown":valid?"valid":"invalid");
    return vc_observation(state,label,valid&&value>=expected-1e-9&&value<=expected+1e-9,
        ok?vf_mass_grams(value):-1,vf_mass_grams(expected));
}
static int sr_reopen_observe(const MoonTestContext *context,TSState *state,const char *expected_owner,int component_ok) {
    CFixture *f=&state->capsule;PTR world=context->world,actor=dget(state->player),bag=dget(state->native_container_id);
    DRoot key=dhold(da.string(state->key));int pass=1,ok=1;char owner[128];
    int backing=sr_loadout_backing(world,actor);
    int owner_matches=sr_identity(actor,owner)&&deq(owner,expected_owner);
    pass&=vc_observation(state,"vacuum-reopen-read-source-actor",owner_matches,owner_matches,1);
    PTR fresh=call(cf.local_player,0,0,&ok);
    pass&=sr_read_string(state,"vacuum-reopen-read-current-actor",fresh,actor,ok);
    pass&=vc_observation(state,"vacuum-reopen-read-context",context->flags==7&&world==dget(state->world)&&s_live(world),context->flags,7);
    int offered=s_offer(state->key)!=0;
    pass&=vc_observation(state,"vacuum-reopen-read-provider",offered,offered,1);
    unsigned owned=backing?shop->state(state->key,world,context->walker):0;
    pass&=vc_observation(state,"vacuum-reopen-read-vacuum-owned",backing&&(owned&MS_OWNED)!=0,backing?(long long)owned:-1,MS_OWNED);
    ok=1;int level=s_progress_level(state,s_tool_level,bag,1,&ok);
    pass&=vc_observation(state,"vacuum-reopen-read-native-bag-owned",ok&&level>=0&&level<1000000,ok?level:-999,0);
    void *owner_args[]={actor};ok=backing;PTR selected=ok?call(s_selected,world,owner_args,&ok):0;
    int slot=ok&&selected?*(int *)unbox(selected):-999;
    pass&=vc_observation(state,"vacuum-reopen-read-selected-slot",ok&&selected&&slot==state->test_slot,slot,state->test_slot);
    void *slot_args[]={actor,&state->test_slot};ok=backing;PTR assigned=ok?call(s_loadout,world,slot_args,&ok):0;
    pass&=sr_read_string(state,"vacuum-reopen-read-loadout-id",assigned,dget(key),ok);
    ok=backing;PTR tool=ok?call(vf.held_tool,world,owner_args,&ok):0;
    PTR held=tool&&ok?call(s_tool_id,tool,0,&ok):0;
    pass&=sr_read_string(state,"vacuum-reopen-read-held-tool-id",held,dget(key),ok);
    int equipped=backing?equipment->is_equipped(state->key,context->walker):-1;
    pass&=vc_observation(state,"vacuum-reopen-read-equipped",equipped==1,equipped,1);
    ok=backing;PTR loaded=ok?call(cf.loaded,world,owner_args,&ok):0;
    pass&=sr_read_string(state,"vacuum-reopen-read-loaded-bag-id",loaded,bag,ok);
    ok=backing;PTR held_bag=ok?call(vf.held_container,world,owner_args,&ok):0;
    pass&=sr_read_string(state,"vacuum-reopen-read-held-bag-id",held_bag,bag,ok);
    PTR inventory=dget(state->native_inventory);ok=backing&&component_ok&&inventory;
    int has=ok?boolean(vf.has_container,inventory,&ok):0;
    pass&=vc_observation(state,"vacuum-reopen-read-has-container",ok&&has,ok?has:-1,1);
    ok=backing&&component_ok&&inventory;double kilograms=ok?vf_double(vf.mass,inventory,&ok):-1;
    pass&=sr_read_mass(state,"vacuum-reopen-read-inventory-grams","reopen-inventory-status",kilograms,.125,ok);
    ok=backing;kilograms=ok?cf_mass(world,actor,&ok):-1;
    pass&=sr_read_mass(state,"vacuum-reopen-read-world-grams","reopen-world-mass-status",kilograms,.125,ok);
    ok=backing;kilograms=ok?vc_container_mass(state,world,bag,&ok):-1;
    pass&=sr_read_mass(state,"vacuum-reopen-read-known-bag-grams","reopen-known-bag-status",kilograms,.125,ok);
    ok=backing&&component_ok&&inventory;double cap=ok?vf_double(vf.capacity,inventory,&ok):-1;
    pass&=sr_read_mass(state,"vacuum-reopen-read-capacity-grams","reopen-capacity-status",cap,state->native_capacity,ok);
    PTR list=0;int next=0;ok=cf_list(dget(f->original),&list,&next);
    int count=cf_count(list,&ok);
    pass&=vc_observation(state,"vacuum-reopen-read-snapshot-empty",ok&&count==0,ok?count:-1,0);
    /* Read next independently of a failed list/count probe. */
    ok=cf_read(dget(f->original),"nextCanisterId",&next);
    pass&=vc_observation(state,"vacuum-reopen-read-next-id",ok&&next==state->reload_next,ok?next:-1,state->reload_next);
    static const char *fields[]={"canisters","canisterSectors","carriedCanisters","flyingCanisters"};
    static const char *labels[]={"vacuum-reopen-read-canister-count","vacuum-reopen-read-sector-count","vacuum-reopen-read-carried-count","vacuum-reopen-read-flying-count"};
    for(unsigned i=0;i<4;i++){
        ok=1;PTR collection=cf_object(world,fields[i],&ok);count=cf_count(collection,&ok);
        pass&=vc_observation(state,labels[i],ok&&count==0,ok?count:-1,0);
    }
    CFixture any={.id=-1};int views=cf_view(&any,world,0);
    pass&=vc_observation(state,"vacuum-reopen-read-active-views",views==0,views,0);
    ddrop(&key);return pass;
}
