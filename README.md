[Alpha] High Speed Latency-Metter using DPDK 
=================

This application allows to measure the network's Latency and Bandwith.
*Current phase*: **Early development**

Clonning
=================
To clone this project execute:

````
git clone git@github.com:hpcn-uam/iDPDK-LatencyMetter.git
git submodule update --init --recursive
````

if you want to update (pull) to a newer commit, you should execute:

````
git pull
git submodule update --init --recursive
````

DPDK-Compilation
=================
The latest tested DPDK-repository with this application is included in the `dpdk` folder.
Howerver, any other compatible-version could be used by exporting `RTE_SDK` variable.

To compile the included DPDK-release, it is recomended to execute and follow the basic `dpdk-setup.sh` script, example:

````
cd dpdk
./tools/dpdk-setup.sh
cd ..
````

APP-Compilation
=================
The application is compiled automatically when executing one of the provided scripts.
If you prefere to compile it manually, in the `src` folder there is a `Makefile` to do it.

Execution
=================
In `script` folder, there are some example scripts:

- `scripts/interface0.sh` starts measuring the latency in the interface number 0. Using it to send and receive packets.
- `scripts/interface0.40g.sh` It is a similar script than the adove. This script uses 2 tx queues to saturate efficiently a 40G Ethernet link.
- `scripts/interface01.sh` starts measuring the latency in the interface number 0 and 1. Using interface 0 to send and interface 1 to receive packets.

The typical test, can be sumarized with the following execution parameters:

- Measure latency and bandwidth using packet trains: `./scripts/interface01.sh --trainLen 1000 --pktLen 60 --sts`
- Measure only bandwidth (it will saturate the link non-stop): `./scripts/interface01.sh --bw --pktLen 60`
- Measure only latency (a sleep should be produced between packets): `./scripts/interface01.sh --trainLen 1000 --pktLen 1500 --trainSleep 2000`

Also, those scripts accept the following extra (optional) parameters:

````
    --etho "aa:bb:cc:dd:ee:ff" : The ethernet origin MAC addr                
    --ethd "aa:bb:cc:dd:ee:ff" : The ethernet destination MAC addr           
    --ipo "11.22.33.44" : The ip origin addr                                 
    --ipd "11.22.33.44" : The ip destination addr                            
    --pktLen "Packet LENGTH" : Sets the size of each sent packet             
    --trainLen "TRAIN LENGTH" : Enables and sets the packet train length     
    --trainSleep "TRAIN SLEEP": Sleep in NS between packets                  
    --sts : Mode that sends lots of packets but only a few are timestamped     
    --waitTime "WAIT TIMEOUT" : Nanoseconds to stop the measurment when all  
                                    packets has been sent                      
    --chksum : Each packet recalculate the IP/ICMP checksum                    
    --autoInc : Each packet autoincrements the ICMP's sequence number          
    --bw : Only measures bandwidth, but with higher resolution                 
    --bwp: Only measures bandwidth(pasive) by just listening. No packet is sent
    --lo : The application works in loopback mode. Used to measure RTT        
````


Calibration
=================
We are developing a auto-config/calibration to the system.
In order to test the calibration, you can try the following parameters:

````
    --calibrate \"outputFile\" : Generate a calibration file. May take hours       
    --calibration \"inputFile\" : Open a calibration file, to fix measurements
````