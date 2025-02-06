#define logbuf(b,s) logbuf_impl(b,s,__FILE__,__LINE__,__FUNCTION__)
#define trace(x...) {trace_impl_pre(__FILE__,__LINE__,__FUNCTION__);fprintf(stderr,x);trace_impl_post(__FILE__,__LINE__,__FUNCTION__);}
#define crash(x...) {crash_impl_pre(__FILE__,__LINE__,__FUNCTION__);fprintf(stderr,"Aborted at line %s:%d\n",__FILE__,__LINE__);crash_impl_post(__FILE__,__LINE__,__FUNCTION__);;}



void logbuf_impl(void *buf, int size, const char *file, int line, const char *func);
void trace_impl_pre(const char *file, int line, const char *func);
void trace_impl_post(const char *file, int line, const char *func);
void crash_impl_pre(const char *file, int line, const char *func);
void crash_impl_post(const char *file, int line, const char *func);
