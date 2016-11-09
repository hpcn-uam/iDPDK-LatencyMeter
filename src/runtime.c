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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <sys/types.h>
#include <string.h>
#include <sys/queue.h>
#include <stdarg.h>
#include <errno.h>
#include <getopt.h>

#include <rte_common.h>
#include <rte_byteorder.h>
#include <rte_log.h>
#include <rte_memory.h>
#include <rte_memcpy.h>
#include <rte_memzone.h>
#include <rte_tailq.h>
#include <rte_eal.h>
#include <rte_per_lcore.h>
#include <rte_launch.h>
#include <rte_atomic.h>
#include <rte_cycles.h>
#include <rte_prefetch.h>
#include <rte_lcore.h>
#include <rte_per_lcore.h>
#include <rte_branch_prediction.h>
#include <rte_interrupts.h>
#include <rte_pci.h>
#include <rte_random.h>
#include <rte_debug.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_ring.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_ip.h>
#include <rte_tcp.h>
#include <rte_lpm.h>

#include "main.h"

#ifndef APP_LCORE_IO_FLUSH
#define APP_LCORE_IO_FLUSH 1000000
#endif

#ifndef APP_LCORE_WORKER_FLUSH
#define APP_LCORE_WORKER_FLUSH 1000000
#endif

#ifndef APP_STATS
#define APP_STATS 1000000
#endif

#define APP_IO_RX_DROP_ALL_PACKETS 1
#define APP_WORKER_DROP_ALL_PACKETS 0
#define APP_IO_TX_DROP_ALL_PACKETS 0

#ifndef APP_IO_RX_PREFETCH_ENABLE
#define APP_IO_RX_PREFETCH_ENABLE 1
#endif

#ifndef APP_WORKER_PREFETCH_ENABLE
#define APP_WORKER_PREFETCH_ENABLE 1
#endif

#ifndef APP_IO_TX_PREFETCH_ENABLE
#define APP_IO_TX_PREFETCH_ENABLE 1
#endif

#if APP_IO_RX_PREFETCH_ENABLE
#define APP_IO_RX_PREFETCH0(p) rte_prefetch0 (p)
#define APP_IO_RX_PREFETCH1(p) rte_prefetch1 (p)
#else
#define APP_IO_RX_PREFETCH0(p)
#define APP_IO_RX_PREFETCH1(p)
#endif

#if APP_WORKER_PREFETCH_ENABLE
#define APP_WORKER_PREFETCH0(p) rte_prefetch0 (p)
#define APP_WORKER_PREFETCH1(p) rte_prefetch1 (p)
#else
#define APP_WORKER_PREFETCH0(p)
#define APP_WORKER_PREFETCH1(p)
#endif

#if APP_IO_TX_PREFETCH_ENABLE
#define APP_IO_TX_PREFETCH0(p) rte_prefetch0 (p)
#define APP_IO_TX_PREFETCH1(p) rte_prefetch1 (p)
#else
#define APP_IO_TX_PREFETCH0(p)
#define APP_IO_TX_PREFETCH1(p)
#endif

#define icmpStart (6 + 6 + 2 + 5 * 4)

