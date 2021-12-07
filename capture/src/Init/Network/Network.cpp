/**
 * Created by root on 19-4-12.
 * 描述：
 * Copyright (c) 2013 - 2019 成都安舟. All rights reserved.
 */
#include "Network.h"
#include "xdpi_conf.h"
#include "CaptureThread/CaptureCatchThread.h"

uint64_t timestampInit = 0;// 作用是什么呢？
uint64_t timestampInitRteRdtsc = 0;//作用是什么呢
uint64_t cpuHZ = 0;// 作用哦是什么呢

//  add_timestamps 当dpdk采集到数据包后，对数据包添加时间戳，用于dpdk rxtx_callbacks回调函数
static uint16_t add_timestamps(uint16_t port __rte_unused, uint16_t qidx __rte_unused,
                               struct rte_mbuf **pkts, uint16_t nb_pkts,
                               uint16_t max_pkts __rte_unused, void *_ __rte_unused) {
    unsigned i;
    if (nb_pkts == 0) {
        return 0;
    }
    uint64_t now = (rte_rdtsc() - timestampInitRteRdtsc) / cpuHZ + timestampInit;

    for (i = 0; i < nb_pkts; i++){
        pkts[i]->timestamp = now;
    }
    return nb_pkts;
}

/**
 * 析构函数 初始化硬件网口，调用network类中的init函数，该函数只初始化在线模块其中转发模式也属于在线模式的一种
 */
Network::Network() {
    // 检测可用网络接口
    if (rte_eth_dev_count_avail() < 1) // 调研的函数作用是什么呢？
        rte_exit(EXIT_FAILURE, "Error: 无可用网络接口\n");
    init();
}

/**
 * 解构函数
 */
Network::~Network() {}

/**
 * 初始化网络口
 * @return
 * xxxxxxxxxxxxxxxxx此处需要读取 配置文件中的运行模式如在线和转发
 */
void Network::init() {
    initTimestamp();// 为什么需要初始化时间戳呢？
    if (GlobalConfig.captureCfg.runMode != RUN_MODLE_OFFLINE)
        initRxPorts();
    if (GlobalConfig.captureCfg.forwardMode != FORWARD_MODE_NO)
        initTxPorts();
}


// initRxPorts 读取了配置文件的接收数据网口，调用initRxPort分别进行了初始化
void Network::initRxPorts() {
    for (uint16_t portid:GlobalConfig.portCfg.rxPorts) {
        initRxPort(portid);
    }
}

void Network::initTxPorts() {
    for (uint16_t portid:GlobalConfig.portCfg.txPorts) {
        initTxPort(portid);
    }
}

void Network::initTimestamp() {
    cpuHZ = rte_get_tsc_hz() / 1000000;
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    timestampInit = tv.tv_sec * 1000000 + tv.tv_usec; // 函数运行完后，修改了该参数信息
    timestampInitRteRdtsc = rte_rdtsc(); // 函数运行完后，修改了该参数信息
}
// 设置dpdk相关配置包括流类型设置，具体有什么作用待定
void Network::networkConfig(rte_eth_conf *port_conf_rx, rte_eth_dev_info *dev_info) {
    port_conf_rx->rxmode.mq_mode = ETH_MQ_RX_RSS;
    port_conf_rx->rxmode.offloads = dev_info->rx_offload_capa;
    port_conf_rx->rxmode.offloads &= ~DEV_RX_OFFLOAD_TCP_LRO;
    port_conf_rx->rxmode.offloads &= ~DEV_RX_OFFLOAD_KEEP_CRC;
    port_conf_rx->rxmode.offloads &= ~DEV_RX_OFFLOAD_JUMBO_FRAME;
    port_conf_rx->rxmode.max_rx_pkt_len = RTE_ETHER_MAX_LEN;
    port_conf_rx->rx_adv_conf.rss_conf.rss_key = rss_intel_key;
    port_conf_rx->rx_adv_conf.rss_conf.rss_hf = dev_info->flow_type_rss_offloads;
    port_conf_rx->link_speeds = dev_info->speed_capa;
    port_conf_rx->txmode.offloads = dev_info->tx_offload_capa;
    port_conf_rx->fdir_conf.mode = RTE_FDIR_MODE_NONE;
}

/**
*   设置流类型（网上抄的，未研究）什么是流类型
*/
void  Network::SetFlowTypeMask(struct rte_eth_hash_filter_info *info, uint32_t ftype)
{
    uint32_t idx, offset;
    idx = ftype / (CHAR_BIT * sizeof(uint32_t));
    offset = ftype % (CHAR_BIT * sizeof(uint32_t));
    info->info.global_conf.valid_bit_mask[idx] |= (1UL << offset);
    info->info.global_conf.sym_hash_enable_mask[idx] |= (1UL << offset);
}

