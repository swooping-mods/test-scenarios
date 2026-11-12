/* Poll the actual returned Task. A successful native call is not a completed
 * save, and a lost root/exception never permits another save invocation. */
static struct {PTR save,completed,faulted,canceled;int ready;} st;
static int st_bind(void) {
    st.save=dmethod("MoonCleaners","MoonWorld","SaveProgress","");
    st.completed=dmethod("System.Threading.Tasks","Task","get_IsCompleted","");
    st.faulted=dmethod("System.Threading.Tasks","Task","get_IsFaulted","");
    st.canceled=dmethod("System.Threading.Tasks","Task","get_IsCanceled","");
    st.ready=st.save&&st.completed&&st.faulted&&st.canceled&&
        method_token(st.save)==100665560&&method_token(st.completed)==100667950&&
        method_token(st.faulted)==100667962&&method_token(st.canceled)==100667944;
    return st.ready;
}
static int st_start(STask *save,PTR world) {
    if(!st.ready||save->started||save->task||save->uncertain||!world)return 0;
    save->started=1;int ok=1;PTR task=call(st.save,world,0,&ok);
    if(ok&&task)save->task=dhold(task);
    if(!ok||!save->task){save->uncertain=1;return 0;}
    return 1;
}
static int st_flag(PTR method,PTR task,int *ok) {
    if(!*ok)return 0;
    PTR value=call(method,task,0,ok);
    if(!*ok||!value){*ok=0;return 0;}
    return *(unsigned char *)unbox(value)!=0;
}
static int st_poll(STask *save) {
    if(!save->started||!save->task||save->uncertain)return -1;
    int ok=1;PTR task=dget(save->task);
    int complete=st_flag(st.completed,task,&ok);
    int fault=st_flag(st.faulted,task,&ok),canceled=st_flag(st.canceled,task,&ok);
    if(!ok||fault||canceled){save->uncertain=1;save->complete=0;return -1;}
    if(save->complete&&!complete){save->uncertain=1;save->complete=0;return -1;}
    save->complete=complete;return complete;
}
