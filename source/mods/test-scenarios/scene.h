/* Owned native Unity fixture objects. Physics hits and renderer properties are
 * queried through their real APIs; no provider event or hit result is forged. */
typedef struct {float x,y;} SCVec2;
static struct {
    PTR primitive,go_transform,component_transform,go_component,set_name,destroy,find;
    PTR position,set_position,forward,scale,children,child;
    PTR collider_enabled,material,has_property,texture,texture_scale,texture_offset;
    PTR ray,hit_transform,hit_distance,sync;
    PTR keyboard,key_item,key_pressed,key_r;
    int ready;const char *failed;
} sc;
static int sc_bind(PTR game) {
#define SC(field,type,name,signature) sc.field=dmethod("UnityEngine",type,name,signature);if(!sc.field){sc.failed=type "::" name;return 0;}
    SC(primitive,"GameObject","CreatePrimitive","UnityEngine.PrimitiveType");
    SC(go_transform,"GameObject","get_transform","");
    SC(component_transform,"Component","get_transform","");
    SC(go_component,"GameObject","GetComponent","System.Type");
    SC(set_name,"Object","set_name","System.String");SC(destroy,"Object","Destroy","UnityEngine.Object");
    SC(find,"GameObject","Find","System.String");
    SC(position,"Transform","get_position","");SC(set_position,"Transform","set_position","UnityEngine.Vector3");
    SC(forward,"Transform","get_forward","");SC(scale,"Transform","set_localScale","UnityEngine.Vector3");
    SC(children,"Transform","get_childCount","");SC(child,"Transform","GetChild","System.Int32");
    SC(collider_enabled,"Collider","get_enabled","");SC(material,"Renderer","get_sharedMaterial","");
    SC(has_property,"Material","HasProperty","System.String");SC(texture,"Material","GetTexture","System.String");
    SC(texture_scale,"Material","GetTextureScale","System.String");SC(texture_offset,"Material","GetTextureOffset","System.String");
    SC(ray,"Physics","Raycast","UnityEngine.Vector3;UnityEngine.Vector3;UnityEngine.RaycastHit&;System.Single;System.Int32;UnityEngine.QueryTriggerInteraction");
    SC(hit_transform,"RaycastHit","get_transform","");SC(hit_distance,"RaycastHit","get_distance","");
    SC(sync,"Physics","SyncTransforms","");
#undef SC
    sc.keyboard=dmethod("UnityEngine.InputSystem","Keyboard","get_current","");
    sc.key_item=dmethod("UnityEngine.InputSystem","Keyboard","get_Item","UnityEngine.InputSystem.Key");
    sc.key_pressed=dmethod("UnityEngine.InputSystem.Controls","ButtonControl","get_isPressed","");
    sc.key_r=get_field(find_class("UnityEngine.InputSystem","Key"),"R");
    int (*size)(PTR,DWORD *)=(int(*)(PTR,DWORD *))GetProcAddress(game,"il2cpp_class_value_size");
    DWORD alignment=0;PTR hit=find_class("UnityEngine","RaycastHit");
    int bytes=size&&hit?size(hit,&alignment):0;
    sc.ready=bytes>0&&bytes<=128&&alignment>0&&alignment<=16;
    sc.failed=sc.ready?0:"RaycastHit value layout";return sc.ready;
}
static void sc_input_diagnostic(const MoonTestContext *context,TSState *state) {
    int ok=1;PTR keyboard=sc.keyboard?call(sc.keyboard,0,0,&ok):0,control=0;
    if(ok&&keyboard&&sc.key_item&&sc.key_r&&s_static_get) {
        int key=0;s_static_get(sc.key_r,&key);void *args[]={&key};control=call(sc.key_item,keyboard,args,&ok);
    }
    int pressed=ok&&control&&sc.key_pressed?boolean(sc.key_pressed,control,&ok):-1;
    s_note("sticker-native-r-held",!ok||pressed<0?"unavailable":pressed?"1":"0");
    int first=boolean(s_firstperson,context->walker,&ok),blocked=boolean(s_menu_blocked,context->walker,&ok),typing=boolean(s_typing,0,&ok);
    s_note("sticker-native-first-person",first?"1":"0");s_note("sticker-native-menu-blocked",blocked?"1":"0");s_note("sticker-native-typing",typing?"1":"0");
    int equipped=equipment->is_equipped(state->key,context->walker);s_note("sticker-native-equipment",equipped?"1":"0");
}
static PTR sc_game_component(PTR go,const char *name,int *ok) {
    PTR type=dtype("UnityEngine",name);void *args[]={type};
    if(!type){*ok=0;return 0;}return call(sc.go_component,go,args,ok);
}
static PTR sc_component(PTR component,const char *name,int *ok) {
    PTR type=dtype("UnityEngine",name);void *args[]={type};
    if(!type){*ok=0;return 0;}return call(s_component,component,args,ok);
}
static PTR sc_find(const char *name,int *ok) {void *args[]={da.string(name)};return call(sc.find,0,args,ok);}
static int sc_set_position(PTR transform,DVec position) {
    int ok=1;void *args[]={&position};call(sc.set_position,transform,args,&ok);call(sc.sync,0,0,&ok);return ok;
}
static int sc_raycast(PTR camera,PTR *target,float *distance) {
    int ok=1;PTR transform=call(sc.component_transform,camera,0,&ok);
    DVec origin=dvector(sc.position,transform,&ok),direction=dvector(sc.forward,transform,&ok);
    float range=8;int mask=~(1<<2),trigger=1;_Alignas(16) unsigned char hit[128]={0};
    if(!ok)return -1;
    void *args[]={&origin,&direction,hit,&range,&mask,&trigger};PTR value=call(sc.ray,0,args,&ok);
    if(!ok||!value)return -1;
    if(!*(unsigned char *)unbox(value)){*target=0;*distance=range;return 0;}
    *target=call(sc.hit_transform,hit,0,&ok);*distance=dnumber(sc.hit_distance,hit,&ok);
    return ok&&*target?1:-1;
}
static int sc_target(TSState *state,const MoonTestContext *context,const char **reason) {
    int ok=1;PTR camera=0,field=get_field(object_class(context->walker),"viewCamera");
    if(field)field_value(context->walker,field,&camera);
    if(!s_live(camera))return MF_FAIL;
    if(state->scene_target){*reason="owned-surface-still-retained";return MF_FAIL;}
    DRoot camera_root=dhold(camera);if(!camera_root)return MF_FAIL;
    ddrop(&state->scene_camera);state->scene_camera=camera_root;
    PTR existing=0;float nearest=0;int hit=sc_raycast(camera,&existing,&nearest);
    if(hit<0)return MF_FAIL;
    if(hit&&!(nearest>1.2f&&nearest<=8.f)){*reason="requires-clear-private-target-space";return MF_BLOCKED;}
    float distance=hit&&nearest<6?nearest*.5f:3.f;
    PTR transform=call(sc.component_transform,camera,0,&ok);
    DVec origin=dvector(sc.position,transform,&ok),forward=dvector(sc.forward,transform,&ok);
    state->scene_center=(DVec){origin.x+forward.x*distance,origin.y+forward.y*distance,origin.z+forward.z*distance};
    int cube=3;void *create[]={&cube};PTR go=call(sc.primitive,0,create,&ok);
    if(!ok||!s_live(go))return MF_FAIL;
    state->scene_target=dhold(go);
    if(!state->scene_target){void *args[]={go};call(sc.destroy,0,args,&ok);return MF_FAIL;}
    void *name[]={da.string("MoonLab private sticker test surface")};call(sc.set_name,go,name,&ok);
    PTR target=call(sc.go_transform,go,0,&ok);state->scene_transform=dhold(target);
    DVec scale={.5f,.5f,.5f};void *size[]={&scale};call(sc.scale,target,size,&ok);
    if(!state->scene_transform||!ok||!sc_set_position(target,state->scene_center))return MF_FAIL;
    return MF_PENDING;
}
static int sc_count(TSState *state,int *ok) {return integer(sc.children,dget(state->scene_transform),ok);}
static PTR sc_child(TSState *state,int index,int *ok) {
    if(index<0||index>=sc_count(state,ok)){*ok=0;return 0;}
    void *args[]={&index};return call(sc.child,dget(state->scene_transform),args,ok);
}
static int sc_aimed(TSState *state) {
    PTR actual=0;float distance=0;int hit=sc_raycast(dget(state->scene_camera),&actual,&distance);
    return hit==1&&actual==dget(state->scene_transform)&&distance>0&&distance<8;
}
static SCVec2 sc_material_vector(PTR method,PTR material,PTR property,int *ok) {
    void *args[]={property};PTR value=call(method,material,args,ok);SCVec2 vector={0};
    if(value)dcopy(&vector,unbox(value),sizeof(vector));else *ok=0;return vector;
}
static int sc_close(float a,float b) {return a>=b-.001f&&a<=b+.001f;}
static int sc_sticker(TSState *state,PTR child,unsigned index) {
    int ok=1;PTR renderer=sc_component(child,"Renderer",&ok),collider=sc_component(child,"Collider",&ok);
    PTR material=renderer?call(sc.material,renderer,0,&ok):0;
    if(!ok||!s_live(material)||!s_live(collider))return 0;
    int enabled=boolean(sc.collider_enabled,collider,&ok);
    if(!ts->check("sticker-collider-disabled",ok&&!enabled,enabled,0))return 0;
    const char *properties[]={"_BaseMap","_MainTex"};DRoot property=0;
    for(unsigned i=0;i<2&&!property;i++){
        PTR candidate=da.string(properties[i]);void *args[]={candidate};PTR supported=call(sc.has_property,material,args,&ok);
        if(!ok||!supported)return 0;if(*(unsigned char *)unbox(supported))property=dhold(candidate);
    }
    if(!property)return 0;
    SCVec2 scale=sc_material_vector(sc.texture_scale,material,dget(property),&ok);
    SCVec2 offset=sc_material_vector(sc.texture_offset,material,dget(property),&ok);
    void *args[]={dget(property)};PTR texture=call(sc.texture,material,args,&ok);ddrop(&property);
    int grid=ok&&(sc_close(offset.x,0)||sc_close(offset.x,.5f))&&(sc_close(offset.y,0)||sc_close(offset.y,.5f));
    int design=grid?(sc_close(offset.y,.5f)?0:2)+(sc_close(offset.x,.5f)?1:0):-1;
    if(index==0)state->sticker_design=design;
    int expected=(state->sticker_design+(int)index)%4;
    int pass=ts->check("sticker-atlas-half-scale-x",ok&&sc_close(scale.x,.5f),scale.x>=-10&&scale.x<=10?(long long)(scale.x*1000):-1,500);
    pass&=ts->check("sticker-atlas-half-scale-y",ok&&sc_close(scale.y,.5f),scale.y>=-10&&scale.y<=10?(long long)(scale.y*1000):-1,500);
    pass&=ts->check("sticker-design-cycle-and-wrap",grid&&design==expected,design,expected);
    if(!state->scene_atlas&&s_live(texture))state->scene_atlas=dhold(texture);
    pass&=ts->check("sticker-real-shared-atlas",ok&&s_live(texture)&&texture==dget(state->scene_atlas),texture==dget(state->scene_atlas),1);
    return pass;
}
static void sc_destroy(PTR go,int *ok) {if(s_live(go)){void *args[]={go};call(sc.destroy,0,args,ok);}}
/* Only forget a surface after native destruction was observed. Provider-owned
 * stickers are removed through real controls; this only drops borrowed roots. */
static void sc_forget_destroyed_surface(TSState *state) {
    ddrop(&state->scene_first);ddrop(&state->scene_last);ddrop(&state->scene_previous);
    ddrop(&state->scene_transform);ddrop(&state->scene_target);
    state->sticker_count=0;
}
static int sc_cleanup(TSState *state) {
    int ok=1;
    if(state->scene_target)sc_destroy(dget(state->scene_target),&ok);
    /* An action remains owned across a destroyed-surface replacement. Losing
     * that surface root cannot prove provider entries were undone. */
    if((state->scene_target||state->sticker_action)&&!state->scene_complete) {
        s_note("cleanup","sticker-provider-rollback-unproven");ok=0;
    }
    ddrop(&state->scene_first);ddrop(&state->scene_last);ddrop(&state->scene_previous);ddrop(&state->scene_atlas);
    ddrop(&state->scene_model);ddrop(&state->scene_transform);ddrop(&state->scene_target);ddrop(&state->scene_camera);
    state->sticker_count=state->sticker_action=state->scene_complete=0;return ok;
}
