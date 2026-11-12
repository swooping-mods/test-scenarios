/* Bounded OS pulses must span both wall time and native frames: elapsed time
 * alone can miss the provider Update when the game is briefly stalled. */
static int s_pulse_settled(const TSState *state,const MoonTestContext *context) {
    return context->frame>=state->frame&&context->frame-state->frame>=2&&
        context->now_ms>=state->action_ms&&context->now_ms-state->action_ms>=80;
}
static void s_pulse_mark(TSState *state,const MoonTestContext *context) {
    state->frame=context->frame;state->action_ms=context->now_ms;
}
static int s_pulse_begin(TSState *state,const MoonTestContext *context,unsigned char key,const char **reason) {
    if(!s_key_down(state,key,reason))return 0;
    s_pulse_mark(state,context);return 1;
}
static int s_pulse_release(TSState *state,const MoonTestContext *context) {
    if(!s_pulse_settled(state,context))return 0;
    int held=state->key_held&&(s_key_state(state->key_held)&0x8000);
    s_key_up(state);s_pulse_mark(state,context);return held?1:-1;
}
