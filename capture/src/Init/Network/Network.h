/**
 * Created by benxiaohai on 19-4-12.
 * 描述：初始化网络接口，采集、发送端口，当前配置采集
 * Copyright (c) 2013 - 2019 成都安舟. All rights reserved.
 */

#ifndef DPDKCAPTURE_NETWORK_H
#define DPDKCAPTURE_NETWORK_H

#include <rte_ethdev.h>
#include <rte_string_fns.h>
#include <rte_malloc.h>
#include <ctime>
#include "Common/Common.h"
#define RX_PORT_TYPE 1
#define TX_PORT_TYPE 2
#define NET_IXGBE "net_ixgbe"
#define NET_I40E "net_i40e"
#define MAX_NETWORKS 4
using namespace std;
struct PortConfig{
    uint16_t rx_ring_size;
    uint16_t tx_ring_size;
    uint16_t rx_ring_num;
    uint16_t tx_ring_num;
};
class Network {
public:
    //析构函数
    Network();

    //解构函数
    ~Network();

private:

    //对称RSS key
    uint8_t rss_intel_key[40] = {0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A, 0x6D,
                                        0x5A, 0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A,
                                        0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A, 0x6D,
                                        0x5A};
    //以太地址
//    struct ether_addr addr;

    // 发包端口配置由于我们主要做采集转发，所以分开配网络接口，这一点跟做一般网卡路由不一样
    struct rte_eth_conf *port_conf_tx = (struct rte_eth_conf *) rte_malloc(NULL, sizeof(struct rte_eth_conf), 0);

    //初始化
    void init();

    //初始化收包端口
    void initRxPort(uint16_t port);

    void initTxPort(uint16_t port);

    void initTimestamp();

    void initRxPorts();

    void initTxPorts();

    void getPortConfig(uint16_t port,PortConfig *portConfig);

    void networkConfig(rte_eth_conf *port_conf_rx, rte_eth_dev_info *dev_info);

    void SetFlowTypeMask(struct rte_eth_hash_filter_info *info, uint32_t ftype);

    void networkI40eSet(uint16_t port);
};


#endif //DPDKCAPTURE_NETWORK_H
