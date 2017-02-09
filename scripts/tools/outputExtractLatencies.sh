#!/bin/bash

grep ": Latency" $1 | cut -f3 -d' '