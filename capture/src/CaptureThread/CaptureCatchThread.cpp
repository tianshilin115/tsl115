/**
 * Created by root on 19-4-12.
 * 描述：数据包采集线程
 * Copyright (c) 2013 - 2019 成都安舟. All rights reserved.
 */


#include "CaptureCatchThread.h"
#include "HandleOffline.h"
#include <rte_mbuf.h>
#include <time.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include "xdpi_conf.h"


static uint64_t last_total_packet = 0; // 最新的采集数据包数量
static uint64_t last_total_bytes = 0; // 最新的采集字节数量
static uint64_t bytes_persec = 0; // 每秒字节数量
static uint64_t packet_persec = 0;// 每秒数据包个数
int event_time_scope = 10; // 什么意思，时间响应范围？

rte_mempool *membufpoolTest;
rte_mempool *packetmempollTest;
thread_local uint64_t errInputQueue = 0;
thread_local uint64_t errPacketMembuff = 0;
thread_local uint64_t txPackets = 0;
//long testQueue[12]={0};
//  采集状态包括发送数据包个数和接收数据包个数
void capture_status1() {
//    for (int i = 0; i < 9; ++i) {
//        printf("队列 %d - %lu\n",i, testQueue[i]);
//    }
    printf("队列错误状态：\n入数据包会话重组队列失败数量：%lu\n", errInputQueue);
//    printf("队列错误状态：\n入数据包会话重组队列失败数量：%lu\npacket内存池取不出来数量:%lu\n", errInputQueue, errPacketMembuff);
    printf("membuf内存池状态：\n%s:%lu\n", membufpoolTest->name, rte_mempool_in_use_count(membufpoolTest));
    txPackets = 0;
    for ( uint16_t portid:GlobalConfig.portCfg.txPorts){
        rte_eth_stats txStats;
        int status = rte_eth_stats_get(portid, &txStats);
        txPackets += txStats.opackets;
    }
    for ( uint16_t portid:GlobalConfig.portCfg.rxPorts){
        rte_eth_stats stats;
        /* code */
        int status = rte_eth_stats_get(portid, &stats);
        packet_persec = (stats.ipackets - last_total_packet) / event_time_scope;
        bytes_persec = (stats.ibytes - last_total_bytes) / event_time_scope;
        last_total_packet = stats.ipackets;
        last_total_bytes = stats.ibytes;
        printf("端口%d状态:\n", portid);
        if (status != 0) {
            printf("获取状态错误");
        } else {
            printf("采集状态：\n数据包总数:%lu\n总流量:%lu\n总丢包数:%lu\n错误包:%lu\n流量：%lu, \n数据包/s:%lu\n发包数量：(网卡统计%lu)\n\n\n\n\n", stats.ipackets,
                   stats.ibytes,
                   stats.imissed,
                   stats.ierrors, bytes_persec, packet_persec,txPackets);
        }
    }
}

extern CaptureCatchThread *captureOfflineThread; // 离线处理的全局变量，可能被其他模块调用

uint64_t hz;
// 离线数据包处理包的头和包的长度不匹配时，报错，认为为错误包
void offlinePacketHandle(u_char *argument, const struct pcap_pkthdr *pktHeader, const u_char *packetContent) {
    if (pktHeader->len != pktHeader->caplen)
        rte_log(RTE_LOG_ERR, RTE_LOGTYPE_USER1, "数据包长度与捕获长度不匹配，错误包跳过不处理");
    captureOfflineThread->handlePackets(packetContent, pktHeader);
}

/**
 * 析构函数 为空，说明，初始化实例时，没有进行任何操作，需要调用实例启动，因此其需要被调用操作
 */
CaptureCatchThread::CaptureCatchThread() {

}


/**
 * 解构函数
 */
CaptureCatchThread::~CaptureCatchThread() {}


/**
 * 初始化抓包线程，对线程绑定的CPU信息和采集状态信息进行实现，初始化队列和环形队列，idx什么参数？
 * 其中初始化的队列和环形队列，存储在实例对象的参数中
 */
