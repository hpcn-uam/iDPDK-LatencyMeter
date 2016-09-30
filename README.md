High Speed Latency-Metter using DPDK
=================

This application allows to measure the network's Latency and Bandwith.
This a

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
The latest tested DPDK-repository with this application is included in the dpdk folder.
Howerver, any other compatible-version could be used by exporting `RTE_SDK` variable.

To compile the included DPDK-release, it is recomended to execute and follow the basic dpdk-setup script, example:

````
cd dpdk
./tools/dpdk-setup.sh
cd ..
````

APP-Compilation
=================
The application is compiled automatically when executing one of the provided scripts.
If you prefere to compile it manually, in the `src` folder there is a Makefile to do so.

Execution
=================
In `script` folder, there are some example scripts:

- `scripts/interface0.sh` starts measuring the latency in the interface number 0. Using it to send and receive packets.
- `scripts/interface01.sh` starts measuring the latency in the interface number 0 and 1. Using interface 0 to send and interface 1 to receive packets.

Also, those scripts accept the following extra (optional) parameters:

````
    --etho "aa:bb:cc:dd:ee:ff" : The ethernet origin MAC addr                
    --ethd "aa:bb:cc:dd:ee:ff" : The ethernet destination MAC addr           
    --ipo "11.22.33.44" : The ip origin addr                                 
    --ipd "11.22.33.44" : The ip destination addr                            
    --trainLen "TRAIN LENGTH" : Enables and sets the packet train length     
    --trainTime "TRAIN TIMEOUT" : When to stop the measurment when all       
                                    packets has been sent                      
    --chksum : Each packet recalculate the IP/ICMP checksum                    
    --autoInc : Each packet autoincrements the ICMP's sequence number  
````
