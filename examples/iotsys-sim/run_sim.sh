#!/bin/bash

trap 'kill $(jobs -p)' SIGINT SIGTERM
#trap "kill $pid1 && kill $pid2 " 2

router_success=0
sim_name=$1

for i in `seq 1 5`; do
  echo "next round"

  # start sim headless
  java -mx512m -jar ../../tools/cooja/dist/cooja.jar -nogui="$sim_name.csc" -contiki='../..' &
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
  filename=$sim_name$separator$i
  filedir="sim_logs"
  date_string=$(date +"%F")

  filedir="$filedir/$date_string"
  if [ ! -d "$filedir" ]; then
    mkdir -p "$filedir"
  fi

  mv "COOJA.radiolog" "$filedir/pdump_$filename.txt"
  mv "COOJA.testlog" "$filedir/powertrace_$filename.txt"
  
done