void CaptureCatchThread::init(int idx, int threadNum) {
    lcore_id = rte_lcore_id();// 函数返回执行程序所在的CPU处理器ID

    socket_id = rte_socket_id();// 在dpdksocket代表物理CPU，二lcore代表逻辑核书

    capture_signal = CAPTURE_SIGNAL_RUN; // 此处为赋值，其采用传值的方式标识采集的开始，可能没有采用所谓信号机制

//    packetMempoolInit();

    hz = rte_get_tsc_hz();// CPU周期数

    initQueue(idx, threadNum); // 调用初始化队列函数

    txPortNum = GlobalConfig.portCfg.txPorts.size();

    if (GlobalConfig.captureCfg.runMode != RUN_MODLE_OFFLINE)
        initRings(idx, threadNum);
};

// 根据信号，来对采集状态信息进行呈现
static void signhandle_show(int sig) {
    capture_status1();
    signal(SIGTSTP, signhandle_show);
}

/**
 *开始捕获 需要指明任务idx、运行线程数和输出管道数组，程序首先进行运行初始化比如队列和管道，展示网卡开始状态信息如启动CPU核心、环形队列范围、写入队列范围
 *最终根据配置中的模块，启动离线和在线处理函数 
 *  @return
 */
int CaptureCatchThread::start(int idx, int threadNum, vector<IPipe *> *output) {
    outputIPipe = output;
    init(idx, threadNum);
    idxTest = idx;
    if (signal(SIGTSTP, signhandle_show) == SIG_ERR)
        return -1;

    printf("启动CPU核心 %d 执行采集插件,处理队列：%d - %d,写入队列：%d - %d\n", lcore_id, startRxRingNum, endRxRingNum, startQueueNum,
           endQueueNum);/
    if (GlobalConfig.captureCfg.runMode == RUN_MODLE_OFFLINE) {
        	startCaptureOffline();
    } else {
        startCaptureOnline();
    }
}

/**
 * 离线解析 首先命名mem池名称，有mbuf_pool_和CPUID+两个空字符构成，之后调用dpdk相关函数根据配置文件中mbufSize参数设置
 * 创建内存池用于存储抓取的数据包，最终离线处理函数为handlePcapFiles（HandleOffline类函数）
 * 
 */
void CaptureCatchThread::startCaptureOffline() {
	string mem_pool_name = "mbuf_pool_" + to_string(socket_id) + to_string(0) + to_string(0);
    offlineMembufPool = rte_pktmbuf_pool_create(mem_pool_name.c_str(), GlobalConfig.captureCfg.mbufSize,  
		MBUF_CACHE_SIZE,       0, RTE_MBUF_DEFAULT_BUF_SIZE, socket_id);
    if (offlineMembufPool == nullptr)
        rte_exit(EXIT_FAILURE, "创建内存池失败:%s,mbuf_pool_%d_%d_%d\n", rte_strerror(rte_errno), socket_id, 0, 0);
    rte_pktmbuf_alloc_bulk(offlineMembufPool, bufs, MAX_PACKET_GET_BULK);
    handleOffline = new HandleOffline();
//    do {
//改为轮询任务
    while(true){

        haveListeningtask = false;
        handleTasks(handleOffline->getOfflineTask(WAIT_FOR_ANALYSIS_PACPFILE_INDEX, R"({"must":[{"term":{"status":1}}]})"),WAIT_FOR_ANALYSIS_PACPFILE_INDEX);
        handleTasks(handleOffline->getOfflineTask(SERVER_MEDIUM_MSG_INDEX, R"({"must":[{"term":{"status":1}}]})"), SERVER_MEDIUM_MSG_INDEX);
        handleTasks(handleOffline->getOfflineTask(NET_EQUIPMENT_INDEX, R"({"should":[{"term":{"status":1}}, {"term":{"listenState":1}}]})"),NET_EQUIPMENT_INDEX);
//        handleListenTask(handleOffline->getOfflineTask(NET_EQUIPMENT_INDEX, R"({"term":{"listenState":1}})"));
        rte_delay_us_sleep(60*1000000);
    } 
    // while (haveListeningtask);

    captureStop();
}

