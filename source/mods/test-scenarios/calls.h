/* Scenario mistakes must become failed observations, not null argument-vector
 * faults inside IL2CPP. Typed bindings still own each argument's layout. */
static inline PTR s_checked_call(PTR method,PTR self,void **args,int *ok){
    if(!method||(da.arity(method)&&!args)){
        s_note("native-call-refused","missing-method-or-argument-vector");*ok=0;return 0;
    }
    return call(method,self,args,ok);
}
static inline int s_checked_boolean(PTR method,PTR self,int *ok){
    if(method&&da.arity(method)){
        s_note("native-call-refused","boolean-helper-requires-zero-parameters");*ok=0;return 0;
    }
    return boolean(method,self,ok);
}
static inline int s_checked_integer(PTR method,PTR self,int *ok){
    if(method&&da.arity(method)){
        s_note("native-call-refused","integer-helper-requires-zero-parameters");*ok=0;return 0;
    }
    return integer(method,self,ok);
}
#define call s_checked_call
#define boolean s_checked_boolean
#define integer s_checked_integer
