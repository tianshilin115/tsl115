#include <cstdio>
#include <unistd.h>
#include <cstring>
#include <string>
#include "Init/Network/Network.h"
#include "CaptureThread/CaptureCatchThread.h"
#include <vector>
#include <atomic>
#include <generic/rte_rwlock.h>
#include "xdpi.h"
using std::string;
using std::atomic;
using std::vector;

/**
 * capture
 */
class CaptureCatchPlugin : public IPlugin {
public:
    bool Init() ;

//    void Start(IPipe *inpipe, IPipe *outpipe);

    void Start(int idx, vector<IPipe*>* inpipegroup, vector<IPipe*>* outpipegroup);

    void Stop() {} // 没有进行实现

    const char *GetName() { return "capture"; }  // 继承了IPlugin的函数，反馈该模块名称

    CaptureCatchPlugin() {
        _flag.store(false);
    }
//
//    ~CaptureCatchPlugin();

private:
    CaptureCatchThread *captureThread;  // 核心的处理函数，在系统start时，对其进行实例化
    atomic<bool> _flag; // 是一个标识？
};

#ifdef __cplusplus
extern "C" {
#endif
int plugin_module_init(int count);
void plugin_module_uninit();
IPlugin *plugin_create_ins(unsigned socket);
void plugin_del_ins(IPlugin *ins);


#ifdef __cplusplus
}
#endif