/**
 * 处理监听任务，每次处理更新文件
 * @param offlineTasks
 */
void CaptureCatchThread::handleListenTask(OfflineTask offlineTask, string updateIndex) {
    list<string> handledTaskFiles{};
    map<string, list<string>>::iterator iter = listenTask.find(offlineTask.taskId);
    if(iter != listenTask.end())
        handledTaskFiles = iter->second;

    list<string> filesNeedHandle = getFilesNeedHandle(handledTaskFiles, offlineTask.files);
    offlineTask.allFileNum = offlineTask.files.size();
    listenTask[offlineTask.taskId] = offlineTask.files;
    handleOfflineFiles(filesNeedHandle, offlineTask.allFileNum, offlineTask.handledFilesNum, updateIndex, offlineTask.taskId);
    offlineTask.handledFilesNum = offlineTask.allFileNum;
}

/**
 * 获取需要分析的数据包文件
 * * @param offlineFiles
 */
list<string> CaptureCatchThread::getFilesNeedHandle(list<string> handledFiles, list<string> allFiles) {
    list<string> filesNeedHandle;
    for (string allFile:allFiles) {
        bool fileIsHandled = false;
        for (string handledFile:handledFiles) {
            if (handledFile == allFile)
                fileIsHandled = true;
        }
        if (!fileIsHandled)
            filesNeedHandle.push_back(allFile);
    }
    return filesNeedHandle;
}

/**
 * 处理任务
 * @param offlineTasks
 * @param updateIndex
 */
void CaptureCatchThread::handleTasks(list<OfflineTask> offlineTasks, string updateIndex) {
    for (OfflineTask offlineTask: offlineTasks) {
        if(offlineTask.isListening){
            haveListeningtask = true;
            handleListenTask(offlineTask, updateIndex);
        }else{
            handleOfflineFiles(offlineTask.files, offlineTask.files.size(), offlineTask.handledFilesNum, updateIndex, offlineTask.taskId);
        }
    }
}

/**
 * 处理文件，离线处理对文件识别不同的格式采用pcapedit命令行方式进行转换为pcap格式，零libpca中的pcap_loop函数对离线数据包进行处理，将读取的离线数据包文件，放入buf中让后面的处理数据进行处理
 * 
 * @param files
 * @param allFileNum
 * @param handledFileNum
 * @param updateIndex
 * @param handleTaskId
 */
