/* Test-only native capsule producer. Snapshot restore is admitted only after
 * proving the original capsule dictionaries empty; cleanup never erases foreign
 * records. No inventory balance, developer gate or provider callback is edited. */
static struct {
    PTR snapshot_class,record_class,snapshot_ctor,record_ctor,snapshot,restore,remove;
    PTR find,view_world,view_id,refresh,mass,container_mass,pack,local_player,loaded,full;
    void (*set)(PTR,PTR,void *),(*barrier)(PTR,PTR *,PTR);
    int (*offset)(PTR);
    int ready;
} cf;
static int cf_view(CFixture *fixture,PTR world,int retain);
static int cf_bind(PTR game) {
    cf.snapshot_class=find_class("MoonCleaners","MoonLooseDustSnapshot");
    cf.record_class=find_class("MoonCleaners","MoonCanisterSnapshot");
    cf.snapshot_ctor=dmethod("MoonCleaners","MoonLooseDustSnapshot",".ctor","");
    cf.record_ctor=dmethod("MoonCleaners","MoonCanisterSnapshot",".ctor","");
    cf.snapshot=dmethod("MoonCleaners","MoonWorld","SnapshotCanisters","MoonCleaners.MoonLooseDustSnapshot");
    cf.restore=dmethod("MoonCleaners","MoonWorld","RestoreCanisters","MoonCleaners.MoonLooseDustSnapshot");
    cf.remove=dmethod("MoonCleaners","MoonWorld","RemoveCanister","System.Int32");
    cf.find=dmethod("UnityEngine","Object","FindObjectsByType","System.Type;UnityEngine.FindObjectsInactive;UnityEngine.FindObjectsSortMode");
    cf.view_world=dmethod("MoonCleaners","MoonDustCanisterView","get_World","");
    cf.view_id=dmethod("MoonCleaners","MoonDustCanisterView","get_Id","");
    cf.refresh=dmethod("MoonCleaners","MoonCanisterStreaming","Refresh","");
    cf.mass=dmethod("MoonCleaners","MoonWorld","InventoryKilograms","System.String");
    cf.container_mass=dmethod("MoonCleaners","MoonWorld","ContainerKilograms","System.String;System.String");
    cf.pack=dmethod("MoonCleaners","MoonWorld","PackDustPortion","System.String;UnityEngine.Vector3;UnityEngine.Vector3;System.Double;System.Int32&");
    cf.full=dmethod("MoonCleaners","MoonWorld","InventoryFull","System.String");
    cf.loaded=dmethod("MoonCleaners","MoonWorld","LoadedContainerId","System.String");
    cf.local_player=dmethod("MoonCleaners","MoonCrew","get_LocalPlayerId","");
    cf.set=(void(*)(PTR,PTR,void *))GetProcAddress(game,"il2cpp_field_set_value");
    cf.barrier=(void(*)(PTR,PTR *,PTR))GetProcAddress(game,"il2cpp_gc_wbarrier_set_field");
    cf.offset=(int(*)(PTR))GetProcAddress(game,"il2cpp_field_get_offset");
    cf.ready=cf.snapshot_class&&cf.record_class&&cf.snapshot_ctor&&cf.record_ctor&&cf.snapshot&&cf.restore&&cf.remove&&cf.find&&cf.view_world&&cf.view_id&&cf.refresh&&cf.mass&&cf.container_mass&&cf.pack&&cf.local_player&&cf.loaded&&cf.full&&cf.set&&cf.barrier&&cf.offset;
    if(cf.ready)cf.ready=method_token(cf.snapshot_ctor)==100668995&&method_token(cf.record_ctor)==100668993&&
        method_token(cf.snapshot)==100664767&&method_token(cf.restore)==100664768&&
        method_token(cf.remove)==100664617&&method_token(cf.refresh)==100664038&&
        method_token(cf.view_world)==100666612&&method_token(cf.view_id)==100666614&&
        method_token(cf.mass)==100665262&&method_token(cf.container_mass)==100665682&&
        method_token(cf.pack)==100664764&&method_token(cf.local_player)==100666068&&method_token(cf.loaded)==100665673&&method_token(cf.full)==100665264;
    return cf.ready;
}
static int cf_scope(const CFixture *fixture,PTR world) {
    if(!world||world!=dget(fixture->world)||!s_live(world)||!fixture->actor)return 0;
    int ok=1;PTR player=call(cf.local_player,0,0,&ok);
    return ok&&player&&s_same_string(player,dget(fixture->actor));
}
static double cf_mass(PTR world,PTR actor,int *ok) {
    void *args[]={actor};PTR value=call(cf.mass,world,args,ok);
    if(!value){*ok=0;return 0;}return *(double *)unbox(value);
}
static int cf_mass_empty(const CFixture *fixture,PTR world) {
    int ok=1;double kg=cf_mass(world,dget(fixture->actor),&ok);
    return ok&&kg>=0&&kg<=1e-9;
}
static int cf_read(PTR object,const char *name,void *value) {
    PTR field=object?get_field(object_class(object),name):0;
    if(!field)return 0;
    field_value(object,field,value);return 1;
}
static PTR cf_object(PTR object,const char *name,int *ok) {
    PTR value=0;if(!*ok||!cf_read(object,name,&value)){*ok=0;return 0;}return value;
}
static int cf_scalar(PTR object,const char *name,void *value) {
    PTR field=object?get_field(object_class(object),name):0;
    if(!field)return 0;
    cf.set(object,field,value);return 1;
}
static int cf_reference(PTR object,const char *name,PTR value) {
    PTR field=object?get_field(object_class(object),name):0;
    int offset=field?cf.offset(field):0;
    if(offset<16||offset>512||(offset&7))return 0;
    cf.barrier(object,(PTR *)((unsigned char *)object+offset),value);return 1;
}
static int cf_count(PTR object,int *ok) {
    if(!*ok||!object){*ok=0;return 0;}
    PTR method=get_method(object_class(object),"get_Count",0);
    PTR value=method?call(method,object,0,ok):0;
    if(!*ok||!value){*ok=0;return 0;}
    int count=*(int *)unbox(value);if(count<0){*ok=0;return 0;}return count;
}
static DRoot cf_new(PTR klass,PTR constructor) {
    DRoot root=dhold(da.new_object(klass));if(!root)return 0;
    int ok=1;call(constructor,dget(root),0,&ok);
    if(!ok)ddrop(&root);
    return root;
}
static DRoot cf_snapshot(PTR world) {
    DRoot root=cf_new(cf.snapshot_class,cf.snapshot_ctor);if(!root)return 0;
    int ok=1;void *args[]={dget(root)};call(cf.snapshot,world,args,&ok);
    if(!ok)ddrop(&root);
    return root;
}
static int cf_counts(PTR world,int counts[4]) {
    static const char *names[]={"canisters","canisterSectors","carriedCanisters","flyingCanisters"};
    for(unsigned i=0;i<4;i++){
        int ok=1;PTR collection=cf_object(world,names[i],&ok);counts[i]=cf_count(collection,&ok);
        if(!ok)return 0;
    }
    return 1;
}
/* -1 is unknown; read all four collections even after a confirmed nonempty one.
 * An unknown later dictionary must never admit a mutation/cleanup transition. */