uint8_t arppkt[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x84, 0x2b, 0x2b, 0x6b, 0x4d, 0x61,
                    0x08, 0x06, 0x00, 0x01, 0x08, 0x00, 0x06, 0x04, 0x00, 0x02, 0x84, 0x2b,
                    0x2b, 0x6b, 0x4d, 0x61, 0xc0, 0xa8, 0x00, 0x78, 0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0xc0, 0xa8, 0x00, 0x78, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
int arppktlen = sizeof (arppkt);

uint8_t icmppkt[] = {0x00, 0x1b, 0x21, 0xad, 0xa9, 0x9c, 0x14, 0xdd, 0xa9, 0xd2, 0xef, 0x57, 0x08,
                     0x00, 0x45, 0x00, 0x00, 0x54, 0x51, 0x36, 0x40, 0x00, 0x40, 0x01, 0x6b, 0xee,
                     0x96, 0xf4, 0x3a, 0x72, 0xd8, 0x3a, 0xd3, 0xe3, 0x08, 0x00, 0x00, 0x00, 0x66,
                     0x02, 0x00, 0x1a, 0x67, 0x72, 0x97, 0x57, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff,
                     0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16,
                     0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23,
                     0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30,
                     0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37};

const uint64_t tspacketId = 0xCACAFEA;

unsigned icmppktlen = sizeof (icmppkt);
unsigned sndpktlen  = sizeof (icmppkt);

int doChecksum       = 0;
int autoIncNum       = 0;
int selectiveTS      = 0;  // Selective Timestamping
int hwTimeTest       = 0;
int bandWidthMeasure = 0;
uint64_t trainLen    = 0;
uint64_t trainSleep  = 0;                          // ns
uint64_t waitTime    = 10 * 1000 * 1000 * 1000UL;  // ns

int continueRX = 1;

struct pktLatencyStat *latencyStats = NULL;

//#define QUEUE_STATS

static inline void app_lcore_io_rx (struct app_lcore_params_io *lp, uint32_t bsz_rd) {
	uint32_t i;

	static uint32_t counter = 0;

	for (i = 0; i < lp->rx.n_nic_queues; i++) {
		uint8_t port  = lp->rx.nic_queues[i].port;
		uint8_t queue = lp->rx.nic_queues[i].queue;
		uint32_t n_mbufs, j;

		n_mbufs = rte_eth_rx_burst (port, queue, lp->rx.mbuf_in.array, (uint16_t)bsz_rd);

		if (unlikely (continueRX == 0)) {
			uint32_t k;
			uint64_t totalBytes = 0;
			hptl_t firstTime = 0, lastTime = 0;
			uint64_t ignored           = 0;
			uint64_t sumLatency        = 0;
			struct timespec hwRelation = {0, 0};
			struct timespec hwDelta;

			for (k = 0; k < trainLen; k++) {
				if (latencyStats[k].recved) {
					uint64_t currentLatency = latencyStats[k].recvTime - latencyStats[k].sentTime;
					printf ("%d: Latency %lu ns", k + 1, currentLatency);
					sumLatency += currentLatency;
					if (hwTimeTest) {
						printf (" hwTime %lu.%lu", latencyStats[k].hwTime.tv_sec,
						        latencyStats[k].hwTime.tv_nsec);
						if (lastTime != 0) {
							printf (" hwDeltaLatency %lu.%lu",
							        latencyStats[k].hwTime.tv_sec - hwDelta.tv_sec,
							        latencyStats[k].hwTime.tv_nsec - hwDelta.tv_nsec);
						}
						hwDelta = latencyStats[k].hwTime;
					}
					if (lastTime != 0) {
						printf (" insta-BandWidth %lf Gbps",
						        (latencyStats[k].pktLen / 1000000000.) /
						            (((double)latencyStats[k].recvTime - lastTime) / 1000000000.));
					} else {
						if (hwTimeTest) {
							hwRelation = hptl_timespec (latencyStats[k].recvTime);
							hwRelation.tv_sec -= latencyStats[k].hwTime.tv_sec;
							hwRelation.tv_nsec -= latencyStats[k].hwTime.tv_nsec;
						}
						firstTime = latencyStats[k].recvTime;
					}
					lastTime = latencyStats[k].recvTime;
					totalBytes += latencyStats[k].pktLen;
					printf ("\n");
				} else {
					printf ("%d: Recved but ignored\n", k + 1);
					ignored++;
				}
			}
			printf ("Mean-BandWidth %lf Gbps\n",
			        (totalBytes / 1000000000.) / (((double)lastTime - firstTime) / 1000000000.));
			printf ("Mean-Latency %lf ns\n", sumLatency / (((double)trainLen - ignored)));

			// Ignored / Dropped stats
			if (ignored > 0) {
				printf ("%ld Packets ignored\n", ignored);
			}
			struct rte_eth_stats stats;
			rte_eth_stats_get (port, &stats);
			if (stats.ierrors > 0) {
				printf ("%ld Packets errored/dropped\n", stats.ierrors);
			}
			exit (0);
		}

		if (unlikely (n_mbufs == 0)) {
			continue;
		}

		if (trainLen &&
		    (*(uint16_t *)(rte_ctrlmbuf_data (lp->rx.mbuf_in.array[n_mbufs - 1]) + icmpStart + 2 +
		                   2)) == (*(uint16_t *)(icmppkt + icmpStart + 2 + 2))) {
			// Add counter #recvPkts-1, so the data is saved in the structure as "the last packet of
			// the bulk, instead of the first one"
			counter += n_mbufs - 1;

			// Latency ounters
			latencyStats[counter].recvTime = hptl_get ();
			latencyStats[counter].sentTime =
			    (*(hptl_t *)(rte_ctrlmbuf_data (lp->rx.mbuf_in.array[n_mbufs - 1]) +
			                 rte_ctrlmbuf_len (lp->rx.mbuf_in.array[n_mbufs - 1]) - 8));
			latencyStats[counter].pktLen = rte_ctrlmbuf_len (lp->rx.mbuf_in.array[n_mbufs - 1]);
			latencyStats[counter].recved = 1;
			if (hwTimeTest) {
				const float fpgaConvRate = 4.294967296;
				latencyStats[counter].hwTime.tv_sec =
				    ntohl (
				        *(uint32_t *)(rte_ctrlmbuf_data (lp->rx.mbuf_in.array[n_mbufs - 1]) + 50)) /
				    fpgaConvRate;
				latencyStats[counter].hwTime.tv_nsec =
				    ntohl (
				        *(uint32_t *)(rte_ctrlmbuf_data (lp->rx.mbuf_in.array[n_mbufs - 1]) + 54)) /
				    fpgaConvRate;
			}

			// end if all packets have been recved
			counter++;
			if (counter == trainLen) continueRX = 0;
		}

#if APP_IO_RX_DROP_ALL_PACKETS
		for (j = 0; j < n_mbufs; j++) {
			struct rte_mbuf *pkt = lp->rx.mbuf_in.array[j];
			rte_pktmbuf_free (pkt);
		}

		continue;
#endif
	}
}

static inline void app_lcore_io_rx_bw (struct app_lcore_params_io *lp, uint32_t bsz_rd) {
	uint32_t i;

	for (i = 0; i < lp->rx.n_nic_queues; i++) {
		uint8_t port  = lp->rx.nic_queues[i].port;
		uint8_t queue = lp->rx.nic_queues[i].queue;
		uint32_t n_mbufs, j;

		n_mbufs = rte_eth_rx_burst (port, queue, lp->rx.mbuf_in.array, (uint16_t)bsz_rd);

		if (unlikely (n_mbufs == 0)) {
			continue;
		}

#if APP_STATS
		lp->rx.nic_queues_iters[i]++;
		lp->rx.nic_queues_count[i] += n_mbufs;
		if (unlikely (lp->rx.nic_queues_iters[i] == APP_STATS * 10)) {
			struct rte_eth_stats stats;
			struct timeval start_ewr, end_ewr;

			rte_eth_stats_get (port, &stats);
			gettimeofday (&lp->rx.end_ewr, NULL);

			start_ewr = lp->rx.start_ewr;
			end_ewr   = lp->rx.end_ewr;

#ifdef QUEUE_STATS
			if (queue == 0) {
#endif
				printf ("NIC port %u: drop ratio = %.2f (%u/%u) speed: %lf Gbps (%.1lf pkts/s)\n",
				        (unsigned)port,
				        (double)stats.ierrors / (double)(stats.ierrors + stats.ipackets),
				        (uint32_t)stats.ipackets, (uint32_t)stats.ierrors,
				        (((stats.ibytes) + stats.ipackets * (/*4crc+8prelud+12ifg*/ (8 + 12))) /
				         (((end_ewr.tv_sec * 1000000. + end_ewr.tv_usec) -
				           (start_ewr.tv_sec * 1000000. + start_ewr.tv_usec)) /
				          1000000.)) /
				            (1000 * 1000 * 1000. / 8.),
				        stats.ipackets / (((end_ewr.tv_sec * 1000000. + end_ewr.tv_usec) -
				                           (start_ewr.tv_sec * 1000000. + start_ewr.tv_usec)) /
				                          1000000.));
#ifdef QUEUE_STATS
			}
			printf (
			    "NIC port %u:%u: drop ratio = %.2f (%u/%u) speed %.1lf pkts/s\n", (unsigned)port,
			    queue, (double)stats.ierrors / (double)(stats.ierrors + lp->rx.nic_queues_count[i]),
			    (uint32_t)lp->rx.nic_queues_count[i], (uint32_t)stats.ierrors,
			    lp->rx.nic_queues_count[i] / (((end_ewr.tv_sec * 1000000. + end_ewr.tv_usec) -
			                                   (start_ewr.tv_sec * 1000000. + start_ewr.tv_usec)) /
			                                  1000000.));
#endif
			lp->rx.nic_queues_iters[i] = 0;
			lp->rx.nic_queues_count[i] = 0;

#ifdef QUEUE_STATS
			if (queue == 0)
#endif
				rte_eth_stats_reset (port);

#ifdef QUEUE_STATS
			if (queue == 0)
#endif
				lp->rx.start_ewr = end_ewr;  // Updating start
		}
#endif

#if APP_IO_RX_DROP_ALL_PACKETS
		for (j = 0; j < n_mbufs; j++) {
			struct rte_mbuf *pkt = lp->rx.mbuf_in.array[j];
			rte_pktmbuf_free (pkt);
		}

		continue;
#endif
	}
}

static inline void app_lcore_io_rx_sts (struct app_lcore_params_io *lp, uint32_t bsz_rd) {
	uint32_t i;

	static uint32_t counter = 0;

	for (i = 0; i < lp->rx.n_nic_queues; i++) {
		uint8_t port  = lp->rx.nic_queues[i].port;
		uint8_t queue = lp->rx.nic_queues[i].queue;
		uint32_t n_mbufs, i, j;

		n_mbufs = rte_eth_rx_burst (port, queue, lp->rx.mbuf_in.array, (uint16_t)bsz_rd);

		if (unlikely (continueRX == 0)) {
			uint32_t k;
			uint64_t totalBytes = 0;
			hptl_t firstTime = 0, lastTime = 0;
			uint64_t ignored           = 0;
			uint64_t sumLatency        = 0;
			struct timespec hwRelation = {0, 0};
			struct timespec hwDelta;

			for (k = 0; k < trainLen; k++) {
				if (latencyStats[k].recved) {
					uint64_t currentLatency = latencyStats[k].recvTime - latencyStats[k].sentTime;
					printf ("%d: Latency %lu ns", k + 1, currentLatency);
					sumLatency += currentLatency;
					if (hwTimeTest) {
						printf (" hwTime %lu.%lu", latencyStats[k].hwTime.tv_sec,
						        latencyStats[k].hwTime.tv_nsec);
						if (lastTime != 0) {
							printf (" hwDeltaLatency %lu.%lu",
							        latencyStats[k].hwTime.tv_sec - hwDelta.tv_sec,
							        latencyStats[k].hwTime.tv_nsec - hwDelta.tv_nsec);
						}
						hwDelta = latencyStats[k].hwTime;
					}
					if (lastTime != 0) {
						printf (" insta-BandWidth %lf Gbps",
						        (latencyStats[k].pktLen / 1000000000.) /
						            (((double)latencyStats[k].recvTime - lastTime) / 1000000000.));
					} else {
						if (hwTimeTest) {
							hwRelation = hptl_timespec (latencyStats[k].recvTime);
							hwRelation.tv_sec -= latencyStats[k].hwTime.tv_sec;
							hwRelation.tv_nsec -= latencyStats[k].hwTime.tv_nsec;
						}
						firstTime = latencyStats[k].recvTime;
					}
					lastTime = latencyStats[k].recvTime;
					totalBytes += latencyStats[k].pktLen;
					printf ("\n");
				} else {
					printf ("%d: Recved but ignored\n", k + 1);
					ignored++;
				}
			}
			printf ("Mean-BandWidth %lf Gbps\n",
			        (totalBytes / 1000000000.) / (((double)lastTime - firstTime) / 1000000000.));
			printf ("Mean-Latency %lf ns\n", sumLatency / (((double)trainLen - ignored)));

			// Ignored / Dropped stats
			if (ignored > 0) {
				printf ("%ld Packets ignored\n", ignored);
			}
			struct rte_eth_stats stats;
			rte_eth_stats_get (port, &stats);
			if (stats.ierrors > 0) {
				printf ("%ld Packets errored/dropped\n", stats.ierrors);
			}
			exit (0);
		}

		if (unlikely (n_mbufs == 0)) {
			continue;
		}

		for (i = 0; i < n_mbufs; i++) {
			uint8_t *data = (uint8_t *)rte_ctrlmbuf_data (lp->rx.mbuf_in.array[i]);
			uint32_t len  = rte_ctrlmbuf_len (lp->rx.mbuf_in.array[i]);

			if (*(uint64_t *)(data + len - 16) == tspacketId) {  // paquete marcado
				if (trainLen && (*(uint16_t *)(data + icmpStart + 2 + 2) ==
				                 *(uint16_t *)(icmppkt + icmpStart + 2 + 2))) {
					// Add counter #recvPkts-1, so the data is saved in the structure as "the last
					// packet of the bulk, instead of the first one"
					counter += i;

					// Latency ounters
					latencyStats[counter].recvTime = hptl_get ();
					latencyStats[counter].sentTime = (*(hptl_t *)(data + len - 8));
					latencyStats[counter].pktLen   = len;
					latencyStats[counter].recved = 1;
					if (hwTimeTest) {
						const float fpgaConvRate = 4.294967296;
						latencyStats[counter].hwTime.tv_sec =
						    ntohl (*(uint32_t *)(data + 50)) / fpgaConvRate;
						latencyStats[counter].hwTime.tv_nsec =
						    ntohl (*(uint32_t *)(data + 54)) / fpgaConvRate;
					}

					// end if all packets have been recved
					counter++;
					if (counter == trainLen) continueRX = 0;
				}
			}
		}

		for (j = 0; j < n_mbufs; j++) {
			struct rte_mbuf *pkt = lp->rx.mbuf_in.array[j];
			rte_pktmbuf_free (pkt);
		}
	}
}

static inline void app_lcore_io_tx (struct app_lcore_params_io *lp) {
	uint32_t i;

	for (i = 0; i < lp->tx.n_nic_queues; i++) {
		uint8_t port  = lp->tx.nic_queues[i].port;
		uint8_t queue = lp->tx.nic_queues[i].queue;
		uint32_t n_mbufs, n_pkts;

		n_mbufs = 1;

		struct rte_mbuf *tmpbuf = rte_ctrlmbuf_alloc (app.pools[0]);
		if (!tmpbuf) {
			continue;
		}

		tmpbuf->pkt_len  = sndpktlen;
		tmpbuf->data_len = sndpktlen;
		tmpbuf->port     = port;

		if (autoIncNum) {
			(*((uint16_t *)(icmppkt + icmpStart + 2 + 2 + 2)))++;
		}

		memcpy (rte_ctrlmbuf_data (tmpbuf), icmppkt, icmppktlen - 8);
		*((hptl_t *)(rte_ctrlmbuf_data (tmpbuf) + sndpktlen - 8)) = hptl_get ();

		if (doChecksum) {
			uint16_t cksum;
			cksum = rte_raw_cksum (rte_ctrlmbuf_data (tmpbuf) + icmpStart, sndpktlen - icmpStart);
			*((uint16_t *)(rte_ctrlmbuf_data (tmpbuf) + icmpStart + 2)) =
			    ((cksum == 0xffff) ? cksum : ~cksum);
		}

		n_pkts = rte_eth_tx_burst (port, queue, &tmpbuf, n_mbufs);

		if (trainSleep) {
			hptl_waitns (trainSleep);
		}

		if (unlikely (n_pkts < n_mbufs)) {
			rte_ctrlmbuf_free (tmpbuf);
		} else {
			lp->tx.mbuf_out[port].n_mbufs++;
			if (trainLen && lp->tx.mbuf_out[port].n_mbufs >= trainLen) {
				hptl_waitns (waitTime);
				continueRX = 0;
				hptl_waitns (waitTime);
				exit (1);
			}
		}
	}
}

static inline void app_lcore_io_tx_bw (struct app_lcore_params_io *lp, uint32_t bsz_wr) {
	uint32_t i;
	uint32_t k;

	for (i = 0; i < lp->tx.n_nic_queues; i++) {
		uint8_t port  = lp->tx.nic_queues[i].port;
		uint8_t queue = lp->tx.nic_queues[i].queue;
		uint32_t n_mbufs, n_pkts;
		n_mbufs = bsz_wr;

		for (k = 0; k < n_mbufs; k++) {
			lp->tx.mbuf_out[port].array[k] = rte_ctrlmbuf_alloc (app.pools[0]);
			if (lp->tx.mbuf_out[port].array[k] == NULL) {
				n_mbufs = k;
				break;
			}

			lp->tx.mbuf_out[port].array[k]->pkt_len  = sndpktlen;
			lp->tx.mbuf_out[port].array[k]->data_len = sndpktlen;
			lp->tx.mbuf_out[port].array[k]->port     = port;

			memcpy (rte_ctrlmbuf_data (lp->tx.mbuf_out[port].array[k]), icmppkt, icmppktlen);
		}

		n_pkts = rte_eth_tx_burst (port, queue, lp->tx.mbuf_out[port].array, n_mbufs);

#if APP_STATS
		lp->tx.nic_queues_iters[i]++;
		lp->tx.nic_queues_count[i] += n_mbufs;
		if (unlikely (lp->tx.nic_queues_iters[i] == APP_STATS)) {
			struct rte_eth_stats stats;
			struct timeval start_ewr, end_ewr;

			rte_eth_stats_get (port, &stats);
			gettimeofday (&lp->tx.end_ewr, NULL);

			start_ewr = lp->tx.start_ewr;
			end_ewr   = lp->tx.end_ewr;

			if (queue == 0) {
				printf (
				    "NIC TX port %u: drop ratio = %.2f (%u/%u) usefull-speed: %lf Gbps, "
				    "link-speed: %lf Gbps (%.1lf pkts/s)\n",
				    (unsigned)port,
				    (double)stats.oerrors / (double)(stats.oerrors + stats.opackets),
				    (uint32_t)stats.opackets, (uint32_t)stats.oerrors,
				    (stats.obytes / (((end_ewr.tv_sec * 1000000. + end_ewr.tv_usec) -
				                      (start_ewr.tv_sec * 1000000. + start_ewr.tv_usec)) /
				                     1000000.)) /
				        (1000 * 1000 * 1000. / 8.),
				    (((stats.obytes) + stats.opackets * (/*4crc+8prelud+12ifg*/ (8 + 12))) /
				     (((end_ewr.tv_sec * 1000000. + end_ewr.tv_usec) -
				       (start_ewr.tv_sec * 1000000. + start_ewr.tv_usec)) /
				      1000000.)) /
				        (1000 * 1000 * 1000. / 8.),
				    stats.opackets / (((end_ewr.tv_sec * 1000000. + end_ewr.tv_usec) -
				                       (start_ewr.tv_sec * 1000000. + start_ewr.tv_usec)) /
				                      1000000.));

				rte_eth_stats_reset (port);
				lp->tx.start_ewr = end_ewr;  // Updating start
			}

			lp->tx.nic_queues_iters[i] = 0;
			lp->tx.nic_queues_count[i] = 0;
		}
#endif

		if (unlikely (n_pkts < n_mbufs)) {
			for (k = n_pkts; k < n_mbufs; k++) {
				struct rte_mbuf *pkt_to_free = lp->tx.mbuf_out[port].array[k];
				rte_ctrlmbuf_free (pkt_to_free);
			}
		}
	}
}

static inline void app_lcore_io_tx_sts (struct app_lcore_params_io *lp, uint32_t bsz_wr) {
	uint32_t i;
	uint32_t k;

	for (i = 0; i < lp->tx.n_nic_queues; i++) {
		uint8_t port  = lp->tx.nic_queues[i].port;
		uint8_t queue = lp->tx.nic_queues[i].queue;
		uint32_t n_mbufs, n_pkts;
		n_mbufs = bsz_wr;

		for (k = 0; k < n_mbufs; k++) {
			lp->tx.mbuf_out[port].array[k] = rte_ctrlmbuf_alloc (app.pools[0]);
			if (lp->tx.mbuf_out[port].array[k] == NULL) {
				n_mbufs = k;
				break;
			}

			lp->tx.mbuf_out[port].array[k]->pkt_len  = sndpktlen;
			lp->tx.mbuf_out[port].array[k]->data_len = sndpktlen;
			lp->tx.mbuf_out[port].array[k]->port     = port;

			if ((k == n_mbufs - 1) && autoIncNum) {
				(*((uint16_t *)(icmppkt + icmpStart + 2 + 2 + 2)))++;
			}

			memcpy (rte_ctrlmbuf_data (lp->tx.mbuf_out[port].array[k]), icmppkt, icmppktlen);

			if (k == n_mbufs - 1) {
				*((uint64_t *)(rte_ctrlmbuf_data (lp->tx.mbuf_out[port].array[k]) + sndpktlen -
				               16)) = tspacketId;
				*((hptl_t *)(rte_ctrlmbuf_data (lp->tx.mbuf_out[port].array[k]) + sndpktlen - 8)) =
				    hptl_get ();

				if (doChecksum) {
					uint16_t cksum;
					cksum = rte_raw_cksum (
					    rte_ctrlmbuf_data (lp->tx.mbuf_out[port].array[k]) + icmpStart,
					    sndpktlen - icmpStart);
					*((uint16_t *)(rte_ctrlmbuf_data (lp->tx.mbuf_out[port].array[k]) + icmpStart +
					               2)) = ((cksum == 0xffff) ? cksum : ~cksum);
				}
			}
		}

		n_pkts = rte_eth_tx_burst (port, queue, lp->tx.mbuf_out[port].array, n_mbufs);

#if APP_STATS
		lp->tx.nic_queues_iters[i]++;
		lp->tx.nic_queues_count[i] += n_mbufs;
		if (unlikely (lp->tx.nic_queues_iters[i] == APP_STATS)) {
			struct rte_eth_stats stats;
			struct timeval start_ewr, end_ewr;

			rte_eth_stats_get (port, &stats);
			gettimeofday (&lp->tx.end_ewr, NULL);

			start_ewr = lp->tx.start_ewr;
			end_ewr   = lp->tx.end_ewr;

			if (queue == 0) {
				printf (
				    "NIC TX port %u: drop ratio = %.2f (%u/%u) usefull-speed: %lf Gbps, "
				    "link-speed: %lf Gbps (%.1lf pkts/s)\n",
				    (unsigned)port,
				    (double)stats.oerrors / (double)(stats.oerrors + stats.opackets),
				    (uint32_t)stats.opackets, (uint32_t)stats.oerrors,
				    (stats.obytes / (((end_ewr.tv_sec * 1000000. + end_ewr.tv_usec) -
				                      (start_ewr.tv_sec * 1000000. + start_ewr.tv_usec)) /
				                     1000000.)) /
				        (1000 * 1000 * 1000. / 8.),
				    (((stats.obytes) + stats.opackets * (/*4crc+8prelud+12ifg*/ (8 + 12))) /
				     (((end_ewr.tv_sec * 1000000. + end_ewr.tv_usec) -
				       (start_ewr.tv_sec * 1000000. + start_ewr.tv_usec)) /
				      1000000.)) /
				        (1000 * 1000 * 1000. / 8.),
				    stats.opackets / (((end_ewr.tv_sec * 1000000. + end_ewr.tv_usec) -
				                       (start_ewr.tv_sec * 1000000. + start_ewr.tv_usec)) /
				                      1000000.));

				rte_eth_stats_reset (port);
				lp->tx.start_ewr = end_ewr;  // Updating start
			}

			lp->tx.nic_queues_iters[i] = 0;
			lp->tx.nic_queues_count[i] = 0;
		}
#endif

		if (unlikely (n_pkts < n_mbufs)) {
			// printf ("Ts packet unsended\n");
			for (k = n_pkts; k < n_mbufs; k++) {
				struct rte_mbuf *pkt_to_free = lp->tx.mbuf_out[port].array[k];
				rte_ctrlmbuf_free (pkt_to_free);
			}
		}
	}
}

static void app_lcore_arp_tx_gratuitous (struct app_lcore_params_io *lp) {
	uint32_t i;
	for (i = 0; i < lp->tx.n_nic_queues; i++) {
		uint8_t port  = lp->tx.nic_queues[i].port;
		uint8_t queue = lp->tx.nic_queues[i].queue;

		struct rte_mbuf *tmpbuf = rte_ctrlmbuf_alloc (app.pools[0]);
		if (!tmpbuf) {
			puts ("Error creating gratuitous ARP");
			exit (-1);
		}

		tmpbuf->pkt_len  = arppktlen;
		tmpbuf->data_len = arppktlen;
		tmpbuf->port     = port;

		memcpy (rte_ctrlmbuf_data (tmpbuf), arppkt, arppktlen);

		rte_eth_macaddr_get (port, (struct ether_addr *)(rte_ctrlmbuf_data (tmpbuf) + 6));
		rte_eth_macaddr_get (port,
		                     (struct ether_addr *)(rte_ctrlmbuf_data (tmpbuf) + 6 + 6 + 2 + 8));
		memcpy (rte_ctrlmbuf_data (tmpbuf) + 6 + 6 + 2 + 14, icmppkt + 6 + 6 + 2 + 4 * 4, 4);

		if (!rte_eth_tx_burst (port, queue, &tmpbuf, 1)) {
			puts ("Error sending gratuitous ARP");
			exit (-1);
		}
	}
}

static void app_lcore_main_loop_io (void) {
	uint32_t lcore                 = rte_lcore_id ();
	struct app_lcore_params_io *lp = &app.lcore_params[lcore].io;
	// uint32_t n_workers = app_get_lcores_worker();
	uint64_t i = 0;

	uint32_t bsz_rx_rd = app.burst_size_io_rx_read;
	// uint32_t bsz_rx_wr = app.burst_size_io_rx_write;

	// uint8_t pos_lb = app.pos_lb;

	app_lcore_arp_tx_gratuitous (lp);

	if (bandWidthMeasure) {  // only measure bw
		for (;;) {
			if (likely (lp->rx.n_nic_queues > 0)) {
				app_lcore_io_rx_bw (lp, bsz_rx_rd);
			}

			if (likely (lp->tx.n_nic_queues > 0)) {
				app_lcore_io_tx_bw (lp, app.burst_size_io_tx_write);
			}

			i++;
		}
	} else if (selectiveTS) {  // measure bw & latency, performing a selective Timestamping
		for (;;) {
			if (likely (lp->rx.n_nic_queues > 0)) {
				app_lcore_io_rx_sts (lp, bsz_rx_rd);
			}

			if (likely (lp->tx.n_nic_queues > 0)) {
				app_lcore_io_tx_sts (lp, app.burst_size_io_tx_write);
			}

			i++;
		}
	} else {  // measure bw & latency, priorizing latency
		for (;;) {
			if (likely (lp->rx.n_nic_queues > 0)) {
				app_lcore_io_rx (lp, bsz_rx_rd);
			}

			if (likely (lp->tx.n_nic_queues > 0)) {
				app_lcore_io_tx (lp);
			}

			i++;
		}
	}
}

int app_lcore_main_loop (__attribute__ ((unused)) void *arg) {
	struct app_lcore_params *lp;
	unsigned lcore;

	lcore = rte_lcore_id ();
	lp    = &app.lcore_params[lcore];

	if (lp->type == e_APP_LCORE_IO) {
		printf ("Logical core %u (I/O) main loop.\n", lcore);
		app_lcore_main_loop_io ();
	}

	return 0;
}