void CaptureCatchThread::handleOfflineFiles(list<string> files, int allFileNum, int handledFileNum, string updateIndex,
                                            string handleTaskId) {
    time_t st = time(NULL);
    pcap_t *pcapHandle;
    char error[1000] = {0};
    int handleResult = 0;
    for (const auto &pcapFile:files) {
        if ((++handledFileNum) % 10 == 0) // 每10个数据包更新一次状态 可能为已处理文件数量handledFileNum
            handleOffline->updateHandleStatus(updateIndex, handledFileNum, allFileNum, handleTaskId);

		size_t pos = pcapFile.find_last_of('.'); //判断文件类型
		if (pos == string::npos)
			continue;

		string suffix = pcapFile.substr(pos + 1, string::npos);
		string pcapFileTmp = pcapFile;
		if (suffix == "5vw" || suffix == "erf" || suffix == "snoop" || suffix == "bfr" || suffix == "cap")
		{
			pcapFileTmp = string(pcapFile.begin(), pcapFile.begin() + pos);
			pcapFileTmp += ".pcap";
			string cmd = "editcap -F pcap " + pcapFile + " " + pcapFileTmp;
			system(cmd.c_str());
		}

		
        rte_log(RTE_LOG_DEBUG, RTE_LOGTYPE_USER1, "处理数据包文件%s\n", pcapFileTmp.c_str());
        pcapHandle = pcap_open_offline(pcapFileTmp.c_str(), error);
        if (pcapHandle == nullptr){
            rte_log(RTE_LOG_ERR, RTE_LOGTYPE_USER1, "处理数据包文件%s错误：%s\n", pcapFileTmp.c_str(), error);
            continue;
        }

        handleResult = pcap_loop(pcapHandle, -1, offlinePacketHandle, nullptr);// pcap_loop函数是什么？离线包处理，最终调用了handlePackets
        if (handleResult < 0)
            rte_log(RTE_LOG_ERR, RTE_LOGTYPE_USER1, "处理数据包错误，错误代码：%d,错误文件：：%s\n", handleResult, pcapFileTmp.c_str());


		if (pcapFile != pcapFileTmp)
			remove(pcapFileTmp.c_str());
    }
    handleOffline->updateHandleStatus(updateIndex, handledFileNum, allFileNum, handleTaskId);// 将处理的数据包存入es中
    time_t ed = time(NULL); // 记录处理的时间
    printf("离线文件已处理完毕，处理文件数量：%d ,time:%d\n", handledFileNum, ed - st);
    flushPackets();
}

/**
 * 读取数据包 对单包进行处理与处理，应该是最小的处理单元，
 * @return
 */
void CaptureCatchThread::handlePackets(const u_char *packetContent, const struct pcap_pkthdr *packetHeader) {
    //大于门限值的数据包暂时不处理 默认IP层，最大数据包为1514，超过时，不尽行处理
    if (packetHeader->len > PACKET_MTU)
        return;
    pcaketToMbuf(packetContent, packetHeader);// 将数据包存入buf中
    packetNumRxed++;
    if (packetNumRxed % MAX_PACKET_GET_BULK != 0) // MAX_PACKET_GET_BULK 存储 每32个mbuf为一个bulk，每满一个bulf进行一次读包处理，传入到队列中，并进行一次刷包
        return;
    //add by benxiaohai方案三转发离线数据包至suricata，离线数据包默认从第一个口出
//    forward(0);
    flushPackets(); // 将获取的数据包输入到管道，并释放内存
}

/**
 * 刷新数据包 将部分内容（指针）通过队列管道形式输出，并记录已使用的membufapplyNum，释放未使用的内存空间，并初始化packetNumRxed 该参数为指定时间获取的数据包个数
 */
void CaptureCatchThread::flushPackets() {
    packetParser();//
    membufApplyNum += MAX_PACKET_GET_BULK; // mem bufApplyNum为 内存buf用的数量，
    if (packetNumRxed < MAX_PACKET_GET_BULK)
        freeMembufUnused();
    packetNumRxed = 0;  // 一定时间接收数据包的数量
    while (rte_pktmbuf_alloc_bulk(offlineMembufPool, bufs, MAX_PACKET_GET_BULK) != 0) {
        rte_log(RTE_LOG_ERR, RTE_LOGTYPE_USER1, "从mbuf内存池申请mbuf对象错误,总共分配membuf数量：%lu,mempool数量:%u\n", membufApplyNum,
                rte_mempool_in_use_count(offlineMembufPool));
        rte_delay_us(WAITE_MEMPOOL_HANDLE_TIME * 1000);
    }
}

/**
 * 数据包字符转换为mbuf 是否为将抓取的数据存入mbuf中
 * @return
 */
void CaptureCatchThread::pcaketToMbuf(const u_char *packetContent, const struct pcap_pkthdr *packetHeader) {
    rte_pktmbuf_reset(bufs[packetNumRxed]);
    bufs[packetNumRxed]->timestamp = 1000000 * packetHeader->ts.tv_sec + packetHeader->ts.tv_usec;
    char *pkt_mbuf_char = rte_pktmbuf_append(bufs[packetNumRxed], packetHeader->len);// rte_pktmbuf_append函数作用
    if (pkt_mbuf_char == nullptr) {
        rte_log(RTE_LOG_ERR, RTE_LOGTYPE_USER1, "mbuf 申请错误:%s", rte_strerror(rte_errno));
        return;
    }

    rte_memcpy(pkt_mbuf_char + offlineEtherOffset, packetContent, packetHeader->len);
}

