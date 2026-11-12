/* Additional owned source while a known bag is already full. Generic cleanup
 * stays zero-mass-only; these transitions prove the positive bag unchanged. */
static int cf_seed_retained(CFixture *fixture,PTR world,PTR bag,double carried,double kilograms) {
    if(fixture->uncertain||!fixture->mutated||!cf_scope(fixture,world)||!cf_original_valid(fixture)||
       !cf_bag_mass(fixture,world,bag,carried)||cf_empty(world)!=1)return 0;
    CFixture any={.id=-1};int next=0;
    if(cf_view(&any,world,0)!=0||!cf_next(world,&next)||next!=fixture->id+1)return 0;
    DRoot snapshot=cf_snapshot(world),record=0;
    PTR fresh_list=0,original_list=0,previous_list=0;int fresh_next=0,original_next=0,previous_next=0;
    if(!snapshot||dget(snapshot)==dget(fixture->original)||dget(snapshot)==dget(fixture->authored)||
       !cf_list(dget(snapshot),&fresh_list,&fresh_next)||
       !cf_list(dget(fixture->original),&original_list,&original_next)||
       !cf_list(dget(fixture->authored),&previous_list,&previous_next)||
       fresh_list==original_list||fresh_list==previous_list){ddrop(&snapshot);return 0;}
    if(snapshot)record=cf_payload(dget(snapshot),dget(fixture->actor),next,fixture->position,kilograms);
    if(!snapshot||!record){ddrop(&record);ddrop(&snapshot);return 0;}
    int still_next=0;
    if(!cf_scope(fixture,world)||!cf_original_valid(fixture)||!cf_bag_mass(fixture,world,bag,carried)||
       cf_empty(world)!=1||cf_view(&any,world,0)!=0||!cf_next(world,&still_next)||still_next!=next){
        ddrop(&record);ddrop(&snapshot);return 0;
    }
    CFixture expected=*fixture;expected.id=next;expected.kilograms=kilograms;expected.physical_pose=1;
    int ok=1;void *args[]={dget(snapshot)};call(cf.restore,world,args,&ok);
    int counts[4]={0};
    if(!ok||!cf_scope(fixture,world)||!cf_original_valid(fixture)||!cf_bag_mass(fixture,world,bag,carried)||
       !cf_counts(world,counts)||counts[0]!=1||counts[1]!=1||counts[2]!=0||counts[3]!=0||!cf_observe(&expected,world)){
        fixture->uncertain=1;ddrop(&record);ddrop(&snapshot);return 0;
    }
    ddrop(&fixture->view);ddrop(&fixture->record);ddrop(&fixture->authored);
    fixture->record=record;fixture->authored=snapshot;fixture->id=next;
    fixture->kilograms=kilograms;fixture->physical_pose=1;fixture->validated=0;
    return 1;
}
static int cf_retire_retained(CFixture *fixture,PTR world,PTR bag,double carried) {
    if(fixture->uncertain||!fixture->mutated||!cf_scope(fixture,world)||!cf_original_valid(fixture)||
       !cf_bag_mass(fixture,world,bag,carried)||!cf_observe(fixture,world))return 0;
    int counts[4]={0};CFixture any={.id=-1};
    int all=cf_view(&any,world,0),owned=cf_view(fixture,world,0);
    if(!cf_counts(world,counts)||counts[0]!=1||counts[1]!=1||counts[2]!=0||counts[3]!=0||
       all<0||owned<0||all!=owned||owned>1)return 0;
    int ok=1;void *args[]={&fixture->id};call(cf.remove,world,args,&ok);
    int next=0;
    if(!ok||!cf_scope(fixture,world)||!cf_original_valid(fixture)||!cf_bag_mass(fixture,world,bag,carried)||
       cf_empty(world)!=1||!cf_next(world,&next)||next!=fixture->id+1||!cf_refresh(world)||cf_view(&any,world,0)!=0){
        fixture->uncertain=1;return 0;
    }
    ddrop(&fixture->view);return 1;
}
