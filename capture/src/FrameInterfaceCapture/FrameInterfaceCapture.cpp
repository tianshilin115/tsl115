
#include "FrameInterfaceCapture.h"
#include "xdpi_conf.h"

CaptureCatchThread *captureOfflineThread;
int threadNum = 1;
//void CaptureCatchPlugin::Start(__rte_unused IPipe *inpipe, IPipe *outpipe) {
//    capture_thread->init();
//    capture_thread->start(outpipe);
//}

void CaptureCatchPlugin::Start(int idx, vector<IPipe*>* inpipegroup, vector<IPipe*>* outpipegroup) {
    if(GlobalConfig.captureCfg.runMode == RUN_MODLE_OFFLINE){
        captureOfflineThread = new CaptureCatchThread();
        captureOfflineThread->start(idx, threadNum, outpipegroup);
    } else{
        captureThread = new CaptureCatchThread();
        captureThread->start(idx, threadNum, outpipegroup);
    }
}

bool CaptureCatchPlugin::Init() {
    return true;
}

/**
 *  初始化插件模块
 * @param path
 * @return
 */
int plugin_module_init(int count) {
    threadNum = count;
//    if (GlobalConfig.captureCfg.runMode != RUN_MODLE_OFFLINE)
    Network network;  // 在生成Network类对象是，调用构造函数，完成相应的初始化；
    if(GlobalConfig.captureCfg.runMode == RUN_MODLE_OFFLINE && count!=1){
        rte_log(RTE_LOG_ERR, RTE_LOGTYPE_USER1, "在离线模式下运行采集线程数暂时只支持1个，当前线程:%d\n", count);
        return -1;
    }
    return 0;
}

/**
 * 创建插件实例
 * @param socket socket是什么意思呢？
 * @return
 */
IPlugin *plugin_create_ins(unsigned socket) {
    return new CaptureCatchPlugin(); // 调用了采集模块的核心
}


/**
 * use test
 */
//
//IPipe * captureOutput;
//IPipe *captureinpipe;
//IPlugin *capturePlugin;
//
//int captureRun(void *arg) {
//    capturePlugin->Start(captureinpipe, captureOutput);
//}
//
//void runCaptureFrameInterface(IPipe *inpipe, IPipe * output,unsigned int lcoreId) {
//    captureOutput = output;
//    captureinpipe = inpipe;
//    plugin_module_init("/root/DPDK/CAPTUREDEBUG/config");
//    capturePlugin = plugin_create_ins(0);
//    capturePlugin->Init();
//    rte_eal_remote_launch(captureRun, nullptr, lcoreId);
//}