/**
 * 在线捕获，根据分配的环形队列，遍历所有端口进行数据包抓包，如果采用多线程，是否存在数据包重复的情况呢？
 */
void CaptureCatchThread::startCaptureOnline() {
    while (capture_signal == CAPTURE_SIGNAL_RUN) {
        for (int ring_num = startRxRingNum; ring_num < endRxRingNum; ring_num++) {
            rxPakcketFromPort(ring_num);
        }
    }
}

/**
 * 根据网口收包，遍历所有端口进行数据包的采集
 * @param port
 */
void CaptureCatchThread::rxPakcketFromPort(u_int16_t ring_num) {
    for (uint16_t port:GlobalConfig.portCfg.rxPorts) {
        rxPakcketFromPortRing(ring_num, port);
    }
}

/**
 * 根据网口和环形队列收到的数据包，
 * @param ring_num
 * @param port
 */
void CaptureCatchThread::rxPakcketFromPortRing(u_int16_t ring_num, u_int16_t port) {
    packetNumRxed = rte_eth_rx_burst(port, ring_num, bufs, MAX_PACKET_GET_BULK);//  packetNumRxed 类的全局变量
    if (unlikely(packetNumRxed == 0))
        return;
//    forward(ring_num);
    packetParser();// 通过读取全局变量抓取的数据包数量，按照一定时间进行统计，按照hash rss进行分类
}

/**
*   转发 (可删除) 逻辑为，如果为shunt 是否为分流概念呢？其中分流没有实现，该函数没有使用
*/
void CaptureCatchThread::forward(int ringNum){
    if(GlobalConfig.captureCfg.forwardMode == FORWARD_MODE_SHUNT){
        txPacketsShunt(ringNum);
    }else if(GlobalConfig.captureCfg.forwardMode == FORWARD_MODE_CONVERGENCE){
        txPacketsConvergence();
    }
}
/**
*   通过队列向端口转发，默认从0口收包，其他口发包（分流）
*   1、配置多少队列默认多少发包口发包以达到同源同溯的目的
*   2、发包口为对应队列数加1，发包队列默认为第一个对列
*/
void CaptureCatchThread::txPacketsShunt(int ringNum){
//     int test = rte_eth_tx_burst(GlobalConfig.portCfg.txPorts[ringNum % txPortNum], 0, bufs, packetNumRxed);
//     txNUM +=test;
//    if(test!=packetNumRxed)
//        printf("收包与发包数量不一致 %d - %d", packetNumRxed, test);
//     for(int i = test;i<packetNumRxed;i++){
//         rte_pktmbuf_free(bufs[i]);
//     }
}

/**
*   未实现（汇聚)
*txRing为类参数，其记录环形队列的起始，如果大于最后的环形队列数，则将开始的发送环形队列辅助给txRing
* 算法可能存在问题，在汇总所有网口的数据包时，应该是双循环，循环多个网口、循环某个网口下得多个环形队列或者，针对发包所有网口，只有一个环形对列，大概率为该情况
*/
void CaptureCatchThread::txPacketsConvergence() {
    if (txRing >= endTxRingNum)
        txRing = startTxRingNum;
    for (uint16_t portid:GlobalConfig.portCfg.txPorts)
        rte_eth_tx_burst(portid, txRing++, bufs, packetNumRxed);
}

/**
 * 会话重组（不是会话重组） 说明：数据包解析实现对采集数据包的初步分类及同一会话数据包放在同一队列中，默认当采集超过1千万个数据包时，打印采集状态信息
 * 最终将数据池的地址入队列中
 */
