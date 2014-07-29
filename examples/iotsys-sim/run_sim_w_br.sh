#!/bin/bash

##
# run_sim.sh
# Date: 2014-05-12
# Author: Philipp Raich
# Description: Repeat a given cooja simulation n times (default "5") and
# write radio-log and testlog-output into:
# './sim_logs/$(date + "%F")/{pdump|powertrace}_<output-postfix>_<iteration>.txt'
#
# This file war written for a certain purpose, please adjust it to your needs
#
# For RadioLogs in headless mode (written to 'COOJA.radiolog' you need a/the
# 'RadioLoggerHeadless' cooja app (not yet publicly available) based on:
# https://github.com/cetic/contiki/tree/feature-cooja-headless-radiologger

trap 'cleanup' SIGINT SIGTERM
#trap "kill $pid1 && kill $pid2 " 2

function cleanup {
  # kill all background jobs:
  kill $(jobs -pr) 
  exit
}

if [ $# != 2 ] && [ $# != 3 ]; then
  "usage: `basename $0` <csc-file> <output-postfix> [<iterations>]" >&2
  exit 1
fi

sudo -v

router_success=0
sim_file=$1
output_name=$2
if [ -z "$3" ]; then
  iterations = 5
else
  iterations=$3
fi

for i in `seq 1 $iterations`; do
  echo "=== Running Simulation run #$i"

  # TODO write output of following commands to log?

  # start sim headless
  java -mx512m -jar ../../tools/cooja/dist/cooja.jar -nogui="$sim_file" -contiki='../..' &
  pid1=$!

  # start border-router
  while [ $router_success = 0 ]; do
    sudo ./connect-router-cooja.sh &
    pid2=$!
    sleep 1
    running=`ps -p $pid2 --no-headers | wc -l`
    if [ $running != 0 ]; then
      router_success=1
    fi
  done

  # wait for end of sim
  wait
  router_success=0
  
  # move/rename output  
  separator='_'
  output_postfix=$output_name$separator$i
  filedir="sim_logs"
  date_string=$(date +"%F")

  filedir="$filedir/$date_string"
  if [ ! -d "$filedir" ]; then
    mkdir -p "$filedir"
  fi

  mv "COOJA.radiolog" "$filedir/pdump_$output_postfix.txt"
  mv "COOJA.testlog" "$filedir/powertrace_$output_postfix.txt"
  
done
