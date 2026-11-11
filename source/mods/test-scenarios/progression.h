/* Build an ephemeral late-game fixture through ordinary native purchases.
 * Progression intentionally survives this case for later cases in the batch;
 * the owning launcher restores the entire profile/settings after the process. */
static PTR s_field(PTR object,const char *name) {
    PTR value=0,field=object?get_field(object_class(object),name):0;
    if(field)field_value(object,field,&value);return value;
}
static int s_bounded_array(PTR object) {
    if(!object)return 0;
    char *name=da.type_name(da.class_type(object_class(object)));unsigned n=0;
    if(name)while(name[n]&&n<512)n++;
    int array=n>=2&&n<512&&name[n-2]=='['&&name[n-1]==']';if(name)da.free(name);
    return array&&da.array_length(object)<=512;
}
static int s_progress_level(TSState *state,PTR method,PTR key,int tool,int *ok) {
    void *args[]={tool?key:dget(state->player),tool?dget(state->player):key};
    PTR value=call(method,dget(state->world),args,ok);if(!value){*ok=0;return 0;}return *(int *)unbox(value);
}
static int s_progress_ready(TSState *state,int report) {
    int ok=1;void *args[]={dget(state->player)};
    PTR value=call(s_tree_owned,dget(state->world),args,&ok);
    int owned=value?*(int *)unbox(value):0,total=integer(s_tree_total,dget(state->world),&ok);
    int boots=boolean(s_boots,dget(state->world),&ok);
    int ready=ok&&value&&boots&&total>0&&owned>=12&&owned*2>=total;
    if(ready&&report) {
        int pass=ts->check("fixture-native-gravity-boots",boots,boots,1);
        pass&=ts->check("fixture-native-upgrade-minimum",owned>=12,owned,12);
        pass&=ts->check("fixture-native-upgrade-half",owned*2>=total,owned*2,total);
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
        if(!tree||!nodes_field||!gear_field||!s_class_init||!s_static_get){*reason="native-upgrade-catalog-unavailable";return MF_BLOCKED;}
        s_class_init(tree);s_static_get(nodes_field,&nodes);s_static_get(gear_field,&gear);
        PTR catalog=s_field(s_field(context->world,"settings"),"upgradeCatalog"),tools=s_field(catalog,"tools");
        if(!s_bounded_array(nodes)||!s_bounded_array(gear)||!s_bounded_array(tools)){
            *reason="unsupported-native-upgrade-array";return MF_BLOCKED;
        }
        state->nodes=dhold(nodes);state->gear=dhold(gear);state->tools=dhold(tools);
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
        if(!s_set(state,1000000000))return MF_FAIL;
        PTR message=0;void *args[]={dget(state->player),id,&level,&message};
        PTR bought=call(s_tool_buy,context->world,args,&ok);
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
