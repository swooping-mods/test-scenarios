/* Observe the real map while the production provider consumes OS F12 edges.
 * Native Close is used only by cleanup, never to produce a successful result. */
static int s_map_press(TSState *state,const MoonTestContext *context,const char **reason) {
    if(!s_key_down(state,0x7b,reason))return MF_BLOCKED;
    state->action_ms=context->now_ms;state->stage=2;return MF_PENDING;
}
static int s_map_step(const MoonTestContext *context,void *user,const char **reason) {
    TSState *state=user;int ok=1;
    if(state->stage==0) {
        if(!s_input_available()){*reason="private-input-driver-unavailable";return MF_BLOCKED;}
        if(!s_offer(state->key)||!(shop->state(state->key,context->world,context->walker)&MS_OWNED)){
            *reason="requires-owned-map-run-purchase-case-first";return MF_BLOCKED;
        }
        PTR screen=s_get_component(context->walker,"MoonMapScreen",&ok);
        if(!ok||!s_live(screen))return MF_FAIL;
        state->map=dhold(screen);if(!state->map)return MF_FAIL;
        state->map_was_open=boolean(s_map_is_open,screen,&ok);
        if(!ok)return MF_FAIL;
        if(state->map_was_open){*reason="requires-closed-native-map";return MF_BLOCKED;}
        state->owns_map=1;state->fixture_index=0;
        if(!s_begin_equipment(state,context,reason))return MF_FAIL;
        state->stage=1;return MF_PENDING;
    }
    if(state->stage==1) {
        if(context->frame-state->frame<5)return MF_PENDING;
        if(!state->action_ms)state->action_ms=context->now_ms;
        int first=boolean(s_firstperson,context->walker,&ok);
        int blocked=boolean(s_menu_blocked,context->walker,&ok),typing=boolean(s_typing,0,&ok);
        if(!ok)return MF_FAIL;
        if(!first||blocked||typing){*reason="waiting-for-native-map-input";return context->now_ms-state->action_ms<10000?MF_PENDING:MF_BLOCKED;}
        int equipped=equipment->is_equipped(state->key,context->walker);
        if(!ts->check("map-native-equipped",equipped==1,equipped,1))return MF_FAIL;
        int initially_open=boolean(s_map_is_open,dget(state->map),&ok);
        if(!ts->check("map-initially-closed",!initially_open&&ok,initially_open,0))return MF_FAIL;
        return s_map_press(state,context,reason);
    }
    if(state->stage==2) {
        if(context->now_ms-state->action_ms<150)return MF_PENDING;
        int held=(s_key_state(0x7b)&0x8000)!=0;
        int pass=ts->check("map-os-toggle-key-held",held,held,1);
        s_key_up(state);state->action_ms=context->now_ms;state->stage=3;
        return pass?MF_PENDING:MF_FAIL;
    }
    if(context->now_ms-state->action_ms<500)return MF_PENDING;
    int opened=boolean(s_map_is_open,dget(state->map),&ok);
    int blocked=boolean(s_menu_blocked,context->walker,&ok);
    if(!ok)return MF_FAIL;
    if(state->stage==4) {
        int equipped=equipment->is_equipped(state->key,context->walker);
        int pass=ts->check("map-unequipped-readback",!equipped,equipped,0);
        pass&=ts->check("map-unequip-closes-native-map",!opened,opened,0);
        pass&=ts->check("map-unequip-releases-native-menu",!blocked,blocked,0);
        if(!pass)return MF_FAIL;
        state->fixture_index=5;return s_map_press(state,context,reason);
    }
    if(state->fixture_index==5) {
        int pass=ts->check("map-unequipped-toggle-refused",!opened,opened,0);
        pass&=ts->check("map-unequipped-input-remains-ready",!blocked,blocked,0);
        return pass?MF_PASS:MF_FAIL;
    }
    int expected=(state->fixture_index%2)==0;
    int pass=ts->check(expected?"map-toggle-opens-native-map":"map-toggle-closes-native-map",opened==expected,opened,expected);
    pass&=ts->check("map-native-menu-follows-open-state",blocked==expected,blocked,expected);
    if(!pass)return MF_FAIL;
    if(state->fixture_index==4) {
        if(!s_restore_equipment(state))return MF_FAIL;
        state->owns_slot=0;state->stage=4;state->action_ms=context->now_ms;return MF_PENDING;
    }
    state->fixture_index++;return s_map_press(state,context,reason);
}
