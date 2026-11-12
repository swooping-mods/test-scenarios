/* Native eligibility transitions while the OS key remains held. The Input
 * contract permits held resumption; these scenarios never forge a press edge. */
static int s_zoom_interruption_step(const MoonTestContext *context,void *user,const char **reason) {
    TSState *state=user;int ok=1;
    if(state->stage<2)return s_zoom_begin(context,user,reason);
    if(context->now_ms-state->action_ms<(state->stage==2||state->stage==4?1000:700))return MF_PENDING;
    float current=dnumber(s_fov,dget(state->camera),&ok),target=state->original_fov/3.f;
    if(!ok)return MF_FAIL;
    int held=s_zoom_held();
    if(state->stage==2) {
        int pass=ts->check("zoom-before-interruption",s_zoom_near(current,target,state->original_fov*.02f),(long long)(current*1000),(long long)(target*1000));
        pass&=ts->check("zoom-interruption-key-held",held,held,1);
        if(!pass)return MF_FAIL;
        if(state->zoom_mode==1) {
            PTR menu=s_native_shop(context,&ok);
            if(!ok||!s_live(menu))return MF_FAIL;
            state->menu=dhold(menu);if(!state->menu)return MF_FAIL;
            state->menu_was_open=boolean(s_shop_is_open,menu,&ok);
            if(!ok)return MF_FAIL;
            if(state->menu_was_open){*reason="requires-closed-native-shop";return MF_BLOCKED;}
            unsigned char yes=1;void *args[]={&yes};call(s_shop_open,menu,args,&ok);
            if(!ok)return MF_FAIL;
        }else {
            if(!s_restore_equipment(state))return MF_FAIL;
            state->owns_slot=0;
        }
        state->stage=3;state->action_ms=context->now_ms;return MF_PENDING;
    }
    int blocked=boolean(s_menu_blocked,context->walker,&ok);
    if(!ok)return MF_FAIL;
    if(state->stage==3) {
        int actual=state->zoom_mode==1?boolean(s_shop_is_open,dget(state->menu),&ok):equipment->is_equipped(state->key,context->walker);
        int expected=state->zoom_mode==1;
        int pass=ts->check("zoom-native-interruption-observed",ok&&actual==expected,actual,expected);
        pass&=ts->check("zoom-interrupted-key-still-held",held,held,1);
        pass&=ts->check("zoom-interruption-restores-fov",s_zoom_near(current,state->original_fov,.01f),(long long)(current*1000),(long long)(state->original_fov*1000));
        pass&=ts->check("zoom-native-menu-gate",blocked==expected,blocked,expected);
        if(!pass)return MF_FAIL;
        if(state->zoom_mode==1) {
            unsigned char no=0;void *args[]={&no};call(s_shop_open,dget(state->menu),args,&ok);
            if(!ok)return MF_FAIL;
        }else {
            /* Keep the original snapshot for cleanup; only reapply its test
             * slot instead of capturing another snapshot over those roots. */
            state->owns_slot=1;
            if(!s_assignment(state,da.string(state->key),state->test_slot)||!equipment->equip(state->key,context->walker))return MF_FAIL;
        }
        state->stage=4;state->action_ms=context->now_ms;return MF_PENDING;
    }
    if(state->stage==4) {
        int equipped=equipment->is_equipped(state->key,context->walker);
        int pass=ts->check("zoom-eligibility-returned",equipped==1&&!blocked,equipped,1);
        pass&=ts->check("zoom-resume-key-still-held",held,held,1);
        pass&=ts->check("zoom-held-resumes-after-interruption",s_zoom_near(current,target,state->original_fov*.02f),(long long)(current*1000),(long long)(target*1000));
        s_key_up(state);state->stage=5;state->action_ms=context->now_ms;
        return pass?MF_PENDING:MF_FAIL;
    }
    return ts->check("zoom-interrupted-release-restores-fov",s_zoom_near(current,state->original_fov,.01f),(long long)(current*1000),(long long)(state->original_fov*1000))?MF_PASS:MF_FAIL;
}
static int s_zoom_camera_step(const MoonTestContext *context,void *user,const char **reason) {
    TSState *state=user;int ok=1;
    if(state->stage<2)return s_zoom_begin(context,user,reason);
    if(context->now_ms-state->action_ms<(state->stage==4?700:1000))return MF_PENDING;
    float current=dnumber(s_fov,dget(state->camera),&ok);
    if(!ok)return MF_FAIL;
    if(state->stage==2) {
        int held=s_zoom_held(),owned=s_camera_api->held(s_camera_owner,state->camera_token);
        int pass=ts->check("zoom-camera-busy-key-held",held,held,1);
        pass&=ts->check("zoom-camera-peer-lease-retained",owned,owned,1);
        pass&=ts->check("zoom-camera-busy-no-fov-write",s_zoom_near(current,state->original_fov,.01f),(long long)(current*1000),(long long)(state->original_fov*1000));
        if(!pass)return MF_FAIL;
        int released=s_camera_api->release(s_camera_owner,state->camera_token);
        if(!ts->check("zoom-camera-peer-release",released,released,1))return MF_FAIL;
        state->camera_token=0;state->stage=3;state->action_ms=context->now_ms;return MF_PENDING;
    }
    if(state->stage==3) {
        float target=state->original_fov/3.f;int held=s_zoom_held();
        int pass=ts->check("zoom-camera-retry-key-still-held",held,held,1);
        pass&=ts->check("zoom-camera-retries-after-peer-release",s_zoom_near(current,target,state->original_fov*.02f),(long long)(current*1000),(long long)(target*1000));
        state->camera_token=s_camera_api->claim(s_camera_owner,context->world,context->walker,dget(state->camera),MOON_CAMERA_FOV);
        pass&=ts->check("zoom-owned-fov-refuses-competitor",!state->camera_token,state->camera_token,0);
        s_key_up(state);state->stage=4;state->action_ms=context->now_ms;
        return pass?MF_PENDING:MF_FAIL;
    }
    if(!ts->check("zoom-camera-release-restores-fov",s_zoom_near(current,state->original_fov,.01f),(long long)(current*1000),(long long)(state->original_fov*1000)))return MF_FAIL;
    state->camera_token=s_camera_api->claim(s_camera_owner,context->world,context->walker,dget(state->camera),MOON_CAMERA_FOV);
    if(!ts->check("zoom-releases-fov-for-next-owner",state->camera_token!=0,state->camera_token!=0,1))return MF_FAIL;
    /* Cleanup releases our final witness lease while its targets stay rooted. */
    return MF_PASS;
}
