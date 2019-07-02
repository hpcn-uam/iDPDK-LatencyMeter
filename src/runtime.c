/*- *   BSD LICENSE *
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
#include <rte_tailq.h>
#include <rte_tcp.h>

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

#ifdef APP_LIMITBW
uint64_t bwlimitNSwait = 0;
#endif

#define icmpStart (6 + 6 + 2 + 5 * 4 + VLAN_OFFSET)

uint8_t arppkt[] = {0xff, 0xff, 0xff, 0xff,    0xff, 0xff, 0xac, 0x1f, 0x6b, 0x8a, 0x06, 0xe0,
#if VLAN_ID
                    0x81, 0x00, 0x00, VLAN_ID,
#endif
                    0x08, 0x06, 0x00, 0x01,    0x08, 0x00, 0x06, 0x04, 0x00, 0x02, 0xac, 0x1f, 0x6b, 0x8a, 0x06,
                    0xe0, 0x0a, 0xa8, 0x00,    0x78, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0xa8, 0x00, 0x78,
#if VLAN_ID
#else
                    0x00, 0x00, 0x00, 0x00,
#endif
                    0x00, 0x00, 0x00, 0x00,    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
int arppktlen = sizeof (arppkt);

//:AC:1F:6B:8A:6:E0
// 0x00, 0x1B, 0x21, 0xd8, 0x00, 0x01
uint8_t icmppkt[] = {0x00, 0x1B, 0x21, 0x19,    0x00, 0x01, 0xac, 0x1f, 0x6b, 0x8a, 0x06, 0xe1,
#if VLAN_ID
                     0x81, 0x00, 0x00, VLAN_ID,
#endif
                     0x08, 0x00, 0x45, 0x00,    0x00, 0x54, 0x51, 0x36, 0x00, 0x00, 0x40, 0x01, 0x6b, 0xee, 0x96,
                     0xf4, 0x3a, 0x72, 0xd8,    0x3a, 0xd3, 0xe3, 0x08, 0x00, 0x00, 0x00, 0x66, 0x02, 0x00, 0x1a,
                     0x67, 0x72, 0x97, 0x57,    0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                     0xff, 0x10, 0x11, 0x12,    0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d,
                     0x1e, 0x1f, 0x20, 0x21,    0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c,
                     0x2d, 0x2e, 0x2f, 0x30,    0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37};

#define TSIDTYPE uint16_t
const uint32_t tspacketId = 0xCACACACA;
const uint32_t tsoffset   = 40;

const unsigned icmppktlen     = sizeof (icmppkt);
const unsigned icmpidoffset   = 36 + VLAN_OFFSET;
const unsigned icmpcntroffset = 38 + VLAN_OFFSET;

unsigned sndpktlen  = sizeof (icmppkt);
unsigned idoffset   = 36 + VLAN_OFFSET;  // = icmpidoffset
unsigned cntroffset = 38 + VLAN_OFFSET;  // = icmpcntroffset

// fpga timer conversion proportion
const float fpgaConvRate = 4.294967296;

// counter used in TX pkts
static unsigned pktcounter  = 0;
static unsigned bytecounter = 0;

int doChecksum             = 0;
int autoIncNum             = 0;
int selectiveTS            = 0;  // Selective Timestamping
int hwTimeTest             = 0;
int bandWidthMeasure       = 0;
int bandWidthMeasureActive = 0;
uint64_t trainLen          = 0;
uint64_t trainSleep        = 0;  // ns
uint64_t trainFriends      = 0;
uint64_t waitTime          = 10 * 1000 * 1000 * 1000UL;  // ns

// Autoconfigure
int doloop = 1;

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
						// fpga time conversion
						uint64_t fpgatime =
						    ntohl (latencyStats[k].hwTime.tv_sec) * 1000000000 + ntohl (latencyStats[k].hwTime.tv_nsec);
						fpgatime /= fpgaConvRate;
						latencyStats[k].hwTime.tv_sec  = fpgatime / 1000000000;
						latencyStats[k].hwTime.tv_nsec = fpgatime % 1000000000;

						printf (" hwTime %lu.%lu", latencyStats[k].hwTime.tv_sec, latencyStats[k].hwTime.tv_nsec);
						if (lastTime != 0) {
							printf (" hwDeltaLatency %lu.%lu",
							        latencyStats[k].hwTime.tv_sec - hwDelta.tv_sec,
							        latencyStats[k].hwTime.tv_nsec - hwDelta.tv_nsec);
						}
						hwDelta = latencyStats[k].hwTime;
					}
					if (lastTime != 0) {
						printf (" insta-BandWidth %lf Gbps",
						        (latencyStats[k].pktLen * 8 / 1000000000.) /
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
			        (totalBytes * 8 / 1000000000.) / (((double)lastTime - firstTime) / 1000000000.));
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

		if (trainLen && (*(uint16_t *)(rte_ctrlmbuf_data (lp->rx.mbuf_in.array[n_mbufs - 1]) + idoffset)) ==
		                    (*(uint16_t *)(icmppkt + idoffset))) {
			// Add counter #recvPkts-1, so the data is saved in the structure as "the last packet of
			// the bulk, instead of the first one"
			counter += n_mbufs - 1;

			// Latency ounters
			latencyStats[counter].recvTime = hptl_get ();
			latencyStats[counter].sentTime =
			    *(hptl_t *)(rte_ctrlmbuf_data (lp->rx.mbuf_in.array[n_mbufs - 1]) + tsoffset);
			latencyStats[counter].pktLen = rte_ctrlmbuf_len (lp->rx.mbuf_in.array[n_mbufs - 1]);
			latencyStats[counter].recved = 1;
			if (hwTimeTest) {
				latencyStats[counter].hwTime.tv_sec =
				    *(uint32_t *)(rte_ctrlmbuf_data (lp->rx.mbuf_in.array[n_mbufs - 1]) + 50 + VLAN_OFFSET);
				latencyStats[counter].hwTime.tv_nsec =
				    *(uint32_t *)(rte_ctrlmbuf_data (lp->rx.mbuf_in.array[n_mbufs - 1]) + 54 + VLAN_OFFSET);
			}

			// end if all packets have been recved
			counter++;
			if (counter == trainLen)
				continueRX = 0;
		}

		// Drop all packets
		for (j = 0; j < n_mbufs; j++) {
			struct rte_mbuf *pkt = lp->rx.mbuf_in.array[j];
			rte_pktmbuf_free (pkt);
		}
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

		if (unlikely (lp->rx.nic_queues_iters[i] >= APP_STATS)) {
			struct rte_eth_stats stats;
			struct timeval start_ewr, end_ewr;

			if (queue == 0) {
				rte_eth_stats_get (port, &stats);
				gettimeofday (&lp->rx.end_ewr, NULL);

				start_ewr = lp->rx.start_ewr;
				end_ewr   = lp->rx.end_ewr;

				rte_eth_stats_reset (port);
				if (lp->rx.start_ewr.tv_sec == 0) {
					rte_eth_stats_reset (port);
					lp->rx.start_ewr = lp->rx.end_ewr;
				} else {
					// printf("vlan.id = %d\n",rte_ctrlmbuf_data (lp->rx.mbuf_in.array[0])[15]);
					printf ("NIC port %u: drop ratio = %.2f = %lu / %lu\t%lf\t%lf\t%.1lf\n",
					        (unsigned)port,
					        (double)(stats.ierrors + stats.imissed) /
					            (double)((stats.ierrors + stats.imissed) + stats.ipackets),
					        (uint64_t)stats.ipackets,
					        (uint64_t) (stats.ierrors + stats.imissed),
					        (((stats.ibytes)) / (((end_ewr.tv_sec * 1000000. + end_ewr.tv_usec) -
					                              (start_ewr.tv_sec * 1000000. + start_ewr.tv_usec)) /
					                             1000000.)) /
					            (1000 * 1000 * 1000. / 8.),
					        (((stats.ibytes) + stats.ipackets * (/*4crc+8prelud+12ifg*/ (8 + 12))) /
					         (((end_ewr.tv_sec * 1000000. + end_ewr.tv_usec) -
					           (start_ewr.tv_sec * 1000000. + start_ewr.tv_usec)) /
					          1000000.)) /
					            (1000 * 1000 * 1000. / 8.),
					        stats.ipackets / (((end_ewr.tv_sec * 1000000. + end_ewr.tv_usec) -
					                           (start_ewr.tv_sec * 1000000. + start_ewr.tv_usec)) /
					                          1000000.));

					lp->rx.nic_queues_iters[i] = 0;
					lp->rx.nic_queues_count[i] = 0;
					lp->rx.start_ewr           = end_ewr;  // Updating start
				}
			} else {
				// printf("Hi from queue %d\n",queue);
			}
		}
