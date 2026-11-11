/* Concrete main-thread scenarios against actual native game/provider APIs. */
#include "../../runtime/native/gameplay.h"
#include "../../runtime/sdk/native_adapters/reflection.h"
#include "../../runtime/native/services.h"
#include "../test-framework/sdk/testing.h"
#include "../shop-framework/sdk.h"
#include "../equipment-framework/sdk/equipment.h"
int _fltused = 0;
static const MoonTesting *ts;
static const MoonTestIsolation *isolation;
static const MoonShop *shop;
static const MoonEquipment *equipment;
static PTR s_alive, s_player, s_wallet, s_decimal_int, s_to_int, s_set_money, s_charge;
static PTR s_component, s_shop_open, s_shop_is_open, s_loadout, s_selected, s_assign, s_select;
static PTR s_clear_slot,s_equip_prototype;
static PTR s_fov,s_set_fov;
static PTR s_firstperson,s_menu_blocked,s_typing;
static PTR s_tree_level,s_tree_max,s_tree_requirements,s_tree_missing,s_tree_buy,s_tree_owned,s_tree_total,s_boots;
static PTR s_tool_level,s_tool_buy,s_tool_id;
static PTR s_sends_achievements;
static void (*s_class_init)(PTR),(*s_static_get)(PTR,void *);
static void (*s_key_event)(unsigned char,unsigned char,DWORD,U64);
static short (*s_key_state)(int);
static PTR (*s_foreground)(void);
static DWORD (*s_window_pid)(PTR,DWORD *);
static PTR s_shop_camera;
static int (*s_string_length)(PTR);
static const unsigned short *(*s_string_chars)(PTR);
static PTR s_log;
static void s_note(const char *event,const char *detail) {
    if(!s_log)return;
    char buffer[500],*end=cat(buffer,"{\"event\":\"");end=cat(end,event);end=cat(end,"\",\"detail\":\"");
    for(unsigned i=0;detail&&detail[i]&&i<127;i++){unsigned char c=(unsigned char)detail[i];*end++=(c>=32&&c<127&&c!='"'&&c!='\\')?(char)c:'?';}
    end=cat(end,"\"}\n");DWORD written=0;WriteFile(s_log,buffer,(DWORD)(end-buffer),&written,0);FlushFileBuffers(s_log);
}
static void s_note_string(const char *event,PTR value) {
    char text[128];int length=value?s_string_length(value):0;
    if(length<0||length>127){s_note(event,"unsupported-string");return;}
    const unsigned short *chars=value?s_string_chars(value):0;
    for(int i=0;i<length;i++)text[i]=chars[i]>=32&&chars[i]<127?(char)chars[i]:'?';
    text[length]=0;s_note(event,text);
}
typedef struct {
    DRoot world, walker, player, menu, previous_tool,previous_prototype,camera;
    int original_money, original_slot, original_belt_slot, test_slot, stage, owns_wallet, owns_slot, menu_was_open;
    unsigned long long frame;
    DWORD thread;
    const char *key;
    int expected_price, equip;
    unsigned long long action_ms;
    float original_fov;
    int key_held;
    DRoot nodes,gear,tools;
    unsigned fixture_index,fixture_round,fixture_progress;
} TSState;
static TSState context_state, wallet_state, catalog_state;
static TSState zoom_state={.key="zoom-module"};
static TSState progression_state;
static TSState locked_states[]={{.key="building-drone"},{.key="joonas-backpack"}};
static TSState tool_states[] = {
    {.key="zoom-module", .expected_price=125, .equip=1},
    {.key="sticker-label-gun", .expected_price=60, .equip=1},
    {.key="handheld-vacuum", .expected_price=150, .equip=1},
    {.key="flag-finder", .expected_price=125, .equip=1},
    {.key="portable-map", .expected_price=200, .equip=1},
    {.key="compass-cardinals", .expected_price=100},
    {.key="conveyor-upgrade-wrench", .expected_price=125},
    {.key="player-catapult", .expected_price=750, .equip=1},
    {.key="building-drone", .expected_price=2000, .equip=1},
    {.key="joonas-backpack", .expected_price=5000, .equip=1},
};
static int s_live(PTR object) {
    int ok=1; void *args[]={object}; PTR value=object?call(s_alive,0,args,&ok):0;
    return ok&&value&&*(unsigned char *)unbox(value);
}
static PTR s_get_component(PTR object,const char *name,int *ok) {
    PTR type=dtype("MoonCleaners",name); void *args[]={type};
    return type?call(s_component,object,args,ok):0;
}
static int s_money(TSState *state,int *ok) {
    void *args[]={dget(state->player)};
    DRoot amount=dhold(call(s_wallet,dget(state->world),args,ok));
    void *convert[]={amount?unbox(dget(amount)):0};
    PTR value=amount?call(s_to_int,0,convert,ok):0;
    int result=value?*(int *)unbox(value):0;
    if(!value)*ok=0;
    ddrop(&amount);return result;
}
static int s_set(TSState *state,int amount) {
    int ok=1; void *args[]={&amount}; DRoot value=dhold(call(s_decimal_int,0,args,&ok));
    if(!value)return 0;
    args[0]=unbox(dget(value));call(s_set_money,dget(state->world),args,&ok);
    ddrop(&value);return ok;
}
static int s_assignment(TSState *state,PTR key,int slot) {
    int ok=1; PTR message=0; void *args[]={dget(state->player),key,&slot,&message};
    PTR result=call(s_assign,dget(state->world),args,&ok);
    int accepted=ok&&result&&*(unsigned char *)unbox(result);
    if(!accepted)s_note_string("native-assignment-refused",message);
    return accepted;
}
static int s_same_string(PTR a,PTR b) {
    int na=a?s_string_length(a):0,nb=b?s_string_length(b):0;if(na!=nb)return 0;
    const unsigned short *ca=a?s_string_chars(a):0,*cb=b?s_string_chars(b):0;
    for(int i=0;i<na;i++)if(ca[i]!=cb[i])return 0;
    return 1;
}
static int s_restore_slot(TSState *state) {
    if(!s_live(dget(state->world)))return 0;
    PTR previous=dget(state->previous_tool);int ok=1;
    if(previous&&s_string_length(previous)>0)(void)s_assignment(state,previous,state->test_slot);
    else {
        PTR message=0;void *args[]={dget(state->player),&state->test_slot,&message};
        call(s_clear_slot,dget(state->world),args,&ok);
    }
    void *args[]={dget(state->player),&state->test_slot};PTR actual=call(s_loadout,dget(state->world),args,&ok);
    return ok&&s_same_string(previous,actual);
}
static int s_capture(const MoonTestContext *context,void *user,const char **reason) {
    TSState *state=user;(void)reason;
    state->stage=0;state->action_ms=0;state->frame=context->frame;state->thread=GetCurrentThreadId();
    state->world=dhold(context->world);state->walker=dhold(context->walker);
    int ok=1;state->player=dhold(call(s_player,0,0,&ok));
    if(!state->world||!state->walker||!state->player||!ok)return MF_FAIL;
    state->original_money=s_money(state,&ok);
    return ok?MF_PENDING:MF_FAIL;
}
static int s_cleanup(const MoonTestContext *context,void *user) {
    TSState *state=user;(void)context;int ok=1;
    if(state->key_held){s_key_event('Z',0,2,0);state->key_held=0;}
    if(state->owns_slot) {
        if(!s_restore_slot(state)){ok=0;s_note("cleanup","assignment-failed");}
        PTR belt=s_get_component(dget(state->walker),"MoonToolbelt",&ok);
        if(belt) {
            if(state->original_belt_slot<0){void *args[]={dget(state->previous_prototype)};call(s_equip_prototype,belt,args,&ok);}
            else {void *args[]={&state->original_belt_slot};call(s_select,belt,args,&ok);}
        }else ok=0;
    }
    if(state->menu&&!state->menu_was_open&&s_live(dget(state->menu))) {
        unsigned char no=0;void *args[]={&no};call(s_shop_open,dget(state->menu),args,&ok);
    }
    if(state->owns_wallet) {
        if(!s_live(dget(state->world))||!s_set(state,state->original_money)){ok=0;s_note("cleanup","wallet-set-failed");}
        int read_ok=1,actual=s_money(state,&read_ok);
        if(!read_ok||actual!=state->original_money){ok=0;s_note("cleanup","wallet-readback-failed");}
    }
    if(state->camera&&s_live(dget(state->camera))) {
        void *args[]={&state->original_fov};call(s_set_fov,dget(state->camera),args,&ok);
        float restored=dnumber(s_fov,dget(state->camera),&ok);
        if(restored<state->original_fov-.01f||restored>state->original_fov+.01f)ok=0;
    }
    ddrop(&state->camera);
    ddrop(&state->nodes);ddrop(&state->gear);ddrop(&state->tools);
    ddrop(&state->previous_tool);ddrop(&state->previous_prototype);ddrop(&state->menu);ddrop(&state->player);
    ddrop(&state->walker);ddrop(&state->world);
    state->owns_slot=state->owns_wallet=state->menu_was_open=state->stage=0;
    return ok;
}
static int s_context(const MoonTestContext *context,void *user,const char **reason) {
    TSState *state=user;(void)reason;
    if(context->frame-state->frame<3)return MF_PENDING;
    int pass=ts->check("same-live-world",context->world==dget(state->world)&&s_live(context->world),1,1);
    pass&=ts->check("same-main-thread",GetCurrentThreadId()==state->thread,GetCurrentThreadId(),state->thread);
    pass&=ts->check("ready-host-focused",context->flags==(MF_READY|MF_HOST|MF_FOCUSED),context->flags,7);
    return pass?MF_PASS:MF_FAIL;
}
static int s_wallet_step(const MoonTestContext *context,void *user,const char **reason) {
    TSState *state=user;(void)context;(void)reason;int ok=1;
    if(state->stage==0) {
        state->owns_wallet=1;
        if(!s_set(state,10000))return MF_FAIL;
        state->stage=1;return MF_PENDING;
    }
    if(state->stage==1) {
        int balance=s_money(state,&ok);
        if(!ts->check("native-set-balance",ok&&balance==10000,balance,10000))return MF_FAIL;
        int amount=-125;void *convert[]={&amount};DRoot price=dhold(call(s_decimal_int,0,convert,&ok));
        if(!price)return MF_FAIL;
        void *args[]={dget(state->player),unbox(dget(price))};
        call(s_charge,dget(state->world),args,&ok);ddrop(&price);
        if(!ok)return MF_FAIL;state->stage=2;return MF_PENDING;
    }
    int balance=s_money(state,&ok);
    return ts->check("native-charge-observed",ok&&balance==9875,balance,9875)?MF_PASS:MF_FAIL;
}
static int s_catalog_step(const MoonTestContext *context,void *user,const char **reason) {
    TSState *state=user;(void)reason;int ok=1;
    if(!state->menu) {
        PTR menu=s_get_component(context->walker,"MoonUpgradeShop",&ok);
        if(!menu&&ok) {
            PTR camera=0;PTR field=get_field(object_class(context->walker),"viewCamera");
            if(field)field_value(context->walker,field,&camera);
            void *args[]={camera};if(camera)menu=call(s_shop_camera,0,args,&ok);
        }
        if(!ok||!s_live(menu))return MF_FAIL;
        state->menu=dhold(menu);state->menu_was_open=boolean(s_shop_is_open,menu,&ok);
        if(!ok)return MF_FAIL;
        (void)shop->open_modded(context->walker);
        state->frame=context->frame;return MF_PENDING;
    }
    if(context->frame-state->frame<3)return MF_PENDING;
    int opened=boolean(s_shop_is_open,dget(state->menu),&ok);
    if(!ok)return MF_FAIL;
    if(!opened){
        if((context->frame-state->frame)%10==0)(void)shop->open_modded(context->walker);
        *reason="waiting-for-native-shop";return MF_PENDING;
    }
    if(!ts->check("native-shop-open",opened,opened,1))return MF_FAIL;
    unsigned count=shop->count();
    if(!ts->check("bounded-shop-count",count<=MS_LIMIT,count,MS_LIMIT))return MF_FAIL;
    for(unsigned i=0;i<count;i++) {
        const MoonShopDefinition *offer=shop->get(i);
        if(!ts->check("valid-offer",offer&&offer->key&&offer->price>=0,offer?offer->price:-1,0))return MF_FAIL;
        for(unsigned j=0;j<i;j++)if(!ts->check("unique-offer-id",!deq(offer->key,shop->get(j)->key),i,j))return MF_FAIL;
    }
    return ts->check("past-end-refused",shop->get(count)==0,shop->get(count)!=0,0)?MF_PASS:MF_FAIL;
}
static const MoonShopDefinition *s_offer(const char *key) {
    for(unsigned i=0;i<shop->count();i++){const MoonShopDefinition *offer=shop->get(i);if(offer&&deq(offer->key,key))return offer;}
    return 0;
}
static int s_locked_step(const MoonTestContext *context,void *user,const char **reason) {
    TSState *state=user;int ok=1;
    if(!s_offer(state->key)){*reason="provider-not-selected";return MF_BLOCKED;}
    unsigned before=shop->state(state->key,context->world,context->walker);
    if((before&MS_OWNED)||!(before&MS_BLOCKED)){*reason="requires-progression-locked-fixture";return MF_BLOCKED;}
    state->owns_wallet=1;
    if(!s_set(state,100000))return MF_FAIL;
    int bought=shop->purchase(state->key,context->world,context->walker);
    int money=s_money(state,&ok);
    unsigned after=shop->state(state->key,context->world,context->walker);
    int pass=ts->check("progression-purchase-refused",bought==0,bought,0);
    pass&=ts->check("locked-purchase-no-charge",ok&&money==100000,money,100000);
    pass&=ts->check("locked-purchase-not-owned",!(after&MS_OWNED),after,MS_BLOCKED);
    pass&=ts->check("progression-gate-retained",(after&MS_BLOCKED)!=0,after,MS_BLOCKED);
    return pass?MF_PASS:MF_FAIL;
}
static int s_begin_equipment(TSState *state,const MoonTestContext *context,const char **reason) {
    int ok=1;void *player_args[]={dget(state->player)};
    PTR selected=call(s_selected,context->world,player_args,&ok);
    if(!selected||!ok)return 0;state->original_slot=*(int *)unbox(selected);
    /* Starter worlds unlock slot zero only. Empty slots restore through the
     * native clear producer rather than TryAssignTool(empty string). */
    state->test_slot=0;
    void *slot_args[]={dget(state->player),&state->test_slot};
    PTR previous=call(s_loadout,context->world,slot_args,&ok);
    state->previous_tool=dhold(previous);s_note_string("captured-slot-tool",previous);
    PTR belt=s_get_component(context->walker,"MoonToolbelt",&ok),prototype=0;
    PTR index_field=belt?get_field(object_class(belt),"selectedIndex"):0;
    PTR prototype_field=belt?get_field(object_class(belt),"prototypeTool"):0;
    if(!ok||!index_field||!prototype_field)return 0;
    field_value(belt,index_field,&state->original_belt_slot);field_value(belt,prototype_field,&prototype);
    state->previous_prototype=dhold(prototype);if(prototype&&!state->previous_prototype)return 0;
    state->owns_slot=1;
    if(!s_assignment(state,da.string(state->key),state->test_slot)){*reason="native-assignment-refused";return 0;}
    if(!equipment->equip(state->key,context->walker)){*reason="native-equip-refused";return 0;}
    state->frame=context->frame;return 1;
}
static int s_tool_step(const MoonTestContext *context,void *user,const char **reason) {
    TSState *state=user;int ok=1;
    const MoonShopDefinition *offer=s_offer(state->key);
    if(!offer){*reason="provider-not-selected";return MF_BLOCKED;}
    if(state->stage==0) {
        unsigned status=shop->state(state->key,context->world,context->walker);
        if(status&MS_OWNED){*reason="requires-unowned-fixture";return MF_BLOCKED;}
        if((status&(MS_AVAILABLE|MS_BLOCKED))!=MS_AVAILABLE){*reason="progression-or-provider-gate";return MF_BLOCKED;}
        if(!ts->check("authored-price",offer->price==state->expected_price,offer->price,state->expected_price))return MF_FAIL;
        state->owns_wallet=1;
        int insufficient=offer->price>0?offer->price-1:0;
        if(!s_set(state,insufficient))return MF_FAIL;
        int denied=shop->purchase(state->key,context->world,context->walker);
        int balance=s_money(state,&ok);
        if(!ts->check("insufficient-refused",denied==0,denied,0)||
           !ts->check("insufficient-no-charge",ok&&balance==insufficient,balance,insufficient))return MF_FAIL;
        if(!s_set(state,100000))return MF_FAIL;
        state->stage=1;return MF_PENDING;
    }
    if(state->stage==1) {
        int before=s_money(state,&ok);
        if(!ok||before!=100000)return MF_FAIL;
        int bought=shop->purchase(state->key,context->world,context->walker);
        int after=s_money(state,&ok);
        if(!ts->check("provider-purchase",bought==1,bought,1)||
           !ts->check("exact-native-wallet-debit",ok&&after==before-offer->price,after,before-offer->price))return MF_FAIL;
        unsigned status=shop->state(state->key,context->world,context->walker);
        if(!ts->check("provider-owned",(status&MS_OWNED)!=0,status,MS_OWNED))return MF_FAIL;
        int duplicate=shop->purchase(state->key,context->world,context->walker);
        int repeated=s_money(state,&ok);
        if(!ts->check("duplicate-refused",duplicate==0,duplicate,0)||
           !ts->check("duplicate-no-charge",ok&&repeated==after,repeated,after))return MF_FAIL;
        if(!state->equip)return MF_PASS;
        if(!s_begin_equipment(state,context,reason))return MF_FAIL;
        state->stage=2;return MF_PENDING;
    }
    if(context->frame-state->frame<5)return MF_PENDING;
    int equipped=equipment->is_equipped(state->key,context->walker);
    return ts->check("native-slot-equipped",equipped==1,equipped,1)?MF_PASS:MF_FAIL;
}
#include "zoom.h"
#include "progression.h"
#define TS_CASE(id,timeout,callback,user) {MF_ABI,sizeof(MoonTestCase),"test-scenarios." id,"test-scenarios",timeout,MF_READY|MF_HOST|MF_FOCUSED,s_capture,callback,s_cleanup,user}
static const MoonTestCase s_cases[]={
    TS_CASE("native-context",5000,s_context,&context_state),
    TS_CASE("native-wallet",5000,s_wallet_step,&wallet_state),
    TS_CASE("shop-catalog",5000,s_catalog_step,&catalog_state),
    TS_CASE("zoom-module",10000,s_tool_step,&tool_states[0]),
    TS_CASE("sticker-label-gun",10000,s_tool_step,&tool_states[1]),
    TS_CASE("handheld-vacuum",10000,s_tool_step,&tool_states[2]),
    TS_CASE("flag-finder",10000,s_tool_step,&tool_states[3]),
    TS_CASE("portable-map",10000,s_tool_step,&tool_states[4]),
    TS_CASE("compass-cardinals",10000,s_tool_step,&tool_states[5]),
    TS_CASE("conveyor-upgrade-wrench",10000,s_tool_step,&tool_states[6]),
    TS_CASE("player-catapult",10000,s_tool_step,&tool_states[7]),
    TS_CASE("building-drone",10000,s_tool_step,&tool_states[8]),
    TS_CASE("joonas-backpack",10000,s_tool_step,&tool_states[9]),
    TS_CASE("building-drone-locked",5000,s_locked_step,&locked_states[0]),
    TS_CASE("joonas-backpack-locked",5000,s_locked_step,&locked_states[1]),
    TS_CASE("zoom-optics",20000,s_zoom_step,&zoom_state),
    TS_CASE("prepare-late-game",30000,s_progression_step,&progression_state),
};
#undef TS_CASE
__declspec(dllexport) int MoonLab_ModStart(PTR domain,PTR game) {
    (void)prepare_hook;(void)install_hooks;(void)integer;(void)cat;(void)num;
    if(!bind_runtime(domain,game)||!dapi(game))return 0;
    s_string_length=(int(*)(PTR))GetProcAddress(game,"il2cpp_string_length");
    s_string_chars=(const unsigned short *(*)(PTR))GetProcAddress(game,"il2cpp_string_chars");
    if(!s_string_length||!s_string_chars)return 0;
    char folder[1000],path[1100];DWORD length=GetEnvironmentVariableA("MOONLAB_EVIDENCE",folder,sizeof(folder));
    if(length&&length<sizeof(folder)){
        cat(cat(path,folder),"\\test-scenarios.jsonl");s_log=CreateFileA(path,0x40000000,1,0,1,0x80,0);
        if(s_log==(PTR)(U64)-1)s_log=0;
    }
    const MoonRuntimeApiV1 *runtime=moon_runtime_connect();if(!runtime)return 0;
    ts=runtime->find("test-framework","testing",MF_ABI,sizeof(MoonTesting));
    isolation=runtime->find("test-framework","isolation",MF_ABI,sizeof(MoonTestIsolation));
    shop=runtime->find("shop-framework","shop",MS_ABI,sizeof(MoonShop));
    equipment=runtime->find("equipment-framework","equipment",ME_ABI,sizeof(MoonEquipment));
    if(!ts||!isolation||!shop||!equipment)return 0;
#define S_BIND(to,ns,type,name,signature) to=dmethod(ns,type,name,signature);if(!to)return 0;
    S_BIND(s_alive,"UnityEngine","Object","op_Implicit","UnityEngine.Object");
    S_BIND(s_player,"MoonCleaners","MoonCrew","get_LocalPlayerId","");
    S_BIND(s_wallet,"MoonCleaners","MoonWorld","WalletDollars","System.String");
    S_BIND(s_decimal_int,"System","Decimal","op_Implicit","System.Int32");
    S_BIND(s_to_int,"System","Decimal","ToInt32","System.Decimal");
    S_BIND(s_set_money,"MoonCleaners","MoonWorld","SetTeamDollars","System.Decimal");
    S_BIND(s_charge,"MoonCleaners","MoonWorld","ChargeBuild","System.String;System.Decimal");
    S_BIND(s_component,"UnityEngine","Component","GetComponent","System.Type");
    S_BIND(s_shop_open,"MoonCleaners","MoonUpgradeShop","SetOpen","System.Boolean");
    S_BIND(s_shop_is_open,"MoonCleaners","MoonUpgradeShop","get_IsOpen","");
    s_shop_camera=method("MoonCleaners","MoonUpgradeShop","ForCamera",1);if(!s_shop_camera)return 0;
    S_BIND(s_loadout,"MoonCleaners","MoonWorld","LoadoutToolId","System.String;System.Int32");
    S_BIND(s_selected,"MoonCleaners","MoonWorld","SelectedLoadoutSlot","System.String");
    S_BIND(s_assign,"MoonCleaners","MoonWorld","TryAssignTool","System.String;System.String;System.Int32;System.String&");
    S_BIND(s_clear_slot,"MoonCleaners","MoonWorld","TryClearEquipmentSlot","System.String;System.Int32;System.String&");
    S_BIND(s_select,"MoonCleaners","MoonToolbelt","SelectSlot","System.Int32");
    S_BIND(s_equip_prototype,"MoonCleaners","MoonToolbelt","EquipPrototype","MoonCleaners.MoonToolDefinition");
    S_BIND(s_fov,"UnityEngine","Camera","get_fieldOfView","");
    S_BIND(s_set_fov,"UnityEngine","Camera","set_fieldOfView","System.Single");
    S_BIND(s_firstperson,"MoonCleaners","MoonWalker","get_IsFirstPerson","");
    S_BIND(s_menu_blocked,"MoonCleaners","MoonWalker","get_MenuBlocked","");
    S_BIND(s_typing,"MoonCleaners","MoonInput","get_Typing","");
    S_BIND(s_tree_level,"MoonCleaners","MoonWorld","TreeLevel","System.String;System.String");
    S_BIND(s_tree_max,"MoonCleaners","MoonWorld","TreeMaxLevel","MoonCleaners.MoonUpgradeTree.Node");
    S_BIND(s_tree_requirements,"MoonCleaners","MoonWorld","TreeRequirementsMet","System.String;MoonCleaners.MoonUpgradeTree.Node");
    S_BIND(s_tree_missing,"MoonCleaners","MoonWorld","TreeToolMissing","System.String;MoonCleaners.MoonUpgradeTree.Node");
    S_BIND(s_tree_buy,"MoonCleaners","MoonWorld","TryPurchaseTreeNode","System.String;System.String;System.Int32;System.String&");
    S_BIND(s_tree_owned,"MoonCleaners","MoonWorld","TreeLevelsOwned","System.String");
    S_BIND(s_tree_total,"MoonCleaners","MoonWorld","TreeLevelsTotal","");
    S_BIND(s_boots,"MoonCleaners","MoonWorld","HasGravityBoots","System.String");
    S_BIND(s_tool_level,"MoonCleaners","MoonWorld","ToolLevel","System.String;System.String");
    S_BIND(s_tool_buy,"MoonCleaners","MoonWorld","TryPurchaseTool","System.String;System.String;System.Int32;System.String&");
    S_BIND(s_tool_id,"MoonCleaners","MoonToolDefinition","get_Id","");
    S_BIND(s_sends_achievements,"MoonCleaners","MoonAchievements","get_SendsToSteam","");
#undef S_BIND
    if(method_token(s_tree_buy)!=100665707||method_token(s_tool_buy)!=100665742)return 0;
    s_class_init=(void(*)(PTR))GetProcAddress(game,"il2cpp_runtime_class_init");
    s_static_get=(void(*)(PTR,void *))GetProcAddress(game,"il2cpp_field_static_get_value");
    const char *achievement_methods[]={"get_SendsToSteam","Flush","SyncWithSteam","Unlock","Add","Raise","OnMoney","OnPurchase"};
    const unsigned achievement_arities[]={0,0,0,1,2,2,2,3};
    for(unsigned i=0;i<8;i++){
        PTR m=method("MoonCleaners","MoonAchievements",achievement_methods[i],(int)achievement_arities[i]);
        if(m){char value[32];num(value,(U64)*(PTR *)m-(U64)game);s_note(achievement_methods[i],value);}
    }
    PTR user32=GetModuleHandleA("user32.dll");
    s_key_event=(void(*)(unsigned char,unsigned char,DWORD,U64))GetProcAddress(user32,"keybd_event");
    s_key_state=(short(*)(int))GetProcAddress(user32,"GetAsyncKeyState");
    s_foreground=(PTR(*)(void))GetProcAddress(user32,"GetForegroundWindow");
    s_window_pid=(DWORD(*)(PTR,DWORD *))GetProcAddress(user32,"GetWindowThreadProcessId");
    const char *(*parameter_name)(PTR,DWORD)=(const char *(*)(PTR,DWORD))GetProcAddress(game,"il2cpp_method_get_param_name");
    if(parameter_name){
        s_note("assign-parameter-0",parameter_name(s_assign,0));s_note("assign-parameter-1",parameter_name(s_assign,1));
        s_note("native-tool-buy-parameter-0",parameter_name(s_tool_buy,0));s_note("native-tool-buy-parameter-1",parameter_name(s_tool_buy,1));
    }
    return ts->register_cases(s_cases,sizeof(s_cases)/sizeof(*s_cases));
}
