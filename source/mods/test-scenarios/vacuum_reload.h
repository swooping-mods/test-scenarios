/* Explicit two-process cases. Retention is a proved fixture outcome; generic
 * zero-only cleanup and ordinary scenario batches are never relaxed. */
static int sr_identity(PTR value,char text[128]) {
    int n=value?s_string_length(value):0;
    const unsigned short *chars=value?s_string_chars(value):0;
    if(!chars||n<1||n>127)return 0;
    for(int i=0;i<n;i++){
        unsigned c=chars[i];
        if(!((c>='0'&&c<='9')||(c>='a'&&c<='z')||(c>='A'&&c<='Z')||c=='-'||c=='_'||c=='.'||c==':'||c=='@'))return 0;
        text[i]=(char)c;
    }
    text[n]=0;return 1;
}
static int sr_positive(TSState *state,PTR world) {
    CFixture *f=&state->capsule;CFixture any={.id=-1};int next=0,ok=1;
    double capacity=vf_double(vf.capacity,dget(state->native_inventory),&ok);
    return !state->capsule_failure&&!state->capsule_stop&&!f->uncertain&&f->mutated&&
        vc_context(state,world)&&cf_scope(f,world)&&cf_original_valid(f)&&
        equipment->is_equipped(state->key,dget(state->walker))&&vc_held(state,world,da.string(state->key))&&
        cf_bag_mass(f,world,dget(state->native_container_id),.125)&&cf_empty(world)==1&&
        cf_view(&any,world,0)==0&&cf_next(world,&next)&&next==f->id+1&&ok&&
        capacity>=state->native_capacity-1e-9&&capacity<=state->native_capacity+1e-9;
}
static int s_vacuum_checkpoint_step(const MoonTestContext *context,void *user,const char **reason) {
    TSState *state=user;
    if(state->stage<49){
        int result=s_vacuum_collect_step(context,user,reason);
        if(result!=MF_PASS)return result;
    }
    if(!sr_positive(state,context->world))return vc_stop(state);
    if(state->stage==49){
        if(!st.ready&&!st_bind())return vc_stop(state);
        /* A native-container fixture may have borrowed test funds. Restore
         * only that wallet, before saving, while all positive-mass proofs hold. */
        if(state->owns_wallet){
            int ok=1;
            if(!s_set(state,state->original_money)||s_money(state,&ok)!=state->original_money||!ok)return vc_stop(state);
            state->owns_wallet=0;
            if(!sr_positive(state,context->world))return vc_stop(state);
        }
        if(!st_start(&state->save,context->world))return vc_stop(state);
        state->stage=50;state->action_ms=context->now_ms;return MF_PENDING;
    }
    if(state->stage==50){
        int saved=st_poll(&state->save);
        if(saved<0)return vc_stop(state);
        if(!saved){
            if(context->now_ms-state->action_ms>=10000)return vc_stop(state);
            return MF_PENDING;
        }
        if(!vc_observation(state,"vacuum-checkpoint-save-task-completed",saved==1,saved,1)||!sr_positive(state,context->world))return vc_stop(state);
        char owner[128],bag[128],value[32];
        if(!sr_identity(dget(state->player),owner)||!sr_identity(dget(state->native_container_id),bag))return vc_stop(state);
        s_note("reload-owner",owner);s_note("reload-bag",bag);
        num(value,(U64)state->test_slot);s_note("reload-slot",value);
        num(value,(U64)(state->capsule.id+1));s_note("reload-next",value);
        num(value,(U64)vf_mass_grams(state->native_capacity));s_note("reload-capacity-grams",value);
        if(!vc_observation(state,"vacuum-checkpoint-native-retention-ready",1,125,125))return vc_stop(state);
        state->stage=51;return MF_PASS;
    }
    return vc_stop(state);
}
static int s_vacuum_checkpoint_cleanup(const MoonTestContext *context,void *user) {
    TSState *state=user;s_key_up(state);
    int pass=context&&state->stage==51&&!state->owns_wallet&&!state->capsule_stop&&
        !state->capsule.uncertain&&st_poll(&state->save)==1&&sr_positive(state,context->world);
    pass=vc_observation(state,"vacuum-checkpoint-retain-cleanup",pass,pass,1)&&pass;
    if(!pass)vc_stop(state);
    cf_drop(&state->capsule);s_drop_borrowed(state);
    return pass;
}
static int sr_env(const char *name,char value[128]) {
    DWORD n=GetEnvironmentVariableA(name,value,128);return n>0&&n<128;
}
static int sr_receipt(const char *value) {
    unsigned i=0;
    for(;value[i];i++)if(i>=64||!((value[i]>='0'&&value[i]<='9')||(value[i]>='a'&&value[i]<='f')))return 0;
    return i==64;
}
static int sr_number(const char *name,int minimum,int maximum,int *out) {
    char value[128];if(!sr_env(name,value))return 0;
    int n=0;
    for(unsigned i=0;value[i];i++){
        if(value[i]<'0'||value[i]>'9'||n>maximum/10)return 0;
        n=n*10+value[i]-'0';if(n>maximum)return 0;
    }
    if(n<minimum)return 0;
    *out=n;return 1;
}
static int sr_pack_point(const MoonTestContext *context,TSState *state) {
    if(!sc.ready)return 0;
    PTR camera=0,field=get_field(object_class(context->walker),"viewCamera");
    if(field)field_value(context->walker,field,&camera);
    if(!s_live(camera))return 0;
    int ok=1;PTR transform=call(sc.component_transform,camera,0,&ok);
    DVec origin=dvector(sc.position,transform,&ok),forward=dvector(sc.forward,transform,&ok);
    PTR world_transform=call(sc.component_transform,context->world,0,&ok);
    DVec world_origin=dvector(sc.position,world_transform,&ok);
    float direction=(forward.x*forward.x+forward.y*forward.y)+forward.z*forward.z;
    DVec point={origin.x+forward.x*2.5f-world_origin.x,origin.y+forward.y*2.5f-world_origin.y,origin.z+forward.z*2.5f-world_origin.z};
    float squared=(point.x*point.x+point.y*point.y)+point.z*point.z;
    if(!ok||!transform||!world_transform||!(direction>=.9f&&direction<=1.1f)||
       !(point.x>=-1e6f&&point.x<=1e6f&&point.y>=-1e6f&&point.y<=1e6f&&point.z>=-1e6f&&point.z<=1e6f)||
       !(squared>=1&&squared<=1e12f))return 0;
    state->capsule.position=point;return 1;
}
#include "vacuum_reopen_observe.h"
static int s_vacuum_reopen_step(const MoonTestContext *context,void *user,const char **reason) {
    TSState *state=user;(void)reason;CFixture *f=&state->capsule;
    if(state->stage==0){
        if(context->frame-state->frame<5)return MF_PENDING;
        char owner[128],bag[128],source[128],actual[128];int capacity=0;
        if(!sr_env("MOONLAB_FAST_RELOAD_RECEIPT",source)||!sr_receipt(source)||!sr_env("MOONLAB_FAST_RELOAD_OWNER",owner)||
           !sr_env("MOONLAB_FAST_RELOAD_BAG",bag)||!sr_identity(dget(state->player),actual)||
           !sr_number("MOONLAB_FAST_RELOAD_SLOT",0,5,&state->test_slot)||
           !sr_number("MOONLAB_FAST_RELOAD_NEXT",1,999999998,&state->reload_next)||
           !sr_number("MOONLAB_FAST_RELOAD_CAPACITY_GRAMS",125,1000,&capacity))return vc_stop(state);
        if(!vf.ready&&!vf_bind())return vc_stop(state);
        if(!cf.ready)return vc_stop(state);
        state->native_container_id=dhold(da.string(bag));
        if(!state->native_container_id||!sr_identity(dget(state->native_container_id),actual)||!deq(actual,bag))return vc_stop(state);
        int ok=1;state->native_inventory=dhold(s_get_component(context->walker,"MoonDustInventory",&ok));
        f->world=dhold(context->world);f->actor=dhold(dget(state->player));
        int component_ok=ok&&state->native_inventory;
        if(!f->world||!f->actor)return vc_stop(state);
        state->native_capacity=(double)capacity/1000.;f->kilograms=.125;
        f->original=cf_snapshot(context->world);f->original_next=state->reload_next;f->id=state->reload_next-1;
        int before=sr_reopen_observe(context,state,owner,component_ok);
        if(!vc_observation(state,"vacuum-reopen-before-any-write-owned-tool-bag-mass-slot-next",before,before,1))return vc_stop(state);
        if(!vc_observation(state,"vacuum-reopen-native-camera-world-pack-point",sr_pack_point(context,state),1,1))return vc_stop(state);
        /* Mark ownership of the forthcoming native packed record only after
         * the saved state is independently proved. No source restore occurs. */
        f->mutated=1;state->stage=1;state->action_ms=context->now_ms;return MF_PENDING;
    }
    if(state->stage==1){
        if(!sr_positive(state,context->world))return vc_stop(state);
        if(state->capsule_retry_ms&&context->now_ms-state->capsule_retry_ms<200)return MF_PENDING;
        state->capsule_retry_ms=context->now_ms;
        int drained=cf_pack_drain(f,context->world,dget(state->native_container_id));
        if(!drained&&context->now_ms-state->action_ms<2000)return MF_PENDING;
        if(!vc_observation(state,"vacuum-reopen-native-pack-accounts-saved125g",drained==1,drained,1))return vc_stop(state);
        int cleaned=cf_cleanup(f,context->world);
        if(!vc_observation(state,"vacuum-reopen-zero-only-owned-cleanup",cleaned,cleaned,1))return vc_stop(state);
        state->stage=2;return MF_PASS;
    }
    return vc_stop(state);
}
static int s_vacuum_reopen_cleanup(const MoonTestContext *context,void *user) {
    TSState *state=user;s_key_up(state);int pass=0;
    if(context&&state->stage==2&&!state->capsule_stop&&!state->capsule.uncertain){
        CFixture scope={.world=state->world,.actor=state->player},any={.id=-1};int next=0;
        pass=cf_bag_mass(&scope,context->world,dget(state->native_container_id),0)&&
            vc_context(state,context->world)&&cf_empty(context->world)==1&&cf_view(&any,context->world,0)==0&&
            cf_next(context->world,&next)&&next==state->reload_next&&vc_held(state,context->world,da.string(state->key));
    }
    pass=vc_observation(state,"vacuum-reopen-final-zero-mass-no-equipment-repair",pass,pass,1)&&pass;
    if(!pass)vc_stop(state);
    cf_drop(&state->capsule);s_drop_borrowed(state);return pass;
}