#endif

		for (j = 0; j < n_mbufs; j++) {
			struct rte_mbuf *pkt = lp->rx.mbuf_in.array[j];
			hptl_get ();
			rte_pktmbuf_free (pkt);
		}
	}
}

static inline void app_lcore_io_rx_sts (struct app_lcore_params_io *lp, uint32_t bsz_rd, uint32_t stsw) {
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
					uint64_t fixedLatency   = currentLatency - (stsw - 1) * (latencyStats[k].pktLen * 8 + 24) / 10.;
					printf ("%d: Latency %lu ns", k + 1, currentLatency);
					printf (" Estimated %lu ns", fixedLatency);
					sumLatency += currentLatency;
					if (hwTimeTest) {
						// fpga time conversion
						uint64_t fpgatime =
						    ntohl (latencyStats[k].hwTime.tv_sec) * 1000000000 + ntohl (latencyStats[k].hwTime.tv_nsec);
						fpgatime /= fpgaConvRate;
						latencyStats[k].hwTime.tv_sec  = fpgatime / 1000000000;
						latencyStats[k].hwTime.tv_nsec = fpgatime % 1000000000;

						printf (" hwTime %lu.%lu", latencyStats[k].hwTime.tv_sec, latencyStats[k].hwTime.tv_nsec);
						if (lastTime != 0) {
							printf (" hwDeltaLatency %lu.%lu",
							        latencyStats[k].hwTime.tv_sec - hwDelta.tv_sec,
							        latencyStats[k].hwTime.tv_nsec - hwDelta.tv_nsec);
						}
						hwDelta = latencyStats[k].hwTime;
					}
					if (lastTime != 0) {
						printf (" insta-BandWidth %lf Gbps",
						        (latencyStats[k].totalBytes * 8 / 1000000000.) /
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
					totalBytes += latencyStats[k].totalBytes;
					printf ("\n");
				} else {
					printf ("%d: Pkt has not been seen\n", k + 1);
					ignored++;
				}
			}
			printf ("Mean-BandWidth %lf Gbps\n",
			        (totalBytes * 8 / 1000000000.) / (((double)lastTime - firstTime) / 1000000000.));
			printf ("Mean-Latency %lf ns\n", sumLatency / (((double)trainLen - ignored)));

			// Ignored / Dropped stats
			if (ignored > 0) {
				printf ("%ld TS-Packets lost\n", ignored);
			}
			struct rte_eth_stats stats;
			rte_eth_stats_get (port, &stats);
			if (stats.ierrors > 0) {
				printf ("%ld Packets errored/dropped\n", stats.ierrors);
			}
			if (stats.oerrors > 0) {
				printf ("%ld Packets errored in TX\n", stats.oerrors);
			}
			printf ("Port %d stats: %lu/%lu/%lu/%lu Pkts  sent/recv/ierror/imissed\n",
			        port,
			        stats.opackets,
			        stats.ipackets,
			        stats.ierrors,
			        stats.imissed);
			printf ("		%lu/%lu Bytes sent/recv\n", stats.obytes, stats.ibytes);
			printf ("		%lu/%lu Queue error sent/recv\n", stats.q_errors[0], stats.rx_nombuf);
			// DEBUG, MUST REMOVE IN RELEASE
			port = 1 - port;
			rte_eth_stats_get (port, &stats);
			printf ("Port %d stats: %lu/%lu/%lu Pkts  sent/recv/oerrors\n",
			        port,
			        stats.opackets,
			        stats.ipackets,
			        stats.oerrors);
			printf ("		%lu/%lu Bytes sent/recv\n", stats.obytes, stats.ibytes);
			printf ("		%lu/%lu Queue error sent/recv\n", stats.q_errors[0], stats.rx_nombuf);
			exit (0);
		}

		if (unlikely (n_mbufs == 0)) {
			continue;
		}

		for (i = 0; i < n_mbufs; i++) {
			uint8_t *data = (uint8_t *)rte_ctrlmbuf_data (lp->rx.mbuf_in.array[i]);
			uint32_t len  = rte_ctrlmbuf_len (lp->rx.mbuf_in.array[i]);

			bytecounter += len;

			if (*(uint16_t *)(data + idoffset) == (TSIDTYPE)tspacketId) {  // paquete marcado
				if (autoIncNum) {
					counter = *(uint16_t *)(data + cntroffset);
					if (counter >= trainLen) {
						continueRX = 0;
						continue;
					}
				}

				// Latency ounters
				latencyStats[counter].recvTime   = hptl_get ();
				latencyStats[counter].sentTime   = (*(hptl_t *)(data + tsoffset));
				latencyStats[counter].pktLen     = len;
				latencyStats[counter].totalBytes = bytecounter;
				latencyStats[counter].recved     = 1;

				bytecounter = 0;  // reset counter

				if (hwTimeTest) {
					latencyStats[counter].hwTime.tv_sec  = *(uint32_t *)(data + 50);
					latencyStats[counter].hwTime.tv_nsec = *(uint32_t *)(data + 54);
				}

				// end if all packets have been recved
				counter++;
				if (counter == trainLen)
					continueRX = 0;
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

		struct rte_mbuf *tmpbuf = rte_ctrlmbuf_alloc (app.pools[1]);
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
		*((hptl_t *)(rte_ctrlmbuf_data (tmpbuf) + tsoffset)) = hptl_get ();

		if (doChecksum) {
			uint16_t cksum;
			cksum = rte_raw_cksum (rte_ctrlmbuf_data (tmpbuf) + icmpStart, sndpktlen - icmpStart);
			*((uint16_t *)(rte_ctrlmbuf_data (tmpbuf) + icmpStart + 2)) = ((cksum == 0xffff) ? cksum : ~cksum);
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
			lp->tx.mbuf_out[port].array[k] = rte_ctrlmbuf_alloc (app.pools[1]);
			if (lp->tx.mbuf_out[port].array[k] == NULL) {
				n_mbufs = k;
				break;
			}

			lp->tx.mbuf_out[port].array[k]->pkt_len  = sndpktlen;
			lp->tx.mbuf_out[port].array[k]->data_len = sndpktlen;
			lp->tx.mbuf_out[port].array[k]->port     = port;

			memcpy (rte_ctrlmbuf_data (lp->tx.mbuf_out[port].array[k]), icmppkt, icmppktlen);
			*(uint16_t *)(rte_ctrlmbuf_data (lp->tx.mbuf_out[port].array[k]) + 26 + VLAN_OFFSET) =
			    htons (k + lp->tx.nic_queues_count[i] + 2);
			*(uint16_t *)(rte_ctrlmbuf_data (lp->tx.mbuf_out[port].array[k]) + 30 + VLAN_OFFSET) =
			    htons (k + lp->tx.nic_queues_count[i] + 2);
		}

		n_pkts = rte_eth_tx_burst (port, queue, lp->tx.mbuf_out[port].array, n_mbufs);

#if APP_STATS
		lp->tx.nic_queues_iters[i]++;
		lp->tx.nic_queues_count[i] += n_mbufs;
		lp->tx.nic_mbufs_count += n_pkts;

		if (lp->tx.nic_mbufs_count >= 25 * 1000 * 10005) {
			printf ("He terminado, a dormir\n");
			fflush (stdout);
			while (1)
				;
		}

		if (unlikely (lp->tx.nic_queues_iters[i] == APP_STATS / 10)) {
			struct rte_eth_stats stats;
			struct timeval start_ewr, end_ewr;

			rte_eth_stats_get (port, &stats);
			gettimeofday (&lp->tx.end_ewr, NULL);

			start_ewr = lp->tx.start_ewr;
			end_ewr   = lp->tx.end_ewr;

			if (queue == 0) {
				printf (
				    "NIC TX port %u: drop ratio = %.2f (%lu/%lu) useful-speed: %lf Gbps, "
				    "link-speed: %lf Gbps (%.1lf pkts/s)\n",
				    (unsigned)port,
				    (double)stats.oerrors / (double)(stats.oerrors + stats.opackets),
				    (uint64_t)stats.opackets,
				    (uint64_t)stats.oerrors,
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

#ifdef APP_LIMITBW
		hptl_waitns (bwlimitNSwait);
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
			lp->tx.mbuf_out[port].array[k] = rte_ctrlmbuf_alloc (app.pools[1]);
			if (lp->tx.mbuf_out[port].array[k] == NULL) {
				n_mbufs = k;
				break;
			}

			lp->tx.mbuf_out[port].array[k]->pkt_len  = sndpktlen;
			lp->tx.mbuf_out[port].array[k]->data_len = sndpktlen;
			lp->tx.mbuf_out[port].array[k]->port     = port;

			memcpy (rte_ctrlmbuf_data (lp->tx.mbuf_out[port].array[k]),
			        icmppkt,
			        icmppktlen > sndpktlen ? sndpktlen : icmppktlen);
		}

		if (queue == 0) {
			*((uint16_t *)(rte_ctrlmbuf_data (lp->tx.mbuf_out[port].array[0]) + idoffset)) = (TSIDTYPE)tspacketId;

			if (autoIncNum) {
				*((uint16_t *)(rte_ctrlmbuf_data (lp->tx.mbuf_out[port].array[0]) + cntroffset)) = pktcounter++;
				if (pktcounter > trainLen) {
					hptl_waitns (waitTime);
					continueRX = 0;
					hptl_waitns (waitTime);
					exit (1);
				}
			}

			*(hptl_t *)(rte_ctrlmbuf_data (lp->tx.mbuf_out[port].array[0]) + tsoffset) = hptl_get ();
		}

		if (doChecksum) {
			uint16_t cksum;
			cksum =
			    rte_raw_cksum (rte_ctrlmbuf_data (lp->tx.mbuf_out[port].array[0]) + icmpStart, sndpktlen - icmpStart);
			*((uint16_t *)(rte_ctrlmbuf_data (lp->tx.mbuf_out[port].array[0]) + icmpStart + 2)) =
			    ((cksum == 0xffff) ? cksum : ~cksum);
		}

		n_pkts = rte_eth_tx_burst (port, queue, lp->tx.mbuf_out[port].array, n_mbufs);

		if (n_pkts == 0) {
			pktcounter--;
			for (k = n_pkts; k < n_mbufs; k++) {
				struct rte_mbuf *pkt_to_free = lp->tx.mbuf_out[port].array[k];
				rte_ctrlmbuf_free (pkt_to_free);
			}
		} else {
			while (unlikely (n_pkts < n_mbufs)) {
				uint64_t tmp;
				tmp = rte_eth_tx_burst (port, queue, lp->tx.mbuf_out[port].array + n_pkts, n_mbufs - n_pkts);
				n_pkts += tmp;
			}
		}
	}
}

static void app_lcore_arp_tx_gratuitous (struct app_lcore_params_io *lp) {
	uint32_t i;
	for (i = 0; i < lp->tx.n_nic_queues; i++) {
		uint8_t port  = lp->tx.nic_queues[i].port;
		uint8_t queue = lp->tx.nic_queues[i].queue;

		struct rte_mbuf *tmpbuf = rte_ctrlmbuf_alloc (app.pools[1]);
		if (!tmpbuf) {
			puts ("Error creating gratuitous ARP");
			exit (-1);
		}

		tmpbuf->pkt_len  = arppktlen;
		tmpbuf->data_len = arppktlen;
		tmpbuf->port     = port;

		memcpy (rte_ctrlmbuf_data (tmpbuf), arppkt, arppktlen);

		rte_eth_macaddr_get (port, (struct ether_addr *)(rte_ctrlmbuf_data (tmpbuf) + 6));
		rte_eth_macaddr_get (port, (struct ether_addr *)(rte_ctrlmbuf_data (tmpbuf) + 6 + 6 + 2 + 8 + VLAN_OFFSET));
		// memcpy (rte_ctrlmbuf_data (tmpbuf) + 6 + 6 + 2 + 14, icmppkt + 6 + 6 + 2 + 4 * 4, 4);

		/*printf ("PKT:\n");
		for (int k = 0; k < arppktlen; k++) {
		    printf ("%02hhX,", rte_ctrlmbuf_data (tmpbuf)[k]);
		}
		printf ("\n");
		exit (0);*/

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
	gettimeofday (&lp->tx.start_ewr, NULL);
	lp->rx.start_ewr        = lp->tx.start_ewr;
	lp->rx.start_ewr.tv_sec = 0;

	if (bandWidthMeasureActive) {  // only measure bw
		while (likely (doloop)) {
			if (likely (lp->rx.n_nic_queues > 0)) {
				app_lcore_io_rx_bw (lp, bsz_rx_rd);
			}

			if (likely (lp->tx.n_nic_queues > 0)) {
				app_lcore_io_tx_bw (lp, app.burst_size_io_tx_write);
			}

			i++;
		}
	} else if (bandWidthMeasure) {  // only measure bw
		while (likely (doloop)) {
			if (likely (lp->rx.n_nic_queues > 0)) {
				app_lcore_io_rx_bw (lp, bsz_rx_rd);
			}

#if APP_STATS
			if (i > APP_STATS * 1000) {
				app_lcore_arp_tx_gratuitous (lp);
				i = 0;
			}
#endif

			i++;
		}
	} else if (selectiveTS) {  // measure bw & latency, performing a selective Timestamping
		// Send a pkt-burst to fill output queues
		app_lcore_io_tx_bw (lp, app.nic_tx_ring_size - 1);

		while (likely (doloop)) {
			if (likely (lp->rx.n_nic_queues > 0)) {
				app_lcore_io_rx_sts (lp, bsz_rx_rd, app.burst_size_io_tx_write);
			}

			if (likely (lp->tx.n_nic_queues > 0)) {
				app_lcore_io_tx_sts (lp, app.burst_size_io_tx_write);
			}

			i++;
		}
	} else if (trainFriends) {
		while (likely (doloop)) {
			if (likely (lp->rx.n_nic_queues > 0)) {
				app_lcore_io_rx_sts (lp, bsz_rx_rd, trainFriends);
			}

			if (likely (lp->tx.n_nic_queues > 0)) {
				app_lcore_io_tx_sts (lp, trainFriends);
				if (trainSleep) {
					hptl_waitns (trainSleep);
				}
			}

			i++;
		}
	} else {  // measure bw & latency, priorizing latency
		*((uint16_t *)(icmppkt + idoffset)) = (TSIDTYPE)tspacketId;
		while (likely (doloop)) {
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
		for (;;) {
			app_lcore_main_loop_io ();
		}
	}

	return 0;
}
