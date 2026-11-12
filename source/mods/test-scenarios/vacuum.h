/* Prove a real owned vanilla container works before comparing the equipped
 * Vacuum. A missing container cannot be mistaken for a collection success. */
static struct {PTR capacity,has_container,mass,held_container,held_tool;int ready;} vf;
static int vf_bind(void) {
    vf.capacity=dmethod("MoonCleaners","MoonDustInventory","get_CapacityKg","");
    vf.has_container=dmethod("MoonCleaners","MoonDustInventory","get_HasContainer","");
    vf.mass=dmethod("MoonCleaners","MoonDustInventory","get_Kilograms","");
    vf.held_container=dmethod("MoonCleaners","MoonWorld","HeldContainerId","System.String");
    vf.held_tool=dmethod("MoonCleaners","MoonWorld","HeldTool","System.String");
    vf.ready=vf.capacity&&vf.has_container&&vf.mass&&vf.held_container&&vf.held_tool;
    return vf.ready;
}
static double vf_double(PTR method,PTR self,int *ok) {
    PTR result=call(method,self,0,ok);double value=0;
    if(!result){*ok=0;return 0;}dcopy(&value,unbox(result),sizeof(value));return value;
}
static long long vf_mass_grams(double value) {
    return value>=0&&value<=1.0e9?(long long)(value*1000):-1;
}
static int vf_slot(TSState *state,PTR id) {
    if(!s_assignment(state,id,state->test_slot))return 0;
    int ok=1;PTR belt=s_get_component(dget(state->walker),"MoonToolbelt",&ok);
    void *args[]={&state->test_slot};if(belt)call(s_select,belt,args,&ok);else ok=0;
    return ok;
}
static int s_vacuum_container_step(const MoonTestContext *context,void *user,const char **reason) {
    TSState *state=user;int ok=1;
    if(state->stage==0) {
        if(!vf.ready&&!vf_bind()){*reason="native-container-api-unavailable";return MF_BLOCKED;}
        if(!isolation||!(isolation->status()&MF_ISOLATE_STEAM_STATS)){*reason="private-stats-isolation-required";return MF_BLOCKED;}
        if(!s_offer(state->key)||!(shop->state(state->key,context->world,context->walker)&MS_OWNED)){
            *reason="requires-owned-vacuum-run-purchase-first";return MF_BLOCKED;
        }
        PTR settings=s_field(context->world,"settings"),catalog=s_field(settings,"upgradeCatalog");
        PTR tools=s_field(catalog,"tools");
        if(!s_bounded_array(tools)){*reason="native-tool-catalog-unavailable";return MF_BLOCKED;}
        state->tools=dhold(tools);if(!state->tools)return MF_FAIL;
        state->fixture_index=0;state->stage=1;return MF_PENDING;
    }
    if(state->stage==1) {
        PTR tools=dget(state->tools);
        if(state->fixture_index>=da.array_length(tools)){*reason="no-purchasable-native-dust-container";return MF_BLOCKED;}
        PTR offer=*(PTR *)darray(tools,state->fixture_index++),definition=s_field(offer,"tool");
        PTR field=definition?get_field(object_class(definition),"containerKg"):0;float capacity=0;
        if(field)field_value(definition,field,&capacity);
        if(!(capacity>0&&capacity<1.0e9f))return MF_PENDING;
        PTR id=call(s_tool_id,definition,0,&ok);if(!ok||!id)return MF_FAIL;
        int level=s_progress_level(state,s_tool_level,id,1,&ok);if(!ok)return MF_FAIL;
        if(level<0) {
            state->owns_wallet=1;if(!s_set(state,1000000000))return MF_FAIL;
            PTR message=0;void *args[]={dget(state->player),id,&level,&message};
            PTR bought=call(s_tool_buy,context->world,args,&ok);if(!ok)return MF_FAIL;
            if(!bought||!*(unsigned char *)unbox(bought)){s_note_string("vacuum-native-container-refused",message);return MF_PENDING;}
            level=s_progress_level(state,s_tool_level,id,1,&ok);
        }
        if(!ts->check("vacuum-fixture-native-container-owned",ok&&level>=0,level,0))return MF_FAIL;
        state->native_container_id=dhold(id);
        if(!state->native_container_id||!s_begin_equipment(state,context,reason))return MF_FAIL;
        if(!vf_slot(state,dget(state->native_container_id)))return MF_FAIL;
        state->native_inventory=dhold(s_get_component(context->walker,"MoonDustInventory",&ok));
        if(!ok||!state->native_inventory)return MF_FAIL;
        state->stage=2;state->frame=context->frame;s_note_string("vacuum-native-container-id",id);return MF_PENDING;
    }
    if(context->frame-state->frame<5)return MF_PENDING;
    if(state->stage==2) {
        int has=boolean(vf.has_container,dget(state->native_inventory),&ok);
        double capacity=vf_double(vf.capacity,dget(state->native_inventory),&ok);
        double mass=vf_double(vf.mass,dget(state->native_inventory),&ok);
        int pass=ts->check("vacuum-native-container-baseline-present",ok&&has,has,1);
        pass&=ts->check("vacuum-native-container-baseline-capacity",ok&&capacity>0&&capacity<1e9,vf_mass_grams(capacity),1);
        pass&=ts->check("vacuum-native-container-baseline-mass-valid",ok&&mass>=0&&mass<=capacity,vf_mass_grams(mass),vf_mass_grams(capacity));
        void *args[]={dget(state->player)};PTR selected=call(vf.held_container,context->world,args,&ok);
        int selected_matches=ok&&s_same_string(selected,dget(state->native_container_id));
        pass&=ts->check("vacuum-baseline-uses-chosen-native-container",selected_matches,selected_matches,1);
        if(!pass)return MF_FAIL;state->native_capacity=capacity;
        if(!vf_slot(state,da.string(state->key)))return MF_FAIL;
        state->stage=3;state->frame=context->frame;return MF_PENDING;
    }
    int equipped=equipment->is_equipped(state->key,context->walker);
    int has=boolean(vf.has_container,dget(state->native_inventory),&ok);
    double capacity=vf_double(vf.capacity,dget(state->native_inventory),&ok);
    void *args[]={dget(state->player)};PTR held=call(vf.held_container,context->world,args,&ok);
    s_note_string("vacuum-native-held-container-id",held);
    int linked=ok&&s_same_string(held,dget(state->native_container_id));
    PTR actual_tool=call(vf.held_tool,context->world,args,&ok);
    PTR actual_id=actual_tool?call(s_tool_id,actual_tool,0,&ok):0;
    int actual_vacuum=ok&&s_same_string(actual_id,da.string(state->key));
    PTR field=actual_tool?get_field(object_class(actual_tool),"containerKg"):0;float native_kg=-1;
    if(field)field_value(actual_tool,field,&native_kg);
    int pass=ts->check("vacuum-alias-keeps-genuine-container-id",linked,linked,1);
    pass&=ts->check("vacuum-actual-held-tool-remains-vacuum",actual_vacuum,actual_vacuum,1);
    pass&=ts->check("vacuum-definition-container-metadata-remains-zero",field&&native_kg==0,vf_mass_grams(native_kg),0);
    pass&=ts->check("vacuum-native-equipped-for-collection",equipped==1,equipped,1);
    pass&=ts->check("vacuum-equipped-has-native-container",ok&&has,has,1);
    pass&=ts->check("vacuum-retains-native-container-capacity",ok&&capacity>=state->native_capacity-.001&&capacity<=state->native_capacity+.001,vf_mass_grams(capacity),vf_mass_grams(state->native_capacity));
    return pass?MF_PASS:MF_FAIL;
}