void CaptureCatchThread::packetParser() {
    // packetNumRxed和packetRecvied、idxTest 为类中的参数值
    for (int i = 0; i < packetNumRxed; i++) {
        if (++packetRecvied % 10000000 == 0&& idxTest==0) {
            membufpoolTest = bufs[i]->pool;
            capture_status1();
        }
        if ((*outputIPipe)[startQueueNum + bufs[i]->hash.rss % queueNumForThread]->Enqueue(bufs[i]) != 0) {
            errInputQueue++;
            rte_pktmbuf_free(bufs[i]);
        }
    }

}

/**
 * ring 初始化环形队列，接收网卡采集的数据包，其中环形队列包括接收所需的环形队列和转发（汇聚模式）的环形队列
 */
void CaptureCatchThread::initRings(int idx, int threadNum) {
    // startRxRingNum和endRxRingNum为类中参数
    initRing(idx, threadNum, &startRxRingNum, &endRxRingNum, GlobalConfig.portCfg.rxRingNum);
    printf("max rx ring num :%d,startRxRingNum:%d,endRxRingNum:%d", GlobalConfig.portCfg.rxRingNum, startRxRingNum,
           endRxRingNum);
    if(GlobalConfig.captureCfg.forwardMode == FORWARD_MODE_CONVERGENCE){
        initRing(idx, threadNum, &startTxRingNum, &endTxRingNum, GlobalConfig.portCfg.txRingNum);
        txRing = startTxRingNum;
        printf("max tx ring num :%d,startTxRingNum:%d,endTxRingNum:%d", GlobalConfig.portCfg.txRingNum, startTxRingNum, endTxRingNum);
    }
}
// 与initQueue思路相似，推测idx为自定义的任务或者线程编号
void CaptureCatchThread::initRing(int idx, int threadNum, int *start, int *end, int ringNum) {
    int rxRingForThread = ringNum / threadNum + (idx>=ringNum % threadNum|| ringNum % threadNum == 0 ? 0 : 1);
    *start = rxRingForThread * idx + (idx<ringNum % threadNum? 0 : ringNum % threadNum);
    *end = startRxRingNum + rxRingForThread;
}
// idx 为什么呢线程编号？ 初始化队列用于传输什么数据呢，与哪个模块对接呢
// 系统将设置的默认管道数量除线程数量，计算得到平均每个线程队列数量，并且计算出队列的起始编号和结束编号
void CaptureCatchThread::initQueue(int idx, int threadNum) {
    queueNumForThread= outputIPipe->size() / threadNum +
                        (idx>=outputIPipe->size() % threadNum || outputIPipe->size() % threadNum == 0 ? 0 : 1);
    startQueueNum =      queueNumForThread * idx + (idx<outputIPipe->size() % threadNum? 0 : outputIPipe->size() % threadNum); // 类中参数
    endQueueNum = startQueueNum + queueNumForThread; // 类中参数，每个抓包线程一个实例对象
}

/**
 * 停止抓包，是直接将相应的线程关闭
 * @param session
 */
void CaptureCatchThread::captureStop() {
    capture_signal = CAPTURE_SIGNAL_STOP;
    // rte_exit(EXIT_SUCCESS, "核心%d收到停止信号退出抓包状态\n", lcore_id);
    // 本进程发送SIGMIN信号

	 // select 120s
	 struct timeval tv;
	 tv.tv_sec = 30;
	 tv.tv_usec = 0;
	 select(0, NULL, NULL, NULL, &tv);
     kill(getpid(), SIGRTMIN); 
     return;
}

/**
 * 释放未使用的内存空间数据包，其中packetNumRxed为已接收数据包数量，MAX_PACKET_GET_BULK应该为默认的最大数据包数量
 * @param session
 */
void CaptureCatchThread::freeMembufUnused() {
    for (int i = packetNumRxed; i < MAX_PACKET_GET_BULK; ++i) {
        rte_pktmbuf_free(bufs[i]);
    }
}
