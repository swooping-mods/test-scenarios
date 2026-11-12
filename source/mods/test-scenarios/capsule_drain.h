/* One native drain invocation, with independently observed pre/post state.
 * 0 permits a bounded later retry only after a proven no-change refusal;
 * -1 stops, and an invoked operation with uncertain progress is quarantined. */
static int cf_pack_drain(CFixture *fixture,PTR world,PTR bag) {
    if(fixture->uncertain||!fixture->mutated||!cf_scope(fixture,world)||
       !cf_original_valid(fixture)||!cf_bag_mass(fixture,world,bag,fixture->kilograms)||cf_empty(world)!=1)return -1;
    CFixture any={.id=-1};int before_next=0;
    if(cf_view(&any,world,0)!=0||!cf_next(world,&before_next)||before_next!=fixture->id+1)return -1;
    int ok=1,packed=0;DVec point=fixture->position,up={0,1,0};double maximum=fixture->kilograms;
    void *args[]={dget(fixture->actor),&point,&up,&maximum,&packed};
    PTR result=call(cf.pack,world,args,&ok);
    if(!ok||!result){fixture->uncertain=1;return -1;}
    if(!*(unsigned char *)unbox(result)){
        int after_next=0;
        if(packed!=0||!cf_scope(fixture,world)||!cf_original_valid(fixture)||
           !cf_bag_mass(fixture,world,bag,maximum)||cf_empty(world)!=1||
           cf_view(&any,world,0)!=0||!cf_next(world,&after_next)||after_next!=before_next){
            fixture->uncertain=1;return -1;
        }
        return 0;
    }
    /* Do not infer success from the returned bool or out-ID. Read the new
     * native snapshot and both real mass getters before accepting its identity. */
    CFixture expected=*fixture;expected.id=packed;expected.physical_pose=0;
    int counts[4]={0};
    if(packed!=before_next||!cf_scope(fixture,world)||!cf_original_valid(fixture)||
       !cf_bag_mass(fixture,world,bag,0)||!cf_counts(world,counts)||counts[0]!=1||
       counts[1]!=1||counts[2]!=0||counts[3]!=0||!cf_observe(&expected,world)){
        fixture->uncertain=1;return -1;
    }
    ddrop(&fixture->view);fixture->id=packed;fixture->physical_pose=0;
    return 1;
}
