/* Full-capacity input uses real provider actions and independently observed
 * native children. Destroyed-parent entries must be drained by Backspace; the
 * fixture never edits the provider registry, count or roots. */
enum {SCAP_FILL=STICKER_PLACE,SCAP_OVER=5,SCAP_UNDO,SCAP_REFILL,SCAP_GHOST_UNDO,
      SCAP_EMPTY_UNDO,SCAP_PROBE,SCAP_FINAL_UNDO,SCAP_OVER_AFTER_PARENT,SCAP_DRAIN_LIVE};
static int s_capacity_press(TSState *state,const MoonTestContext *context,unsigned action,const char **reason) {
    unsigned char key=action==SCAP_UNDO||action==SCAP_GHOST_UNDO||action==SCAP_EMPTY_UNDO||action==SCAP_FINAL_UNDO||action==SCAP_DRAIN_LIVE?8:'R';
    if(!s_pulse_begin(state,context,key,reason))return MF_BLOCKED;
    state->sticker_action=action;state->stage=3;return MF_PENDING;
}
static int s_capacity_last(TSState *state,int index,int *ok) {
    PTR child=sc_child(state,index,ok);
    if(!*ok||!s_live(child))return 0;
    DRoot root=dhold(child);if(!root)return 0;
    ddrop(&state->scene_last);state->scene_last=root;
    if(!state->scene_first){state->scene_first=dhold(child);if(!state->scene_first)return 0;}
    return 1;
}
static int s_sticker_capacity_step(const MoonTestContext *context,void *user,const char **reason) {
    TSState *state=user;int ok=1;
    if(state->stage<3) {
        if(state->stage==0)state->fixture_round=state->fixture_index=0;
        int result=s_sticker_step(context,user,reason);
        if(result==MF_PENDING&&state->stage==3)s_pulse_mark(state,context);
        return result;
    }
    if(state->stage==3) {
        int released=s_pulse_release(state,context);if(!released)return MF_PENDING;
        if(!ts->check("capacity-os-key-held-before-release",released==1,released,1))return MF_FAIL;
        state->stage=4;return MF_PENDING;
    }
    if(!s_pulse_settled(state,context))return MF_PENDING;
    if(state->stage==5) {
        int gone=!s_live(dget(state->scene_target));
        int first_gone=!s_live(dget(state->scene_first)),last_gone=!s_live(dget(state->scene_last));
        int pass=ts->check("capacity-full-parent-destroyed",gone,!gone,0);
        pass&=ts->check("capacity-parent-destroys-first-sticker",first_gone,!first_gone,0);
        pass&=ts->check("capacity-parent-destroys-last-sticker",last_gone,!last_gone,0);
        if(!pass)return MF_FAIL;
        sc_forget_destroyed_surface(state);state->fixture_index=0;
        int ready=sc_target(state,context,reason);if(ready!=MF_PENDING)return ready;
        state->stage=8;s_pulse_mark(state,context);return MF_PENDING;
    }
    if(state->stage==8) {
        int count=sc_count(state,&ok),aimed=sc_aimed(state);
        int pass=ts->check("capacity-replacement-empty",ok&&count==0,count,0);
        pass&=ts->check("capacity-replacement-ray-hit",aimed,aimed,1);
        if(!pass)return MF_FAIL;
        return s_capacity_press(state,context,SCAP_GHOST_UNDO,reason);
    }
    if(state->stage==6) {
        int equipped=equipment->is_equipped(state->key,context->walker);
        int pass=ts->check("capacity-equipment-restored",!equipped,equipped,0);
        int visible=s_live(dget(state->scene_model));
        pass&=ts->check("capacity-held-model-removed",!visible,visible,0);
        if(!pass)return MF_FAIL;
        sc_destroy(dget(state->scene_target),&ok);if(!ok)return MF_FAIL;
        state->stage=7;s_pulse_mark(state,context);return MF_PENDING;
    }
    if(state->stage==7) {
        int gone=!s_live(dget(state->scene_target));
        if(!ts->check("capacity-replacement-destroyed",gone,!gone,0))return MF_FAIL;
        state->scene_complete=1;return MF_PASS;
    }
    int count=sc_count(state,&ok);if(!ok)return MF_FAIL;
    unsigned action=state->sticker_action;
    if(action==SCAP_FILL) {
        int expected=(int)state->sticker_count+1;
        if(!ts->check("capacity-place-adds-exactly-one",count==expected,count,expected)||
           !s_capacity_last(state,count-1,&ok))return MF_FAIL;
        state->sticker_count++;
        if(state->sticker_count<128)return s_capacity_press(state,context,SCAP_FILL,reason);
        if(!ts->check("capacity-reaches-128",count==128,count,128))return MF_FAIL;
        state->scene_previous=dhold(sc_child(state,126,&ok));if(!ok||!state->scene_previous)return MF_FAIL;
        return s_capacity_press(state,context,state->fixture_round?SCAP_OVER_AFTER_PARENT:SCAP_OVER,reason);
    }
    if(action==SCAP_OVER_AFTER_PARENT) {
        if(!ts->check("capacity-replacement-129th-refused",count==128,count,128))return MF_FAIL;
        return s_capacity_press(state,context,SCAP_DRAIN_LIVE,reason);
    }
    if(action==SCAP_DRAIN_LIVE) {
        int expected=(int)state->sticker_count-1,last=s_live(dget(state->scene_last));
        int pass=ts->check("capacity-full-collection-undo-one-child",count==expected,count,expected);
        pass&=ts->check("capacity-full-collection-undo-newest",!last,last,0);
        if(!pass)return MF_FAIL;
        state->sticker_count--;ddrop(&state->scene_last);
        if(expected>0) {
            if(!s_capacity_last(state,expected-1,&ok))return MF_FAIL;
            return s_capacity_press(state,context,SCAP_DRAIN_LIVE,reason);
        }
        int first=s_live(dget(state->scene_first));
        if(!ts->check("capacity-full-collection-first-also-destroyed",!first,first,0))return MF_FAIL;
        ddrop(&state->scene_first);ddrop(&state->scene_previous);
        return s_capacity_press(state,context,SCAP_EMPTY_UNDO,reason);
    }
    if(action==SCAP_OVER) {
        int last=s_live(dget(state->scene_last));
        int pass=ts->check("capacity-129th-placement-refused",count==128,count,128);
        pass&=ts->check("capacity-refusal-preserves-newest",last,last,1);
        if(!pass)return MF_FAIL;
        return s_capacity_press(state,context,SCAP_UNDO,reason);
    }
    if(action==SCAP_UNDO) {
        int last=s_live(dget(state->scene_last)),previous=s_live(dget(state->scene_previous));
        int pass=ts->check("capacity-undo-opens-one-slot",count==127,count,127);
        pass&=ts->check("capacity-undo-destroys-newest",!last,last,0);
        pass&=ts->check("capacity-undo-preserves-previous",previous,previous,1);
        if(!pass)return MF_FAIL;
        return s_capacity_press(state,context,SCAP_REFILL,reason);
    }
    if(action==SCAP_REFILL) {
        if(!ts->check("capacity-placement-reuses-open-slot",count==128,count,128)||
           !s_capacity_last(state,127,&ok))return MF_FAIL;
        sc_destroy(dget(state->scene_target),&ok);if(!ok)return MF_FAIL;
        state->stage=5;s_pulse_mark(state,context);return MF_PENDING;
    }
    if(action==SCAP_GHOST_UNDO) {
        if(!ts->check("capacity-deleted-parent-undo-keeps-new-surface-empty",count==0,count,0))return MF_FAIL;
        state->fixture_index++;
        if(state->fixture_index<128)return s_capacity_press(state,context,SCAP_GHOST_UNDO,reason);
        /* A single new placement cannot prove all dead entries were removed:
         * even one leaked count would permit it. Refill the entire128 slots. */
        state->fixture_round=1;
        return s_capacity_press(state,context,SCAP_FILL,reason);
    }
    if(action==SCAP_EMPTY_UNDO) {
        if(!ts->check("capacity-129th-undo-is-empty-noop",count==0,count,0))return MF_FAIL;
        return s_capacity_press(state,context,SCAP_PROBE,reason);
    }
    if(action==SCAP_PROBE) {
        if(!ts->check("capacity-placement-after-deleted-parent-undo",count==1,count,1)||
           !s_capacity_last(state,0,&ok))return MF_FAIL;
        return s_capacity_press(state,context,SCAP_FINAL_UNDO,reason);
    }
    if(action!=SCAP_FINAL_UNDO)return MF_FAIL;
    int last=s_live(dget(state->scene_last));
    int pass=ts->check("capacity-final-undo-empty",count==0,count,0);
    pass&=ts->check("capacity-final-sticker-destroyed",!last,last,0);
    if(!pass||!s_restore_equipment(state))return MF_FAIL;
    state->owns_slot=0;state->stage=6;s_pulse_mark(state,context);return MF_PENDING;
}
