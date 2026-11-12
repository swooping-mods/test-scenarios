/* Real production Zoom response to synthetic OS input in the private Wine/X11
 * session. No provider state or camera lease is forged by this scenario. */
static int s_zoom_begin(const MoonTestContext *context,void *user,const char **reason) {
    TSState *state=user;int ok=1;
    if(state->stage==0) {
        if(!s_key_event||!s_key_state||!s_foreground||!s_window_pid){*reason="private-input-driver-unavailable";return MF_BLOCKED;}
        if(!s_offer(state->key)||!(shop->state(state->key,context->world,context->walker)&MS_OWNED)){
            *reason="requires-owned-zoom-fixture-run-purchase-case-first";return MF_BLOCKED;
        }
        if(!s_begin_equipment(state,context,reason))return MF_FAIL;
        state->stage=1;return MF_PENDING;
    }
    if(state->stage==1) {
        if(context->frame-state->frame<5)return MF_PENDING;
        int first=boolean(s_firstperson,context->walker,&ok);
        int blocked=boolean(s_menu_blocked,context->walker,&ok),typing=boolean(s_typing,0,&ok);
        if(!ok)return MF_FAIL;
        if(!state->action_ms){
            s_note("zoom-first-person",first?"1":"0");s_note("zoom-menu-blocked",blocked?"1":"0");s_note("zoom-typing",typing?"1":"0");
            state->action_ms=context->now_ms;
        }
        if(!first||blocked||typing){
            *reason="waiting-for-native-first-person-input";
            return context->now_ms-state->action_ms<10000?MF_PENDING:MF_BLOCKED;
        }
        int equipped=equipment->is_equipped(state->key,context->walker);
        if(!ts->check("zoom-native-equipped",equipped==1,equipped,1))return MF_FAIL;
        PTR camera=0,field=get_field(object_class(context->walker),"viewCamera");
        if(field)field_value(context->walker,field,&camera);
        if(!s_live(camera))return MF_FAIL;
        if(state->zoom_mode==3) {
            if(!s_camera_api){*reason="shared-camera-service-unavailable";return MF_BLOCKED;}
            state->camera_token=s_camera_api->claim(s_camera_owner,context->world,context->walker,camera,MOON_CAMERA_FOV);
            if(!state->camera_token){*reason="requires-unclaimed-camera-fixture";return MF_BLOCKED;}
            int held=s_camera_api->held(s_camera_owner,state->camera_token);
            if(!ts->check("scenario-camera-lease-held",held,held,1))return MF_FAIL;
        }
        state->original_fov=dnumber(s_fov,camera,&ok);
        if(!ts->check("zoom-valid-original-fov",ok&&state->original_fov>=10&&state->original_fov<=150,(long long)(state->original_fov*1000),10000))return MF_FAIL;
        state->camera=dhold(camera);if(!state->camera)return MF_FAIL;
        if(!s_key_down(state,'Z',reason))return MF_BLOCKED;
        state->action_ms=context->now_ms;
        state->stage=2;return MF_PENDING;
    }
    return MF_FAIL;
}
static int s_zoom_near(float current,float target,float tolerance) {
    return current>=target-tolerance&&current<=target+tolerance;
}
static int s_zoom_held(void) {return (s_key_state('Z')&0x8000)!=0;}
static int s_zoom_step(const MoonTestContext *context,void *user,const char **reason) {
    TSState *state=user;int ok=1;
    if(state->stage<2)return s_zoom_begin(context,user,reason);
    float current=dnumber(s_fov,dget(state->camera),&ok);
    if(!ok)return MF_FAIL;
    if(state->stage==2) {
        if(context->now_ms-state->action_ms<1000)return MF_PENDING;
        int held=(s_key_state('Z')&0x8000)!=0;
        if(!ts->check("zoom-os-key-held",held,held,1))return MF_FAIL;
        float target=state->original_fov/3.f,tolerance=state->original_fov*.02f;
        int pass=ts->check("zoom-threefold-native-fov",current>=target-tolerance&&current<=target+tolerance,(long long)(current*1000),(long long)(target*1000));
        s_key_up(state);state->action_ms=context->now_ms;
        state->stage=3;return pass?MF_PENDING:MF_FAIL;
    }
    if(context->now_ms-state->action_ms<500)return MF_PENDING;
    return ts->check("zoom-release-restores-native-fov",current>=state->original_fov-.01f&&current<=state->original_fov+.01f,(long long)(current*1000),(long long)(state->original_fov*1000))?MF_PASS:MF_FAIL;
}
