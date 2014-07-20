#!/bin/bash

##
# run_multicast_sims.sh
# Date: 2014-05-12
# Author: Philipp Raich
# Description: Repeat the existing multicast sims with varying payloads
# Simulations and payloads hardcoded (sorry)
#
# This file war written for a certain purpose, please adjust it to your needs
#

trap 'kill 0 && exit' SIGINT SIGTERM EXIT

declare -a payloads=('4' '8' '16' '32' '64')
iterations=2

for i in ${payloads[@]}; do
  echo "=== Running payload #$i"

  echo "=== writing payload to project-conf.h"
  sed -i "s/#define REQUEST_SIZE[ tab]*[0-9]\+/#define REQUEST_SIZE $i/g" project-conf.h

  echo "=== make clean && make -j4"
  make clean && make -j4

  echo "=== run simulations '$iterations' times"
  payload_string="b"
  ./run_sim.sh sim_group_mc1_auto.csc mc_test_$i$payload_string $iterations
done
