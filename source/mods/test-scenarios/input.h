/* Synthetic input belongs exclusively to this private game window. Never
 * release a key that was already held before this scenario pressed it. */
static int s_input_available(void) {
    return s_key_event&&s_key_state&&s_foreground&&s_window_pid;
}
static int s_key_down(TSState *state,unsigned char key,const char **reason) {
    if(!s_input_available()){*reason="private-input-driver-unavailable";return 0;}
    DWORD pid=0;s_window_pid(s_foreground(),&pid);
    if(pid!=GetCurrentProcessId()){*reason="private-game-window-not-foreground";return 0;}
    if(state->key_held||(s_key_state(key)&0x8000)||
       ((s_key_state(0x10)|s_key_state(0x11)|s_key_state(0x12))&0x8000)){
        *reason="requires-released-key-and-modifiers";return 0;
    }
    s_key_event(key,0,0,0);state->key_held=key;return 1;
}
static void s_key_up(TSState *state) {
    if(state->key_held){s_key_event((unsigned char)state->key_held,0,2,0);state->key_held=0;}
}