void Network::networkI40eSet(uint16_t port) {
    struct rte_eth_hash_filter_info info;
    if (rte_eth_dev_filter_supported(port, RTE_ETH_FILTER_HASH) < 0)
        rte_exit(EXIT_FAILURE, "网口 %d 不支持 RTE_ETH_FILTER_HASH\n", port);

    memset(&info, 0, sizeof(info));
    info.info_type = RTE_ETH_HASH_FILTER_GLOBAL_CONFIG;
//    info.info.enable = 1;
    info.info.global_conf.hash_func = RTE_ETH_HASH_FUNCTION_TOEPLITZ;

    // i40e支持的所有flow type见drivers/net/i40e/i40e_ethdev.c, I40E_FLOW_TYPES
    //SetFlowTypeMask(&info, RTE_ETH_FLOW_IPV4);
    SetFlowTypeMask(&info, RTE_ETH_FLOW_FRAG_IPV4);
    SetFlowTypeMask(&info, RTE_ETH_FLOW_NONFRAG_IPV4_TCP);
    SetFlowTypeMask(&info, RTE_ETH_FLOW_NONFRAG_IPV6_TCP);
    SetFlowTypeMask(&info, RTE_ETH_FLOW_NONFRAG_IPV4_OTHER);
    //SetFlowTypeMask(&info, RTE_ETH_FLOW_IPV6);
    SetFlowTypeMask(&info, RTE_ETH_FLOW_FRAG_IPV6);
    SetFlowTypeMask(&info, RTE_ETH_FLOW_NONFRAG_IPV6_OTHER);
    //SetFlowTypeMask(&info, RTE_ETH_FLOW_IPV6_EX);

    //SetFlowTypeMask(&info, RTE_ETH_FLOW_IPV6_TCP_EX);
    SetFlowTypeMask(&info, RTE_ETH_FLOW_NONFRAG_IPV4_UDP);
    SetFlowTypeMask(&info, RTE_ETH_FLOW_NONFRAG_IPV6_UDP);
    //SetFlowTypeMask(&info, RTE_ETH_FLOW_IPV6_UDP_EX);
    if (rte_eth_dev_filter_ctrl(port, RTE_ETH_FILTER_HASH, RTE_ETH_FILTER_SET, &info) < 0)
        rte_exit(EXIT_FAILURE, "网卡 %d 不支持全局HASH配置\n", port);
}


/**
 * 初始化收包接口 完成端口的初始化设置，包括存储数据包的地址，以及网卡的混杂模式设置，后面是否可以将过滤放在此处
 * @param port
 */
