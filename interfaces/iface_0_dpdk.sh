#!/bin/bash

# 0000:19:00.0 enp25s0f0
# 0000:19:00.1 enp25s0f1
# 0000:d8:00.0 enp216s0f0
# 0000:d8:00.1 enp216s0f1

ifconfig enp25s0f0 down

/home/naudit/iDPDK-LatencyMetter/dpdk/usertools/dpdk-devbind.py -b igb_uio 0000:19:00.0
