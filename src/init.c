/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <errno.h>
#include <getopt.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/queue.h>
#include <sys/types.h>

#include <rte_atomic.h>
#include <rte_branch_prediction.h>
#include <rte_byteorder.h>
#include <rte_common.h>
#include <rte_cycles.h>
#include <rte_debug.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_ether.h>
#include <rte_flow.h>
#include <rte_interrupts.h>
#include <rte_ip.h>
#include <rte_launch.h>
#include <rte_lcore.h>
#include <rte_log.h>
#include <rte_lpm.h>
#include <rte_mbuf.h>
#include <rte_memcpy.h>
#include <rte_memory.h>
#include <rte_mempool.h>
#include <rte_memzone.h>
#include <rte_pci.h>
#include <rte_per_lcore.h>
#include <rte_prefetch.h>
#include <rte_random.h>
#include <rte_ring.h>
#include <rte_string_fns.h>
#include <rte_tailq.h>
#include <rte_tcp.h>

#include "autoconf.h"
#include "main.h"

static struct rte_eth_conf port_conf = {
    .rxmode =
        {
            .mq_mode        = ETH_MQ_RX_RSS,
            .max_rx_pkt_len = 9212,  // 1518,  // ETHER_MAX_JUMBO_FRAME_LEN,
            .split_hdr_size = 0,
            .header_split   = 1, /**< Header Split disabled */
            .hw_ip_checksum = 0, /**< IP checksum offload disabled */
            .hw_vlan_filter = 0, /**< VLAN filtering disabled */
            .jumbo_frame    = 1, /**< Jumbo Frame Support enabled */
            .hw_strip_crc = 0, /**< CRC stripped by hardware */
        },
    .rx_adv_conf =
        {
            .rss_conf =
                {
                    .rss_key = NULL,
                    .rss_hf  = ETH_RSS_IP,
                },
        },
    .txmode =
        {
            .mq_mode = ETH_MQ_TX_NONE,
        },
};

static struct rte_eth_rxconf rx_conf = {
    .rx_thresh =
        {
            .pthresh = APP_DEFAULT_NIC_RX_PTHRESH,
            .hthresh = APP_DEFAULT_NIC_RX_HTHRESH,
            .wthresh = APP_DEFAULT_NIC_RX_WTHRESH,
        },
    .rx_free_thresh = APP_DEFAULT_NIC_RX_FREE_THRESH,
    .rx_drop_en     = APP_DEFAULT_NIC_RX_DROP_EN,
};

static struct rte_eth_txconf tx_conf = {
    .tx_thresh =
        {
            .pthresh = APP_DEFAULT_NIC_TX_PTHRESH,
            .hthresh = APP_DEFAULT_NIC_TX_HTHRESH,
            .wthresh = APP_DEFAULT_NIC_TX_WTHRESH,
        },
    .tx_free_thresh = APP_DEFAULT_NIC_TX_FREE_THRESH,
    .tx_rs_thresh   = APP_DEFAULT_NIC_TX_RS_THRESH,
};

static void app_init_mbuf_pools (void) {
	unsigned socket, lcore;

	/* Init the buffer pools */
	for (socket = 0; socket < APP_MAX_SOCKETS; socket++) {
		char name[32];
		if (app_is_socket_used (socket) == 0) {
			continue;
		}

		snprintf (name, sizeof (name), "mbuf_pool_%u", socket);
		printf ("Creating the mbuf pool for socket %u ...\n", socket);
		app.pools[socket] = rte_mempool_create (name,
		                                        APP_DEFAULT_MEMPOOL_BUFFERS,
		                                        APP_DEFAULT_MBUF_SIZE,
		                                        APP_DEFAULT_MEMPOOL_CACHE_SIZE,
		                                        sizeof (struct rte_pktmbuf_pool_private),
		                                        rte_pktmbuf_pool_init,
		                                        NULL,
		                                        rte_pktmbuf_init,
		                                        NULL,
		                                        socket,
		                                        0);
		if (app.pools[socket] == NULL) {
			rte_panic ("Cannot create mbuf pool on socket %u\n", socket);
		}
	}

	for (lcore = 0; lcore < APP_MAX_LCORES; lcore++) {
		if (app.lcore_params[lcore].type == e_APP_LCORE_DISABLED) {
			continue;
		}

		socket                       = rte_lcore_to_socket_id (lcore);
		app.lcore_params[lcore].pool = app.pools[socket];
	}
}