void Network::initRxPort(uint16_t port) {
    rte_eth_conf port_conf_rx;
    memset(&port_conf_rx, 0, sizeof(port_conf_rx));
    rte_eth_dev_info dev_info;
    PortConfig portConfig;
    unsigned int port_socket_id = rte_eth_dev_socket_id(port);

    rte_eth_dev_info_get(port, &dev_info);
    getPortConfig(port ,&portConfig);
    if (portConfig.rx_ring_num > dev_info.max_rx_queues)
        rte_exit(EXIT_FAILURE, "网络接口队列数大于网卡支持的队列数：%d\n", dev_info.max_rx_queues);

    if (portConfig.rx_ring_size > dev_info.rx_desc_lim.nb_max)
        rte_exit(EXIT_FAILURE, "网络接口队列长度大于网卡支持的队列长度：%d\n", dev_info.rx_desc_lim.nb_max);

    networkConfig(&port_conf_rx, &dev_info);


    if (!rte_eth_dev_is_valid_port(port))
        rte_exit(EXIT_FAILURE, "网络接口 - %d无效\n", port);

    if (rte_eth_dev_configure(port, portConfig.rx_ring_num, 0, &port_conf_rx) != 0)
        rte_exit(EXIT_FAILURE, "网络接口 - %d配置失败\n, error:%s", port, rte_strerror(rte_errno));


    if(memcmp(dev_info.driver_name, NET_IXGBE, sizeof(NET_IXGBE))==0){
    }else if(memcmp(dev_info.driver_name, NET_I40E, sizeof(NET_I40E))==0){
        networkI40eSet(port);
        struct rte_eth_hash_filter_info info;
        memset(&info, 0, sizeof(info));
        info.info_type = RTE_ETH_HASH_FILTER_SYM_HASH_ENA_PER_PORT;
        info.info.enable = 1;
        if(rte_eth_dev_filter_ctrl(port, RTE_ETH_FILTER_HASH, RTE_ETH_FILTER_SET, &info))
            rte_exit(EXIT_FAILURE, "网卡 %d 不能设置对称HASH\n", port);
    }else{
        rte_exit(EXIT_FAILURE, "网卡的驱动类型未支持：%d\n", dev_info.driver_name);
    }
    if (rte_eth_dev_adjust_nb_rx_tx_desc(port, &portConfig.rx_ring_size, &portConfig.tx_ring_size) != 0)
        rte_exit(EXIT_FAILURE, "网络接口 - %d队列配置不正确，队列长度必须为2的指数\n", port);

    //初始化收包队列，指定numa，内存池，启动端口
    for (uint16_t rx_ring = 0; rx_ring < portConfig.rx_ring_num; rx_ring++) {
		string mem_pool_name = "mbuf_pool_" + to_string(port_socket_id) + to_string(port) + to_string(rx_ring);
    	struct rte_mempool* pool = rte_pktmbuf_pool_create(mem_pool_name.c_str(), GlobalConfig.captureCfg.mbufSize,  
										MBUF_CACHE_SIZE,       0, RTE_MBUF_DEFAULT_BUF_SIZE, port_socket_id);
		if(pool==nullptr)
		   rte_exit(EXIT_FAILURE, "内存池创建失败 - error：%s\n", rte_strerror(rte_errno));
        if (rte_eth_rx_queue_setup(port, rx_ring, portConfig.rx_ring_size, port_socket_id, nullptr, pool) < 0)
            rte_exit(EXIT_FAILURE, "网络接口 - %d初始化内存池错误\n", port);
        rte_eth_add_rx_callback(port, rx_ring, add_timestamps, nullptr);//  回调函数除了增加时间戳，也可以进行其他方面处理
    }

//	PkgFilter filter(port, rx_rings);
//	filter.LoadFilter();
    if (rte_eth_dev_start(port) < 0)
        rte_exit(EXIT_FAILURE, "网络接口 - %d启动失败\n", port);

    printf("收包网络接口 %u 初始化成功\n", port);

    /**端口设置为混杂模式接收所有的包 */
    rte_eth_promiscuous_enable(port);
}
// 从配置文件中读取环形队列个数与缓存大小
void Network::getPortConfig(uint16_t port,PortConfig *portConfig){

    portConfig->rx_ring_size = GlobalConfig.portCfg.rxRingSize;
    portConfig->tx_ring_size = GlobalConfig.portCfg.txRingSize;

    portConfig->rx_ring_num = GlobalConfig.portCfg.rxRingNum;
    portConfig->tx_ring_num = GlobalConfig.portCfg.txRingNum;
}
// 初始化发包网口
void Network::initTxPort(uint16_t port) {
    rte_eth_conf port_conf_tx;
    memset(&port_conf_tx, 0, sizeof(port_conf_tx));
    rte_eth_dev_info dev_info;
    PortConfig portConfig;
    rte_eth_dev_info_get(port, &dev_info);
    if (!rte_eth_dev_is_valid_port(port))
        rte_exit(EXIT_FAILURE, "发包网络接口 - %d无效\n", port);
    getPortConfig(port ,&portConfig);
    port_conf_tx.txmode.mq_mode = ETH_MQ_TX_NONE;
    if (dev_info.tx_offload_capa & DEV_TX_OFFLOAD_MBUF_FAST_FREE)
        port_conf_tx.txmode.offloads |= DEV_TX_OFFLOAD_MBUF_FAST_FREE;
    port_conf_tx.rxmode.max_rx_pkt_len = RTE_ETHER_MAX_LEN;
    port_conf_tx.link_speeds = dev_info.speed_capa;
    port_conf_tx.txmode.offloads = dev_info.tx_offload_capa;
//    port_conf_tx.rxmode.offloads = dev_info.rx_offload_capa;
//    port_conf_tx.rxmode.offloads &= ~DEV_RX_OFFLOAD_TCP_LRO;
//    port_conf_tx.rxmode.offloads &= ~DEV_RX_OFFLOAD_KEEP_CRC;
    port_conf_tx.rxmode.offloads = 0;
//    port_conf_tx.rx_adv_conf.rss_conf.rss_hf = 0;

    port_conf_tx.fdir_conf.mode = RTE_FDIR_MODE_NONE;
    

    if (rte_eth_dev_configure(port, 0, portConfig.tx_ring_num, &port_conf_tx) != 0)
        rte_exit(EXIT_FAILURE, "发包网络接口 - %d 队列数量配置失败: %s\n", port, rte_strerror(rte_errno));

    /** 调整队列长度到限制范围 - 队列长度必须为2的幂 */
    if (rte_eth_dev_adjust_nb_rx_tx_desc(port, &portConfig.rx_ring_size, &portConfig.tx_ring_size) != 0)
        rte_exit(EXIT_FAILURE, "发包网络接口 - %d队列长度配置失败\n", port);

    struct rte_eth_txconf tx_port_conf = dev_info.default_txconf;
//    tx_port_conf.offloads = port_conf.txmode.offloads;

    /**初始化转发队列，指定numa，内存池，启动端口**/
    for (uint16_t tx_ring = 0; tx_ring < portConfig.tx_ring_num; tx_ring++) {
        if (rte_eth_tx_queue_setup(port, tx_ring, portConfig.tx_ring_size, (unsigned int) rte_eth_dev_socket_id(port), &tx_port_conf) < 0)
            rte_exit(EXIT_FAILURE, "发包网络接口 - %d队列初始化配置失败\n", port);
    }

    /** 启动端口 */
    if (rte_eth_dev_start(port) < 0)
        rte_exit(EXIT_FAILURE, "启动发包网络接口 - %d失败\n", port);

    printf("发包网络接口 %u 初始化成功\n", port);

    /**.端口设置为混杂模式 */
    rte_eth_promiscuous_enable(port);
}
