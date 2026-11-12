static int vc_native_full(TSState *state,PTR world,int *ok) {
    void *args[]={dget(state->player)};PTR value=call(cf.full,world,args,ok);
    if(!value){*ok=0;return 0;}return *(unsigned char *)unbox(value)!=0;
}
static int vc_full_phase(const MoonTestContext *context,TSState *state,const char **reason) {
    CFixture *fixture=&state->capsule;PTR world=context->world,bag=dget(state->native_container_id);
    int ok=1;double carried=state->native_capacity;
    if(state->stage==40){
        double capacity=vf_double(vf.capacity,dget(state->native_inventory),&ok);
        int full=vc_native_full(state,world,&ok);
        int saturated=ok&&full&&capacity>=carried-1e-9&&capacity<=carried+1e-9&&
            vc_held(state,world,da.string(state->key))&&cf_bag_mass(fixture,world,bag,carried);
        if(!vc_observation(state,"vacuum-native-capacity-saturated",saturated,vf_mass_grams(capacity),vf_mass_grams(carried)))return vc_stop(state);
        int seeded=cf_seed_retained(fixture,world,bag,carried,.125);
        if(!vc_observation(state,"vacuum-full-owned-second-source-created",seeded,seeded,1))return vc_stop(state);
        state->stage=41;state->frame=context->frame;return MF_PENDING;
    }
    if(state->stage==41){
        int views=cf_view(fixture,world,1);
        if(views<0)return vc_stop(state);
        if(!views&&context->frame-state->frame<180)return MF_PENDING;
        PTR transform=views==1?call(sc.component_transform,dget(fixture->view),0,&ok):0;
        DVec position=transform?dvector(sc.position,transform,&ok):(DVec){0},expected=state->scene_center;
        int pose=ok&&transform&&position.x>=expected.x-.05f&&position.x<=expected.x+.05f&&
            position.y>=expected.y-.05f&&position.y<=expected.y+.05f&&position.z>=expected.z-.05f&&position.z<=expected.z+.05f;
        int ready=views==1&&pose&&cf_observe(fixture,world)&&cf_bag_mass(fixture,world,bag,carried)&&
            vc_native_full(state,world,&ok)&&ok&&vc_held(state,world,da.string(state->key));
        if(!vc_observation(state,"vacuum-full-second-source-native-view-and-mass",ready,views,1))return vc_stop(state);
        if(!s_pulse_begin(state,context,'R',reason))return vc_stop(state);
        state->stage=42;return MF_PENDING;
    }
    if(state->stage==42){
        if(!s_pulse_settled(state,context)||context->now_ms-state->action_ms<320)return MF_PENDING;
        int held=s_pulse_release(state,context);
        vc_observation(state,"vacuum-full-real-r-held",held==1,held,1);
        state->stage=43;return MF_PENDING;
    }
    if(state->stage==43){
        if(!s_pulse_settled(state,context))return MF_PENDING;
        int unchanged=cf_observe(fixture,world)&&cf_bag_mass(fixture,world,bag,carried)&&
            vc_native_full(state,world,&ok)&&ok&&vc_held(state,world,da.string(state->key));
        if(!vc_observation(state,"vacuum-full-refuses-with-source-and-mass-unchanged",unchanged,unchanged,1))return vc_stop(state);
        int retired=cf_retire_retained(fixture,world,bag,carried);
        if(!vc_observation(state,"vacuum-full-owned-refused-source-retired-without-draining-bag",retired,retired,1))return vc_stop(state);
        /* The refused test source is gone; the carried fill remains exactly
         * the original native capacity and is now the amount to pack/drain. */
        fixture->kilograms=carried;state->capsule_full_done=1;
        state->stage=37;state->action_ms=context->now_ms;state->capsule_retry_ms=0;
        return MF_PENDING;
    }
    return vc_stop(state);
}