static void app_init_rings_tx (void) {
	unsigned lcore;

	/* Initialize the rings for the TX side */
	for (lcore = 0; lcore < APP_MAX_LCORES; lcore++) {
		unsigned port;

		if (app.lcore_params[lcore].type != e_APP_LCORE_WORKER) {
			continue;
		}

		for (port = 0; port < APP_MAX_NIC_PORTS; port++) {
			uint32_t lcore_io;

			if (app_get_nic_tx_queues_per_port (port) == 0) {
				continue;
			}

			if (app_get_lcore_for_nic_tx ((uint8_t)port, 0, &lcore_io) < 0) {  // TODO check other queues
				rte_panic (
				    "Algorithmic error (no I/O core to handle TX of port %u "
				    "and queue 0)\n",
				    port);
			}
		}
	}
}

/* Check the link status of all ports in up to 9s, and print them finally */
static void check_all_ports_link_status (uint8_t port_num, uint32_t port_mask) {
#define CHECK_INTERVAL 100 /* 100ms */
#define MAX_CHECK_TIME 90  /* 9s (90 * 100ms) in total */
	uint8_t portid, count, all_ports_up, print_flag = 0;
	struct rte_eth_link link;
	uint32_t n_rx_queues, n_tx_queues;

	printf ("\nChecking link status");
	fflush (stdout);
	for (count = 0; count <= MAX_CHECK_TIME; count++) {
		all_ports_up = 1;
		for (portid = 0; portid < port_num; portid++) {
			if ((port_mask & (1 << portid)) == 0)
				continue;
			n_rx_queues = app_get_nic_rx_queues_per_port (portid);
			n_tx_queues = app_get_nic_tx_queues_per_port (portid);
			if ((n_rx_queues == 0) && (n_tx_queues == 0))
				continue;
			memset (&link, 0, sizeof (link));
			rte_eth_link_get_nowait (portid, &link);
			/* print link status if flag set */
			if (print_flag == 1) {
				if (link.link_status)
					printf (
					    "Port %d Link Up - speed %u "
					    "Mbps - %s\n",
					    (uint8_t)portid,
					    (unsigned)link.link_speed,
					    (link.link_duplex == ETH_LINK_FULL_DUPLEX) ? ("full-duplex") : ("half-duplex\n"));
				else {
					printf ("Port %d Link Down\n", (uint8_t)portid);
					//					portid--;
					rte_delay_ms (CHECK_INTERVAL * 2);
				}
				continue;
			}
			/* clear all_ports_up flag if any link down */
			if (link.link_status == 0) {
				all_ports_up = 0;
				break;
			}
		}
		/* after finally printing all link status, get out */
		if (print_flag == 1)
			break;

		if (all_ports_up == 0) {
			printf (".");
			fflush (stdout);
			rte_delay_ms (CHECK_INTERVAL);
		}

		/* set the print_flag if all ports up or timeout */
		if (all_ports_up == 1 || count == (MAX_CHECK_TIME - 1)) {
			print_flag = 1;
			printf ("done\n");
		}
	}
}

extern uint8_t icmppkt[];
extern uint8_t arppkt[];
extern unsigned sndpktlen;

