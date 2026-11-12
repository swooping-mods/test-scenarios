/* Real OS R only. The native inventory, source snapshot and native pack/drain
 * readbacks own the effect evidence; no provider callback/counter is invoked. */
static int vc_observation(TSState *state,const char *label,int pass,long long actual,long long expected) {
    int accepted=ts->check(label,pass,actual,expected);
    if(!accepted)state->capsule_failure=1;
    return accepted;
}
static int vc_slot(TSState *state,PTR world) {
    int ok=1;void *args[]={dget(state->player)};PTR value=call(s_selected,world,args,&ok);
    return ok&&value&&*(int *)unbox(value)==state->test_slot;
}
static int vc_held(TSState *state,PTR world,PTR id) {
    int ok=1;void *args[]={dget(state->player)};PTR tool=call(vf.held_tool,world,args,&ok);
    PTR key=tool?call(s_tool_id,tool,0,&ok):0;
    return ok&&key&&s_same_string(key,id)&&vc_slot(state,world);
}
static double vc_container_mass(TSState *state,PTR world,PTR bag,int *ok) {
    void *args[]={dget(state->player),bag};PTR value=call(cf.container_mass,world,args,ok);
    if(!value){*ok=0;return -1;}return *(double *)unbox(value);
}
static int vc_stop(TSState *state) {
    state->capsule_stop=1;state->capsule.uncertain=1;return MF_FAIL;
}
static int vc_context(TSState *state,PTR world) {
    if(!world||world!=dget(state->world)||!s_live(world)||!state->player)return 0;
    int ok=1;PTR player=call(cf.local_player,0,0,&ok);
    return ok&&player&&s_same_string(player,dget(state->player));
}
#include "vacuum_full.h"
static int s_vacuum_collect_step(const MoonTestContext *context,void *user,const char **reason) {
    TSState *state=user;CFixture *fixture=&state->capsule;PTR world=context->world,bag=dget(state->native_container_id);
    int ok=1;
    if(state->stage<30){
        int result=s_vacuum_capsule_fixture_step(context,user,reason);
        if(result!=MF_PASS)return result;
        state->stage=30;state->action_ms=context->now_ms;
        bag=dget(state->native_container_id);
    }
    if(!vc_context(state,world))return vc_stop(state);
    if(!cf_scope(fixture,world)&&state->stage<38)return vc_stop(state);
    if(state->stage>=40)return vc_full_phase(context,state,reason);
    if(state->stage==30){
        int first=boolean(s_firstperson,context->walker,&ok),blocked=boolean(s_menu_blocked,context->walker,&ok),typing=boolean(s_typing,0,&ok);
        if(!ok)return MF_FAIL;
        if(!first||blocked||typing){*reason="waiting-for-native-vacuum-input";return context->now_ms-state->action_ms<10000?MF_PENDING:MF_BLOCKED;}
        int owned=cf_observe(fixture,world)&&vc_held(state,world,da.string(state->key)),empty_bag=cf_bag_mass(fixture,world,bag,0);
        if(!vc_observation(state,"vacuum-before-r-owned-source",owned,owned,1)||
           !vc_observation(state,"vacuum-before-r-empty-correct-bag",empty_bag,empty_bag,1))return MF_FAIL;
        if(!s_pulse_begin(state,context,'R',reason))return MF_BLOCKED;
        state->stage=31;return MF_PENDING;
    }
    if(state->stage==31){
        if(!s_pulse_settled(state,context)||context->now_ms-state->action_ms<320)return MF_PENDING;
        int held=s_pulse_release(state,context);
        vc_observation(state,"vacuum-real-r-held",held==1,held,1);
        state->stage=32;return MF_PENDING;
    }
    if(state->stage==32){
        if(!s_pulse_settled(state,context))return MF_PENDING;
        double mass=vf_double(vf.mass,dget(state->native_inventory),&ok);
        int transferred=ok&&cf_bag_mass(fixture,world,bag,fixture->kilograms);
        int removed=cf_empty(world)==1;
        vc_observation(state,"vacuum-r-transfers-exact-source-mass",transferred,vf_mass_grams(mass),vf_mass_grams(fixture->kilograms));
        vc_observation(state,"vacuum-r-removes-source-record",removed,removed,1);
        if(!transferred||!removed){
            int counts[4]={0};
            int unchanged=vc_held(state,world,da.string(state->key))&&cf_bag_mass(fixture,world,bag,0)&&
                cf_counts(world,counts)&&counts[0]==1&&counts[1]==1&&counts[2]==0&&counts[3]==0&&cf_observe(fixture,world);
            return unchanged?MF_FAIL:vc_stop(state);
        }
        CFixture any={.id=-1};int gone=cf_refresh(world)&&cf_view(&any,world,0)==0;
        if(!vc_observation(state,"vacuum-collected-view-retires",gone,gone,1))return vc_stop(state);
        if(!s_pulse_begin(state,context,'R',reason))return MF_FAIL;
        state->stage=33;return MF_PENDING;
    }
    if(state->stage==33){
        if(!s_pulse_settled(state,context))return MF_PENDING;
        int held=s_pulse_release(state,context);
        vc_observation(state,"vacuum-empty-repeat-real-r-held",held==1,held,1);
        state->stage=34;return MF_PENDING;
    }
    if(state->stage==34){
        if(!s_pulse_settled(state,context))return MF_PENDING;
        int unchanged=cf_empty(world)==1&&cf_bag_mass(fixture,world,bag,fixture->kilograms);
        if(!vc_observation(state,"vacuum-empty-repeat-no-duplicate-mass",unchanged,unchanged,1))return vc_stop(state);
        if(!vf_slot(state,bag))return vc_stop(state);
        state->stage=35;state->frame=context->frame;return MF_PENDING;
    }
    if(state->stage==35){
        if(context->frame-state->frame<5)return MF_PENDING;
        int preserved=vc_held(state,world,bag)&&cf_bag_mass(fixture,world,bag,fixture->kilograms);
        double known=vc_container_mass(state,world,bag,&ok);
        int known_mass=ok&&known>=fixture->kilograms-1e-9&&known<=fixture->kilograms+1e-9;
        vc_observation(state,"vacuum-known-bag-mass-after-container-selection",known_mass,vf_mass_grams(known),vf_mass_grams(fixture->kilograms));
        if(!vc_observation(state,"vacuum-positive-mass-survives-native-container-selection",preserved,preserved,1)||!known_mass)return vc_stop(state);
        if(!vf_slot(state,da.string(state->key)))return vc_stop(state);
        state->stage=36;state->frame=context->frame;return MF_PENDING;
    }
    if(state->stage==36){
        if(context->frame-state->frame<5)return MF_PENDING;
        int preserved=equipment->is_equipped(state->key,context->walker)&&vc_held(state,world,da.string(state->key))&&
            cf_bag_mass(fixture,world,bag,fixture->kilograms);
        double known=vc_container_mass(state,world,bag,&ok);
        int known_mass=ok&&known>=fixture->kilograms-1e-9&&known<=fixture->kilograms+1e-9;
        vc_observation(state,"vacuum-known-bag-mass-after-reequip",known_mass,vf_mass_grams(known),vf_mass_grams(fixture->kilograms));
        if(!vc_observation(state,"vacuum-positive-mass-survives-reequip",preserved,preserved,1)||!known_mass)return vc_stop(state);
        if(state->capsule_full&&!state->capsule_full_done){state->stage=40;state->frame=context->frame;return MF_PENDING;}
        if(state->capsule_checkpoint){state->stage=49;return MF_PASS;}
        state->stage=37;state->action_ms=context->now_ms;state->capsule_retry_ms=0;
    }
    if(state->stage==37){
        if(state->capsule_retry_ms&&context->now_ms-state->capsule_retry_ms<200)return MF_PENDING;
        state->capsule_retry_ms=context->now_ms;
        int drained=cf_pack_drain(fixture,world,bag);
        if(!drained&&context->now_ms-state->action_ms<2000){*reason="native-pack-refused-with-proven-unchanged-ledger";return MF_PENDING;}
        if(!vc_observation(state,"vacuum-native-pack-drain-proven",drained==1,drained,1))return vc_stop(state);
        int packed=cf_observe(fixture,world)&&cf_bag_mass(fixture,world,bag,0);
        if(!vc_observation(state,"vacuum-native-pack-owned-record-and-zero-mass",packed,packed,1))return vc_stop(state);
        int restored=cf_cleanup(fixture,world);
        if(!vc_observation(state,"vacuum-native-mass-fixture-cleanup",restored,restored,1))return vc_stop(state);
        if(!s_restore_equipment(state))return vc_stop(state);
        state->owns_slot=0;state->stage=38;state->frame=context->frame;return MF_PENDING;
    }
    if(state->stage==38){
        if(context->frame-state->frame<5)return MF_PENDING;
        void *args[]={dget(state->player),bag};PTR value=call(cf.container_mass,world,args,&ok);
        double stored=ok&&value?*(double *)unbox(value):-1;
        int empty=ok&&stored>=0&&stored<=1e-9&&cf_empty(world)==1;
        void *owner[]={dget(state->player)};PTR selected=call(s_selected,world,owner,&ok);
        int slot=ok&&selected?*(int *)unbox(selected):-999;
        if(!vc_observation(state,"vacuum-original-selected-slot-restored",ok&&slot==state->original_slot,slot,state->original_slot))return vc_stop(state);
        if(!vc_observation(state,"vacuum-restored-slot-leaves-native-bag-empty",empty,vf_mass_grams(stored),0))return vc_stop(state);
        return state->capsule_failure?MF_FAIL:MF_PASS;
    }
    return MF_FAIL;
}
