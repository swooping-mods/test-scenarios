/* Build an ephemeral late-game fixture through ordinary native purchases.
 * Progression intentionally survives this case for later cases in the batch;
 * the owning launcher restores the entire profile/settings after the process. */
static PTR s_field(PTR object,const char *name) {
    PTR value=0,field=object?get_field(object_class(object),name):0;
    if(field)field_value(object,field,&value);return value;
}
static int s_bounded_array(PTR object) {
    /* Declaring fields validate the array shape. Avoid extra runtime-array
     * type reflection inside this frame callback. */
    return object&&da.array_length(object)<=512;
}
static int s_progress_level(TSState *state,PTR method,PTR key,int tool,int *ok) {
    void *args[]={tool?key:dget(state->player),tool?dget(state->player):key};
    PTR value=call(method,dget(state->world),args,ok);if(!value){*ok=0;return 0;}return *(int *)unbox(value);
}
static int s_progress_ready(TSState *state,int report) {
    int ok=1;void *args[]={dget(state->player)};
    int trace=state->stage==1&&state->fixture_index==0;
    if(trace)s_note("fixture-ready-phase","owned-levels");
    PTR value=call(s_tree_owned,dget(state->world),args,&ok);
    int owned=value?*(int *)unbox(value):0;
    if(trace)s_note("fixture-ready-phase","total-levels");
    int total=integer(s_tree_total,dget(state->world),&ok);
    if(trace)s_note("fixture-ready-phase","gravity-boots");
    PTR boots_value=call(s_boots,dget(state->world),args,&ok);
    int boots=boots_value?*(unsigned char *)unbox(boots_value):0;if(!boots_value)ok=0;
    if(trace)s_note("fixture-ready-phase","readiness-returned");
    int half=total/2+total%2;
    int ready=ok&&value&&boots&&total>0&&owned<=total&&owned>=12&&owned>=half;
    if(ready&&report) {
        int pass=ts->check("fixture-native-gravity-boots",boots,boots,1);
        pass&=ts->check("fixture-native-upgrade-minimum",owned>=12,owned,12);
        pass&=ts->check("fixture-native-upgrade-half",owned>=half,owned,half);
        return pass;
    }
    return ready;
}
static int s_progression_step(const MoonTestContext *context,void *user,const char **reason) {
    TSState *state=user;int ok=1;
    int reports=boolean(s_sends_achievements,0,&ok);
    if(!ok)return MF_FAIL;
    unsigned isolated=isolation?isolation->status()&MF_ISOLATE_STEAM_STATS:0;
    if(reports&&!isolated){*reason="steam-achievement-reporting-enabled";return MF_BLOCKED;}
    if(state->stage==0) {
        if(!ts->check("fixture-steam-achievement-writes-blocked",reports==0||isolated,isolated,MF_ISOLATE_STEAM_STATS))return MF_FAIL;
        PTR tree=find_class("MoonCleaners","MoonUpgradeTree");
        PTR nodes=0,gear=0,nodes_field=tree?get_field(tree,"Nodes"):0,gear_field=tree?get_field(tree,"Gear"):0;
        if(!tree||!nodes_field||!gear_field||!s_static_get){*reason="native-upgrade-catalog-unavailable";return MF_BLOCKED;}
        PTR (*field_type)(PTR)=(PTR(*)(PTR))GetProcAddress(GetModuleHandleA("GameAssembly.dll"),"il2cpp_field_get_type");
        if(!field_type)return MF_FAIL;
        PTR fields[]={nodes_field,gear_field};
        for(unsigned i=0;i<2;i++){
            char *type=da.type_name(field_type(fields[i]));unsigned length=0;
            if(type)while(type[length]&&length<512)length++;
            s_note(i?"fixture-gear-field-type":"fixture-nodes-field-type",type);
            int array=length>=2&&length<512&&type[length-2]=='['&&type[length-1]==']';if(type)da.free(type);
            if(!array){*reason="unsupported-native-upgrade-field";return MF_BLOCKED;}
        }
        s_note("fixture-stage","initializing-tree-through-native-total");
        (void)integer(s_tree_total,context->world,&ok);if(!ok)return MF_FAIL;
        s_note("fixture-stage","reading-static-arrays");
        s_static_get(nodes_field,&nodes);s_static_get(gear_field,&gear);
        s_note("fixture-stage","reading-native-tool-catalog");
        PTR settings=s_field(context->world,"settings");s_note("fixture-stage","settings-read");
        PTR catalog=s_field(settings,"upgradeCatalog");s_note("fixture-stage","catalog-read");
        PTR tool_field=catalog?get_field(object_class(catalog),"tools"):0;
        char *tool_type=tool_field?da.type_name(field_type(tool_field)):0;unsigned tool_length=0;
        if(tool_type)while(tool_type[tool_length]&&tool_length<512)tool_length++;
        s_note("fixture-tool-field-type",tool_type);
        int tool_array=tool_length>=2&&tool_length<512&&tool_type[tool_length-2]=='['&&tool_type[tool_length-1]==']';
        if(tool_type)da.free(tool_type);
        if(!tool_array){*reason="unsupported-native-tool-field";return MF_BLOCKED;}
        PTR tools=s_field(catalog,"tools");s_note("fixture-stage","tool-array-read");
        if(!s_bounded_array(nodes)||!s_bounded_array(gear)||!s_bounded_array(tools)){
            *reason="unsupported-native-upgrade-array";return MF_BLOCKED;
        }
        s_note("fixture-stage","arrays-bounded");
        state->nodes=dhold(nodes);state->gear=dhold(gear);state->tools=dhold(tools);
        s_note("fixture-stage","arrays-rooted");
        if(!state->nodes||!state->gear||!state->tools)return MF_FAIL;
        state->fixture_index=state->fixture_round=state->fixture_progress=0;
        state->owns_wallet=1;state->stage=1;return MF_PENDING;
    }
    if(s_progress_ready(state,0))return s_progress_ready(state,1)?MF_PASS:MF_FAIL;
    /* One transaction per frame bounds scene work and keeps native callbacks,
     * notifications, persistence and prerequisite validation in the path. */
    if(state->stage==1) {
        PTR tools=dget(state->tools);
        if(state->fixture_index>=da.array_length(tools)){state->fixture_index=0;state->stage=2;return MF_PENDING;}
        PTR offer=*(PTR *)darray(tools,state->fixture_index++),definition=s_field(offer,"tool");
        PTR id=definition?call(s_tool_id,definition,0,&ok):0;
        if(!id||!ok)return MF_FAIL;
        int level=s_progress_level(state,s_tool_level,id,1,&ok);
        if(!ok)return MF_FAIL;if(level>=0)return MF_PENDING;
        s_note_string("fixture-native-tool-candidate",id);s_note("fixture-stage","setting-funding");
        if(!s_set(state,1000000000))return MF_FAIL;
        s_note("fixture-stage","buying-native-tool");
        PTR message=0;void *args[]={dget(state->player),id,&level,&message};
        PTR bought=call(s_tool_buy,context->world,args,&ok);
        s_note("fixture-stage","native-tool-returned");
        if(!ok)return MF_FAIL;
        if(!bought||!*(unsigned char *)unbox(bought)){
            s_note_string("fixture-native-tool-refused",message);return MF_PENDING;
        }
        int after=s_progress_level(state,s_tool_level,id,1,&ok);
        if(!ts->check("fixture-native-tool-owned",ok&&after>=0,after,0))return MF_FAIL;
        s_note_string("fixture-native-tool-bought",id);return MF_PENDING;
    }
    PTR nodes=dget(state->nodes),gear=dget(state->gear);
    U64 node_count=da.array_length(nodes),total=node_count+da.array_length(gear);
    if(!total){*reason="native-upgrade-tree-empty";return MF_BLOCKED;}
    if(state->fixture_index>=total) {
        state->fixture_index=0;
        if(!state->fixture_progress||++state->fixture_round>=64){*reason="native-progression-prerequisites-unmet";return MF_BLOCKED;}
        state->fixture_progress=0;return MF_PENDING;
    }
    unsigned index=state->fixture_index++;
    PTR node=*(PTR *)darray(index<node_count?nodes:gear,index<node_count?index:index-node_count);
    PTR id=s_field(node,"id");if(!node||!id)return MF_FAIL;
    int level=s_progress_level(state,s_tree_level,id,0,&ok);
    void *max_args[]={node};PTR maximum=call(s_tree_max,context->world,max_args,&ok);
    if(!ok||!maximum)return MF_FAIL;
    if(level>=*(int *)unbox(maximum))return MF_PENDING;
    void *requirements[]={dget(state->player),node};
    PTR met=call(s_tree_requirements,context->world,requirements,&ok),missing=call(s_tree_missing,context->world,requirements,&ok);
    if(!ok||!met||!missing)return MF_FAIL;
    if(!*(unsigned char *)unbox(met)||*(unsigned char *)unbox(missing))return MF_PENDING;
    if(!s_set(state,1000000000))return MF_FAIL;
    PTR message=0;void *args[]={dget(state->player),id,&level,&message};
    PTR bought=call(s_tree_buy,context->world,args,&ok);if(!ok)return MF_FAIL;
    if(!bought||!*(unsigned char *)unbox(bought)){s_note_string("fixture-native-tree-refused",message);return MF_PENDING;}
    int after=s_progress_level(state,s_tree_level,id,0,&ok);
    if(!ts->check("fixture-native-level-advanced",ok&&after==level+1,after,level+1))return MF_FAIL;
    state->fixture_progress++;s_note_string("fixture-native-level-bought",id);return MF_PENDING;
}
