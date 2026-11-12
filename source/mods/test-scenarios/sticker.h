/* Actual provider controls and independently observed native scene state. */
enum {STICKER_PLACE=1,STICKER_CYCLE,STICKER_UNDO,STICKER_DENIED};
static int s_sticker_press(TSState *state,const MoonTestContext *context,unsigned action,const char **reason) {
    unsigned char key=action==STICKER_CYCLE?'Q':action==STICKER_UNDO?8:'R';
    if(!s_key_down(state,key,reason))return MF_BLOCKED;
    state->sticker_action=action;state->stage=3;state->action_ms=context->now_ms;return MF_PENDING;
}
static int s_sticker_step(const MoonTestContext *context,void *user,const char **reason) {
    TSState *state=user;int ok=1;
    if(state->stage==0) {
        if(!sc.ready){s_note("scene-binding-failed",sc.failed);*reason="native-scene-fixture-bindings-unavailable";return MF_BLOCKED;}
        if(!s_input_available()){*reason="private-input-driver-unavailable";return MF_BLOCKED;}
        if(!s_offer(state->key)||!(shop->state(state->key,context->world,context->walker)&MS_OWNED)){
            *reason="requires-owned-sticker-run-purchase-case-first";return MF_BLOCKED;
        }
        state->sticker_count=state->scene_complete=0;
        if(!s_begin_equipment(state,context,reason))return MF_FAIL;
        state->stage=1;return MF_PENDING;
    }
    if(state->stage==1) {
        if(context->frame-state->frame<5)return MF_PENDING;
        if(!state->action_ms)state->action_ms=context->now_ms;
        int first=boolean(s_firstperson,context->walker,&ok),blocked=boolean(s_menu_blocked,context->walker,&ok),typing=boolean(s_typing,0,&ok);
        if(!ok)return MF_FAIL;
        if(!first||blocked||typing){*reason="waiting-for-native-sticker-input";return context->now_ms-state->action_ms<10000?MF_PENDING:MF_BLOCKED;}
        PTR model=sc_find("Handheld sticker label gun",&ok);
        if(!ok)return MF_FAIL;
        if(!s_live(model)){*reason="sticker-model-did-not-attach";return context->now_ms-state->action_ms<5000?MF_PENDING:MF_FAIL;}
        int equipped=equipment->is_equipped(state->key,context->walker);
        if(!ts->check("sticker-native-equipped",equipped==1,equipped,1)||!ts->check("sticker-held-model-present",s_live(model),s_live(model),1))return MF_FAIL;
        state->scene_model=dhold(model);if(!state->scene_model)return MF_FAIL;
        int ready=sc_target(state,context,reason);if(ready!=MF_PENDING)return ready;
        state->stage=2;state->frame=context->frame;return MF_PENDING;
    }
    if(state->stage==2) {
        if(context->frame-state->frame<3)return MF_PENDING;
        int count=sc_count(state,&ok);PTR collider=sc_game_component(dget(state->scene_target),"Collider",&ok);
        int enabled=collider?boolean(sc.collider_enabled,collider,&ok):0;
        int pass=ts->check("sticker-fixture-empty",ok&&count==0,count,0);
        pass&=ts->check("sticker-fixture-real-collider",ok&&enabled,enabled,1);
        int aimed=sc_aimed(state);pass&=ts->check("sticker-fixture-native-ray-hit",aimed,aimed,1);
        if(!pass)return MF_FAIL;
        return s_sticker_press(state,context,STICKER_PLACE,reason);
    }
    if(state->stage==3) {
        if(context->now_ms-state->action_ms<150)return MF_PENDING;
        int held=(s_key_state(state->key_held)&0x8000)!=0;
        int pass=ts->check("sticker-os-action-key-held",held,held,1);
        if(state->sticker_action==STICKER_PLACE)sc_input_diagnostic(context,state);
        s_key_up(state);state->stage=4;state->action_ms=context->now_ms;return pass?MF_PENDING:MF_FAIL;
    }
    if(context->now_ms-state->action_ms<300)return MF_PENDING;
    if(state->stage==5) {
        DVec actual=dvector(sc.position,dget(state->scene_first),&ok),before=state->scene_first_position;
        int moved=ok&&sc_close(actual.x-before.x,.4f)&&sc_close(actual.y-before.y,.1f)&&sc_close(actual.z-before.z,.2f);
        if(!ts->check("sticker-follows-moving-parent",moved,(long long)((actual.x-before.x)*1000),400))return MF_FAIL;
        if(!sc_set_position(dget(state->scene_transform),state->scene_center))return MF_FAIL;
        return s_sticker_press(state,context,STICKER_UNDO,reason);
    }
    if(state->stage==6) {
        int equipped=equipment->is_equipped(state->key,context->walker);
        int pass=ts->check("sticker-unequipped-readback",!equipped,equipped,0);
        pass&=ts->check("sticker-unequip-removes-held-model",!s_live(dget(state->scene_model)),s_live(dget(state->scene_model)),0);
        int aimed=sc_aimed(state);pass&=ts->check("sticker-unequipped-target-still-aimed",aimed,aimed,1);
        if(!pass)return MF_FAIL;
        return s_sticker_press(state,context,STICKER_DENIED,reason);
    }
    if(state->stage==7) {
        int gone=!s_live(dget(state->scene_target));
        if(!ts->check("sticker-fixture-destroyed",gone,!gone,0))return MF_FAIL;
        state->scene_complete=1;return MF_PASS;
    }
    int count=sc_count(state,&ok);if(!ok)return MF_FAIL;
    if(state->sticker_action==STICKER_DENIED) {
        if(!ts->check("sticker-unequipped-placement-refused",count==0,count,0))return MF_FAIL;
        sc_destroy(dget(state->scene_target),&ok);if(!ok)return MF_FAIL;
        state->stage=7;state->action_ms=context->now_ms;return MF_PENDING;
    }
    if(state->sticker_action==STICKER_CYCLE) {
        if(!ts->check("sticker-cycle-does-not-place",count==(int)state->sticker_count,count,state->sticker_count))return MF_FAIL;
        return s_sticker_press(state,context,STICKER_PLACE,reason);
    }
    if(state->sticker_action==STICKER_PLACE) {
        int expected=(int)state->sticker_count+1;
        if(!ts->check("sticker-r-places-one-native-child",count==expected,count,expected))return MF_FAIL;
        PTR child=sc_child(state,count-1,&ok);
        if(!ok||!s_live(child)||!sc_sticker(state,child,state->sticker_count))return MF_FAIL;
        ddrop(&state->scene_previous);state->scene_previous=state->scene_last;state->scene_last=dhold(child);
        if(!state->scene_last)return MF_FAIL;
        if(!state->scene_first){state->scene_first=dhold(child);if(!state->scene_first)return MF_FAIL;}
        state->sticker_count++;
        if(state->sticker_count<5)return s_sticker_press(state,context,STICKER_CYCLE,reason);
        state->scene_first_position=dvector(sc.position,dget(state->scene_first),&ok);
        DVec moved=state->scene_center;moved.x+=.4f;moved.y+=.1f;moved.z+=.2f;
        if(!ok||!sc_set_position(dget(state->scene_transform),moved))return MF_FAIL;
        state->stage=5;state->action_ms=context->now_ms;return MF_PENDING;
    }
    int expected=(int)state->sticker_count-1;
    int pass=ts->check("sticker-undo-removes-one-child",count==expected,count,expected);
    pass&=ts->check("sticker-undo-destroys-newest",!s_live(dget(state->scene_last)),s_live(dget(state->scene_last)),0);
    if(expected>0)pass&=ts->check("sticker-undo-preserves-previous",s_live(dget(state->scene_previous)),s_live(dget(state->scene_previous)),1);
    if(!pass)return MF_FAIL;
    state->sticker_count--;
    ddrop(&state->scene_last);state->scene_last=state->scene_previous;state->scene_previous=0;
    if(expected>1){state->scene_previous=dhold(sc_child(state,expected-2,&ok));if(!ok||!state->scene_previous)return MF_FAIL;}
    if(expected>0)return s_sticker_press(state,context,STICKER_UNDO,reason);
    if(!s_restore_equipment(state))return MF_FAIL;
    state->owns_slot=0;state->stage=6;state->action_ms=context->now_ms;return MF_PENDING;
}