static int cf_empty(PTR world) {
    int counts[4];if(!cf_counts(world,counts))return -1;
    return counts[0]==0&&counts[1]==0&&counts[2]==0&&counts[3]==0;
}
static int cf_list(PTR snapshot,PTR *list,int *next) {
    int ok=1;*list=cf_object(snapshot,"canisters",&ok);
    if(!ok||!cf_read(snapshot,"nextCanisterId",next)||*next<1||*next>=1000000000)return 0;
    return 1;
}
static int cf_original_valid(const CFixture *fixture) {
    PTR list=0;int next=0,ok=1;
    return fixture->original&&cf_list(dget(fixture->original),&list,&next)&&
        cf_count(list,&ok)==0&&ok&&next==fixture->original_next;
}
static int cf_next(PTR world,int *next) {
    DRoot snapshot=cf_snapshot(world);PTR list=0;int ok=1;
    int pass=snapshot&&cf_list(dget(snapshot),&list,next)&&cf_count(list,&ok)==0&&ok;
    ddrop(&snapshot);return pass;
}
static int cf_bag_mass(const CFixture *fixture,PTR world,PTR bag,double expected) {
    if(!bag||!cf_scope(fixture,world))return 0;
    int ok=1;void *owner[]={dget(fixture->actor)};PTR loaded=call(cf.loaded,world,owner,&ok);
    if(!ok||!loaded||!s_same_string(loaded,bag))return 0;
    double inventory=cf_mass(world,dget(fixture->actor),&ok);if(!ok)return 0;
    void *args[]={dget(fixture->actor),bag};PTR value=call(cf.container_mass,world,args,&ok);
    if(!ok||!value)return 0;
    double stored=*(double *)unbox(value);
    return inventory>=expected-1e-9&&inventory<=expected+1e-9&&stored>=expected-1e-9&&stored<=expected+1e-9;
}
static void cf_drop(CFixture *fixture) {
    ddrop(&fixture->view);ddrop(&fixture->record);ddrop(&fixture->authored);
    ddrop(&fixture->original);ddrop(&fixture->actor);ddrop(&fixture->world);
}
static DRoot cf_payload(PTR snapshot,PTR actor,int id,DVec position,double kilograms) {
    if(!(kilograms>0&&kilograms<=1000)||id<1||id>=999999999)return 0;
    int ok=1,next=0;PTR list=0;
    if(!cf_list(snapshot,&list,&next)||next!=id||cf_count(list,&ok)!=0||!ok)return 0;
    DRoot root=cf_new(cf.record_class,cf.record_ctor);if(!root)return 0;
    PTR record=dget(root);float zero=0,one=1;unsigned char physical=1;next=id+1;
    int pass=cf_scalar(record,"id",&id)&&cf_reference(record,"owner",actor)&&
        cf_scalar(record,"kilograms",&kilograms)&&cf_scalar(record,"capacity",&kilograms)&&
        cf_scalar(record,"x",&position.x)&&cf_scalar(record,"y",&position.y)&&cf_scalar(record,"z",&position.z)&&
        cf_scalar(record,"ux",&zero)&&cf_scalar(record,"uy",&one)&&cf_scalar(record,"uz",&zero)&&
        cf_scalar(record,"physicalPose",&physical)&&cf_scalar(record,"qx",&zero)&&
        cf_scalar(record,"qy",&zero)&&cf_scalar(record,"qz",&zero)&&cf_scalar(record,"qw",&one)&&
        cf_scalar(snapshot,"nextCanisterId",&next);
    PTR add=pass?get_method(object_class(list),"Add",1):0;
    if(!add)pass=0;
    if(pass){void *args[]={record};call(add,list,args,&ok);pass=ok&&cf_count(list,&ok)==1&&ok;}
    if(!pass)ddrop(&root);
    return root;
}
/* 1 prepared, -1 unsupported/precondition, 0 error. No world write occurs before
 * mutated is set, including native exceptions after a partial Restore. */
