/*
 *
 * Created by root on 19-4-12.
 * 描述：
 * Copyright (c) 2013 - 2019 成都安舟. All rights reserved.
 */
#ifndef CAPTURE_CAPTURECATCHTHREAD_H
#define CAPTURE_CAPTURECATCHTHREAD_H

#include <rte_ethdev.h>
#include <rte_mbuf.h>
#include <rte_reorder.h>
#include <rte_malloc.h>
#include <rte_jhash.h>
#include <rte_timer.h>
#include <rte_lcore.h>
#include <rte_thash.h>
#include <map>
#include "HandleOffline.h"
#include <generic/rte_rwlock.h>
#include <Common/Common.h>
#include <xdpi_pipes.h>
#include <vector>
#include <pcap.h>


#define SESSION_KEY_LEN 19 // 作用是什么？
#define CAPTURE_SIGNAL_RUN 1 // 谁发出的采集信号
#define CAPTURE_SIGNAL_STOP 2 // 谁发出的停止信号
#define HANDLE_TIMEOUT_NUM 50 // 什么处理时间超时
#define HASH_TABLE_FULL_SLEEP_TIME 8192 // hash table 满时暂停时间
#define MAX_SESSION_GET_BULK 256 // 什么是最大会话 bulk？
#define MAX_PACKET_GET_BULK 32 // 什么是最大数据包 bulk
#define PCAP_ERRBUF_SIZE 256 // 数据包错误buf大小什么意思
#define PACKET_MTU 1514 // 数据包mtu 即网络层的IP规定，每个单独的数据包大小不能超过1500字节
#define WAITE_MEMPOOL_HANDLE_TIME 5000 // 等待内存池处理时间，可能用于采集获取的数据包，提供上游业务处理


#define MBUF_CACHE_SIZE 250 // 作用是什么呢？

// 离线数据包处理函数，餐宿：argument什么意思？难道是后台调用命令行进行离线分析？pktHeader数据包头 和数据包内容，是对pcap文件还是单个数据包进行处理呢？
void offlinePacketHandle(u_char *argument, const struct pcap_pkthdr *pktHeader, const u_char *packetContent);

class CaptureCatchThread {
public:
    // 析构函数
    CaptureCatchThread();

    // 解构函数
    ~CaptureCatchThread();

    //初始化
    void init(int idx, int threadNum);

    //开始捕获
    int start(int idx, int threadNum, vector<IPipe *> *output);

    //停止捕获
    void captureStop();

    //核心id 是CPU核心？还是什么？
    unsigned lcore_id;
    // 参数类似离线处理，这里在线数据，是对单个数据包处理？需要确认
    void handlePackets(const u_char *packetContent, const struct pcap_pkthdr *packetHeader);

private:
    map<string, list<string>> listenTask;//  作用是什么呢？
    int haveListeningtask = false; // 是启动采集监听的标志？

    HandleOffline *handleOffline;// 离线分析类实例化
    int idxTest = 0;// 作用是什么呢？
    uint64_t packetRecvied = 0; // 作用是什么呢？
    u_int64_t queueErr = 0; // 作用是什么呢？


    int8_t startQueueNum = 0;// 队列数，队列用于什么呢？

    int8_t endQueueNum = 0;// 是输出队列数？输出内容是什么呢？

    int8_t queueNumForThread = 0; // 与线程有关的队列数？

    u_int64_t membufApplyNum = 0; // 什么作用

    vector<IPipe *> *outputIPipe; // 输出管道指针

    int startRxRingNum = 0;// 什么作用
    int endRxRingNum  = 0;// 什么作用
    int startTxRingNum = 0;// 什么作用
    int endTxRingNum  = 0;// 什么作用

    int txPortNum = 0;// 什么作用
    int txRing = 0;// 什么作用
    //获取数据包次数 什么一是一
    uint32_t capture_num = 0;

    //当前核socket id 是CPU核标记？
    int socket_id;

    //数据包mbuf数据池
    rte_mempool *offlineMembufPool;// 此处为采集数据指针地址？可能要传递到下一个模块

    //已使用的packet对象数
    int handledPacket = 0;// 什么叫pacake对象，是采用多线程？

    //抓包状态
    int capture_signal;// 谁发出的抓包信号

    //数据包队列
    list<uint16_t> packet_rings;// 这个是输出队列么？

    //采集端口 即硬件上可用于采集的口子
    list<u_int16_t> rx_ports; 

    //每次获取数据包的mbuf集和  是所有采集数据包的指针地址？
    struct rte_mbuf *bufs[MAX_PACKET_GET_BULK];

    //每次获取到数据包数量
    uint16_t packetNumRxed = 0;

    //分析离线包数量
//    uint16_t offlinePacketNum = 0;

    int offlineEtherOffset = 0;

    //从网口中收包 采集模块的采集
    void rxPakcketFromPort(u_int16_t port);

    //从队列中收包 和从网口中收包有什么区别呢
    void rxPakcketFromPortRing(u_int16_t ring, u_int16_t port);

    //packet内存池初始化
    void packetMempoolInit(); // 关键的输出

    //会话重组 此处会话重组和sessionregroup有什么区别以及flow
    void packetParser();

    //为核心分配收包ring 为CPU分配环形队列？
    void initRings(int idx, int threadNum);


    //在线捕获
    void startCaptureOnline();
    // 离线分析函数
    void startCaptureOffline();
    // 完成什么功能呢
    void flushPackets();
    // 将数据包放在内存中？
    void pcaketToMbuf(const u_char *packetContent, const struct pcap_pkthdr *packetHeader);
    // 释放不用的内存？
    void freeMembufUnused();
    // 初始化队列，用于进程间通信，通信内容是什么呢？是信号的传递？
    void initQueue(int idx, int threadNum);
    // 初始化环形队列，进行采集，此处是否调用了network相关函数
    void initRing(int idx, int threadNum, int *start, int *end, int ringNum);
    // 发送数据包？
    void txPackets();
    // 转发数据包函数
    void forward(int ringNum);
    // 发送数据包关闭
    void txPacketsShunt(int ringNum);
    // 发送数据包融合
    void txPacketsConvergence();

//    void handleTask(OfflineTask offlinePcapFile); 统计需要处理离线文件数量？内容？
    list<string> getFilesNeedHandle(list<string> handledFiles, list<string> allFiles);

    //处理离线文件
    void handleOfflineFiles(list<string> files, int allFileNum, int handledFileNum, string updateIndex, string handleTaskId);
    // 处理任务和处理监听任务区别是什么呢？
    void handleTasks(list<OfflineTask> offlineFiles, string updateIndex);
    // 处理监听任务什么意思呢？为什么有处理任务
    void handleListenTask(OfflineTask offlineTask, string updateIndex);
};

#endif //CAPTURE_CAPTURECATCHTHREAD_H