static void app_init_nics (void) {
	unsigned socket;
	uint32_t lcore = 0;
	uint8_t port, queue;
	int ret;
	uint32_t n_rx_queues, n_tx_queues;

	// generate random etho
	// eth_random_addr(icmppkt+6);

	/* Init NIC ports and queues, then start the ports */
	for (port = 0; port < APP_MAX_NIC_PORTS; port++) {
		struct rte_mempool *pool;

		n_rx_queues = app_get_nic_rx_queues_per_port (port);
		n_tx_queues = app_get_nic_tx_queues_per_port (port);

		if ((n_rx_queues == 0) && (n_tx_queues == 0)) {
			continue;
		}

		/* Init port */
		printf ("Initializing NIC port %u ...\n", (unsigned)port);

		ret = rte_eth_dev_configure (port, (uint8_t)n_rx_queues, (uint8_t)n_tx_queues, &port_conf);
		if (ret < 0) {
			rte_panic ("Cannot init NIC port %u (%d)\n", (unsigned)port, ret);
		}
		rte_eth_promiscuous_enable (port);

		// set MAC
		struct ether_addr myaddr = {.addr_bytes = {0x00, 0x1b, 0x21, 0xad, 0xa9, 0x9c}};
		// get pci-id
		char name[RTE_ETH_NAME_MAX_LEN];
		rte_eth_dev_get_name_by_port (port, name);

		sscanf (name, "0000:%02hhx:%02hhx.%02hhx", myaddr.addr_bytes + 3, myaddr.addr_bytes + 4, myaddr.addr_bytes + 5);

		rte_eth_dev_mac_addr_add (port, &myaddr, 0);
		rte_eth_dev_default_mac_addr_set (port, &myaddr);

		/* Init RX queues */
		for (queue = 0; queue < APP_MAX_RX_QUEUES_PER_NIC_PORT; queue++) {
			if (app.nic_rx_queue_mask[port][queue] == 0) {
				continue;
			}

			app_get_lcore_for_nic_rx (port, queue, &lcore);
			socket = rte_lcore_to_socket_id (lcore);
			pool   = app.lcore_params[lcore].pool;

			printf ("Initializing NIC port %u RX queue %u ...\n", (unsigned)port, (unsigned)queue);
			ret = rte_eth_rx_queue_setup (port, queue, (uint16_t)app.nic_rx_ring_size, socket, &rx_conf, pool);
			if (ret < 0) {
				rte_panic ("Cannot init RX queue %u for port %u (%d)\n", (unsigned)queue, (unsigned)port, ret);
			}
		}

		/* Init TX queues */
		for (queue = 0; queue < APP_MAX_TX_QUEUES_PER_NIC_PORT; queue++) {
			if (app.nic_tx_queue_mask[port][queue] == 0) {
				continue;
			}

			app_get_lcore_for_nic_tx (port, queue, &lcore);
			socket = rte_lcore_to_socket_id (lcore);

			printf ("Initializing NIC port %u TX queue %u ...\n", (unsigned)port, (unsigned)queue);
			ret = rte_eth_tx_queue_setup (port, queue, (uint16_t)app.nic_tx_ring_size, socket, &tx_conf);
			if (ret < 0) {
				rte_panic ("Cannot init TX queue %u for port %u (%d)\n", (unsigned)queue, (unsigned)port, ret);
			}
		}

		/* Start port */
		ret = rte_eth_dev_start (port);
		if (ret < 0) {
			rte_panic ("Cannot start port %d (%d)\n", port, ret);
		}

		/************ FLOW TESTS ************/
		/*
		struct rte_flow_attr attr         = {0};
		attr.ingress                      = 1;
		struct rte_flow_item pattern[5]   = {0};
		struct rte_flow_action actions[5] = {0};
		// struct rte_flow_item_eth eth      = {0};
		struct rte_flow_action_queue queue = {.index = 0};
		struct rte_flow_item_vlan vlan     = {0};
		struct rte_flow *flow;
		struct rte_flow_error error;

		vlan.tci        = 2048;
		pattern[0].type = RTE_FLOW_ITEM_TYPE_ETH;
		pattern[1].type = RTE_FLOW_ITEM_TYPE_VLAN;
		pattern[1].spec = &vlan;
		pattern[2].type = RTE_FLOW_ITEM_TYPE_END;

		actions[0].type = RTE_FLOW_ACTION_TYPE_QUEUE;
		actions[0].conf = &queue;
		actions[1].type = RTE_FLOW_ACTION_TYPE_END;

		printf ("validating...\n");
		if (!rte_flow_validate (port, &attr, pattern, actions, &error)) {
		    printf ("creating...\n");
		    flow = rte_flow_create (port, &attr, pattern, actions, &error);
		    printf ("INSERTED FLOWRULE (%p)\n", (void *)flow);
		} else {
		    printf ("FLOWRULE FAILED: %s\n", error.message);
		}

		vlan.tci        = 2304;
		queue.index     = 1;
		pattern[0].type = RTE_FLOW_ITEM_TYPE_ETH;
		pattern[1].type = RTE_FLOW_ITEM_TYPE_VLAN;
		pattern[1].spec = &vlan;
		pattern[2].type = RTE_FLOW_ITEM_TYPE_END;

		actions[0].type = RTE_FLOW_ACTION_TYPE_QUEUE;
		actions[0].conf = &queue;
		actions[1].type = RTE_FLOW_ACTION_TYPE_END;

		printf ("validating...\n");
		if (!rte_flow_validate (port, &attr, pattern, actions, &error)) {
		    printf ("creating...\n");
		    flow = rte_flow_create (port, &attr, pattern, actions, &error);
		    printf ("INSERTED FLOWRULE (%p)\n", (void *)flow);
		} else {
		    printf ("FLOWRULE FAILED: %s\n", error.message);
		}

		pattern[0].type = RTE_FLOW_ITEM_TYPE_ETH;
		pattern[1].type = RTE_FLOW_ITEM_TYPE_END;

		actions[0].type = RTE_FLOW_ACTION_TYPE_DROP;
		actions[1].type = RTE_FLOW_ACTION_TYPE_END;

		printf ("validating...\n");
		if (!rte_flow_validate (port, &attr, pattern, actions, &error)) {
		    printf ("creating...\n");
		    flow = rte_flow_create (port, &attr, pattern, actions, &error);
		    printf ("INSERTED FLOWRULE (%p)\n", (void *)flow);
		} else {
		    printf ("FLOWRULE FAILED: %s\n", error.message);
		}
		*/
		/************************************/

		// get current mac addr
		rte_eth_macaddr_get (port, (struct ether_addr *)(icmppkt + 6));
		rte_eth_macaddr_get (port, (struct ether_addr *)(arppkt + 6));
		rte_eth_macaddr_get (port, (struct ether_addr *)(arppkt + 6 + 6 + 2 + 8 + VLAN_OFFSET));

		// change ip orig in arp
		arppkt[29 + VLAN_OFFSET] = 1;                                                    // arppkt[25];
		arppkt[30 + VLAN_OFFSET] = 2;                                                    // arppkt[26];
		arppkt[31 + VLAN_OFFSET] = arppkt[25 + VLAN_OFFSET] + arppkt[27 + VLAN_OFFSET];  // arppkt[27];

		printf ("ETHOrig (%d) set to: %hhX:%hhX:%hhX:%hhX:%hhX:%hhX\n",
		        port,
		        arppkt[6],
		        arppkt[7],
		        arppkt[8],
		        arppkt[9],
		        arppkt[10],
		        arppkt[11]);

		printf ("ETHDest (%d) set to: %hhX:%hhX:%hhX:%hhX:%hhX:%hhX\n",
		        port,
		        icmppkt[0],
		        icmppkt[1],
		        icmppkt[2],
		        icmppkt[3],
		        icmppkt[4],
		        icmppkt[5]);

		printf ("IPOrig  (%d) set to: %hhu.%hhu.%hhu.%hhu\n",
		        port,
		        arppkt[28 + VLAN_OFFSET],
		        arppkt[29 + VLAN_OFFSET],
		        arppkt[30 + VLAN_OFFSET],
		        arppkt[31 + VLAN_OFFSET]);

		printf ("IPDest  (%d) set to: %hhu.%hhu.%hhu.%hhu\n",
		        port,
		        arppkt[38 + VLAN_OFFSET],
		        arppkt[39 + VLAN_OFFSET],
		        arppkt[40 + VLAN_OFFSET],
		        arppkt[41 + VLAN_OFFSET]);

		// set IP Checksum
		struct ipv4_hdr *hdr = (struct ipv4_hdr *)(icmppkt + 6 + 6 + 2 + VLAN_OFFSET);
		hdr->total_length    = htons (sndpktlen - (14 + VLAN_OFFSET));
		hdr->hdr_checksum    = 0;
		hdr->hdr_checksum    = rte_ipv4_cksum (hdr);

		rte_eth_dev_stop (port);

		/* Start port */
		ret = rte_eth_dev_start (port);
		if (ret < 0) {
			rte_panic ("Cannot start port %d (%d)\n", port, ret);
		}
	}

	check_all_ports_link_status (APP_MAX_NIC_PORTS, (~0x0));
}

void app_init (void) {
	app_init_mbuf_pools ();
	// app_init_rings_rx ();
	app_init_rings_tx ();
	app_init_nics ();

	// HPTL
	hptl_config conf = {.clockspeed = 0, .precision = 8};
	hptl_init (&conf);

	printf ("Using HPTL %s.\n", hptl_VERSION);

	// AutoConf
	app_autoconf_init ();

	printf ("Initialization completed.\n");
}
