High Speed Latency-Metter using Intel DPDK
=================

Intel Data Plane Development Kit (DPDK) LatencyMetter

This program allows to measure the Latency.


Compilation
=================
The program can be easly compiled using the makefile provided.
It needs (as anyother DPDK app) the DPDK env. variables such as *RTE_SDK* to be defined first.

The script *setup.sh* can be used to compile everything. Feel free to modify the script and modify those variables.


Execution
=================
There are 2 scripts, one for measure 1 interface (the traffic must return to the same interface).
The other script, measure 2 ports bidirectional
