/* First qualify the real capsule producer/presenter and rollback. Actual R
 * collection is a later scenario, after this fixture has native evidence. */
static int s_vacuum_capsule_fixture_step(const MoonTestContext *context,void *user,const char **reason) {
    TSState *state=user;CFixture *fixture=&state->capsule;int ok=1;
    if(state->stage<20){
        int result=s_vacuum_container_step(context,user,reason);
        if(result!=MF_PASS)return result;
        state->stage=20;
    }
    if(state->stage==20){
        if(!cf.ready||!sc.ready){*reason="native-capsule-fixture-api-unavailable";return MF_BLOCKED;}
        double mass=vf_double(vf.mass,dget(state->native_inventory),&ok);
        if(!ok)return MF_FAIL;
        if(mass<0||mass>1e-9){*reason="requires-empty-native-inventory";return MF_BLOCKED;}
        PTR camera=0,field=get_field(object_class(context->walker),"viewCamera");
        if(field)field_value(context->walker,field,&camera);
        if(!s_live(camera))return MF_FAIL;
        state->scene_camera=dhold(camera);if(!state->scene_camera)return MF_FAIL;
        PTR hit=0;float nearest=0;int ray=sc_raycast(camera,&hit,&nearest);
        if(ray<0)return MF_FAIL;
        float distance=ray&&nearest<3?nearest-.5f:2.5f;
        if(!(distance>.75f&&distance<=2.5f)){*reason="requires-clear-native-capsule-space";return MF_BLOCKED;}
        PTR transform=call(sc.component_transform,camera,0,&ok);
        DVec origin=dvector(sc.position,transform,&ok),forward=dvector(sc.forward,transform,&ok);
        if(!ok)return MF_FAIL;
        DVec position={origin.x+forward.x*distance,origin.y+forward.y*distance,origin.z+forward.z*distance};
        state->scene_center=position;
        PTR world_transform=call(sc.component_transform,context->world,0,&ok);
        DVec world_origin=dvector(sc.position,world_transform,&ok);
        if(!ok)return MF_FAIL;
        /* Pinned native Place adds world.transform.position to physical-pose
         * records. Snapshot coordinates are local to that world origin. */
        DVec local={position.x-world_origin.x,position.y-world_origin.y,position.z-world_origin.z};
        double kilograms=state->capsule_full?state->native_capacity:.125;
        if(state->capsule_full&&!(kilograms>0&&kilograms<=1)){
            *reason="full-fixture-requires-native-bag-at-most-one-kilogram";return MF_BLOCKED;
        }
        int prepared=cf_prepare(fixture,context->world,dget(state->player),local,kilograms);
        if(prepared<0){*reason="requires-empty-native-capsule-system";return MF_BLOCKED;}
        if(!ts->check("capsule-fixture-native-restore",prepared==1,prepared,1))return MF_FAIL;
        state->stage=21;state->frame=context->frame;return MF_PENDING;
    }
    if(!fixture->validated){
        int observed=cf_observe(fixture,context->world);
        if(!ts->check("capsule-fixture-native-id-owner-mass-pose",observed,observed,1))return MF_FAIL;
        fixture->validated=1;
    }
    int views=cf_view(fixture,context->world,1);
    if(views<0)return MF_FAIL;
    if(!views&&context->frame-state->frame<180)return MF_PENDING;
    if(!ts->check("capsule-fixture-one-real-streamed-view",views==1,views,1))return MF_FAIL;
    PTR transform=call(sc.component_transform,dget(fixture->view),0,&ok);
    DVec position=dvector(sc.position,transform,&ok),expected=state->scene_center;
    int pose=ok&&position.x>=expected.x-.05f&&position.x<=expected.x+.05f&&
        position.y>=expected.y-.05f&&position.y<=expected.y+.05f&&position.z>=expected.z-.05f&&position.z<=expected.z+.05f;
    double mass=vf_double(vf.mass,dget(state->native_inventory),&ok);
    int pass=ts->check("capsule-fixture-streamed-pose",pose,pose,1);
    pass&=ts->check("capsule-fixture-inventory-remains-empty",ok&&mass>=0&&mass<=1e-9,vf_mass_grams(mass),0);
    return pass?MF_PASS:MF_FAIL;
}