static int cf_prepare(CFixture *fixture,PTR world,PTR actor,DVec position,double kilograms) {
    if(!cf.ready||!world||!s_live(world)||!actor||fixture->uncertain||fixture->mutated||fixture->world||!(kilograms>0&&kilograms<=1000))return 0;
    if(!(position.x>=-1e6f&&position.x<=1e6f&&position.y>=-1e6f&&position.y<=1e6f&&position.z>=-1e6f&&position.z<=1e6f))return 0;
    int empty=cf_empty(world);if(empty!=1)return empty<0?0:-1;
    CFixture any={.id=-1};int views=cf_view(&any,world,0);
    if(views)return views<0?0:-1;
    fixture->validated=0;fixture->physical_pose=1;
    fixture->world=dhold(world);fixture->actor=dhold(actor);
    if(!fixture->world||!fixture->actor||!cf_scope(fixture,world))return 0;
    int mass_ok=1;double before=cf_mass(world,actor,&mass_ok);
    if(!mass_ok)return 0;
    if(!(before>=0&&before<=1e-9))return -1;
    fixture->original=cf_snapshot(world);fixture->authored=cf_snapshot(world);
    PTR original=0,authored=0;int original_next=0,next=0,ok=1;
    if(!fixture->original||!fixture->authored||dget(fixture->original)==dget(fixture->authored)||
       !cf_list(dget(fixture->original),&original,&original_next)||
       !cf_list(dget(fixture->authored),&authored,&next)||original==authored||next!=original_next)return 0;
    if(cf_count(original,&ok)||!ok)return 0;
    if(cf_count(authored,&ok)||!ok)return 0;
    fixture->original_next=fixture->id=next;fixture->position=position;fixture->kilograms=kilograms;
    fixture->record=cf_payload(dget(fixture->authored),actor,next,position,kilograms);
    if(!fixture->record)return 0;
    PTR untouched=0;int untouched_next=0;
    if(!cf_list(dget(fixture->original),&untouched,&untouched_next)||untouched!=original||
       cf_count(untouched,&ok)!=0||!ok||untouched_next!=fixture->original_next||cf_empty(world)!=1)return 0;
    fixture->mutated=1;
    void *args[]={dget(fixture->authored)};call(cf.restore,world,args,&ok);return ok;
}
static int cf_observe(CFixture *fixture,PTR world) {
    DRoot snapshot=cf_snapshot(world);if(!snapshot)return 0;
    PTR list=0;int next=0,ok=1,pass=cf_list(dget(snapshot),&list,&next);
    if(pass)pass=cf_count(list,&ok)==1&&ok&&next==fixture->id+1;
    PTR item=pass?get_method(object_class(list),"get_Item",1):0;int index=0;
    void *args[]={&index};PTR record=item?call(item,list,args,&ok):0;
    int id=0;PTR owner=0;double kg=0;DVec position={0};unsigned char physical=0;
    if(!ok||!record||!cf_read(record,"id",&id)||!cf_read(record,"owner",&owner)||
       !cf_read(record,"kilograms",&kg)||!cf_read(record,"physicalPose",&physical)||
       !cf_read(record,"x",&position.x)||!cf_read(record,"y",&position.y)||!cf_read(record,"z",&position.z))pass=0;
    pass=pass&&id==fixture->id&&s_same_string(owner,dget(fixture->actor))&&physical==(unsigned char)fixture->physical_pose&&
        kg>=fixture->kilograms-1e-9&&kg<=fixture->kilograms+1e-9&&
        position.x>=fixture->position.x-.01f&&position.x<=fixture->position.x+.01f&&
        position.y>=fixture->position.y-.01f&&position.y<=fixture->position.y+.01f&&
        position.z>=fixture->position.z-.01f&&position.z<=fixture->position.z+.01f;
    ddrop(&snapshot);return pass;
}
static DRoot cf_find(const char *name,int *ok) {
    PTR type=dtype("MoonCleaners",name);int inactive=0,sort=0;
    if(!type){*ok=0;return 0;}void *args[]={type,&inactive,&sort};
    DRoot root=dhold(call(cf.find,0,args,ok));if(!root)*ok=0;
    if(!*ok)ddrop(&root);
    return root;
}
/* Active views only. A pooled, inactive view remains a live Unity object. */
static int cf_view(CFixture *fixture,PTR world,int retain) {
    int ok=1,matches=0;DRoot array=cf_find("MoonDustCanisterView",&ok);
    if(!ok)return -1;
    U64 count=da.array_length(dget(array));if(count>512){ddrop(&array);return -1;}
    for(U64 i=0;i<count&&ok;i++){
        PTR view=*(PTR *)darray(dget(array),i);if(!s_live(view))continue;
        PTR owner=call(cf.view_world,view,0,&ok);if(!ok)break;if(owner!=world)continue;
        PTR value=call(cf.view_id,view,0,&ok);if(!ok||!value){ok=0;break;}
        if(fixture->id>=0&&*(int *)unbox(value)!=fixture->id)continue;
        matches++;
        if(retain&&!fixture->view){fixture->view=dhold(view);if(!fixture->view)ok=0;}
    }
    ddrop(&array);return ok?matches:-1;
}
static int cf_refresh(PTR world) {
    int ok=1,found=0;DRoot array=cf_find("MoonCanisterStreaming",&ok);if(!ok)return 0;
    U64 count=da.array_length(dget(array));if(count>16){ddrop(&array);return 0;}
    for(U64 i=0;i<count&&ok;i++){
        PTR streaming=*(PTR *)darray(dget(array),i);if(!s_live(streaming))continue;
        PTR owner=cf_object(streaming,"world",&ok);if(!ok)break;if(owner!=world)continue;
        call(cf.refresh,streaming,0,&ok);found++;
    }
    ddrop(&array);return ok&&found;
}
static int cf_cleanup(CFixture *fixture,PTR world) {
    int pass=1;
    if(fixture->mutated){
        pass=!fixture->uncertain&&cf_scope(fixture,world)&&fixture->original&&cf_mass_empty(fixture,world);
        if(pass)pass=cf_original_valid(fixture);
        int counts[4]={0},empty=-1;
        if(pass){
            pass=cf_counts(world,counts);
            if(pass)empty=counts[0]==0&&counts[1]==0&&counts[2]==0&&counts[3]==0;
            if(pass&&!empty)pass=counts[0]==1&&counts[1]<=1&&counts[2]==0&&counts[3]==0;
        }
        if(pass){
            CFixture any={.id=-1};int all=cf_view(&any,world,0),owned=cf_view(fixture,world,0);
            pass=all>=0&&owned>=0&&all==owned&&owned<=1;
        }
        /* A partial restore may have left an empty world. Otherwise only the
         * exact authored record may be removed, never an unknown occupant. */
        if(pass&&empty==0){
            pass=cf_observe(fixture,world);
            if(pass){int ok=1;void *args[]={&fixture->id};call(cf.remove,world,args,&ok);pass=ok;}
        }else if(empty!=1)pass=0;
        if(pass)pass=cf_empty(world)==1;
        if(pass){int ok=1;void *args[]={dget(fixture->original)};call(cf.restore,world,args,&ok);pass=ok;}
        if(pass){
            CFixture any={.id=-1};pass=cf_empty(world)==1&&cf_refresh(world)&&cf_view(&any,world,0)==0;
        }
        if(pass){
            DRoot restored=cf_snapshot(world);PTR list=0;int next=0,ok=1;
            pass=restored&&cf_list(dget(restored),&list,&next)&&cf_count(list,&ok)==0&&ok&&next==fixture->original_next;
            ddrop(&restored);
        }
        if(pass)pass=cf_mass_empty(fixture,world);
        if(pass)fixture->mutated=0;
    }
    cf_drop(fixture);return pass;
}
