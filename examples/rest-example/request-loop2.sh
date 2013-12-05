#!/bin/bash

#trap "killall curl" 2

available=0
interval=$1

while [ "$available" = "0" ]; do
  available=1
  for i in `seq 2 3`; do
      echo "ping id: $i"
      (ping6 -c 1 "aaaa::c30c:0:0:$i")
      if [ "$?" != "0" ]; then
        available=0
        break
      fi
  done
  if [ "$available" = "0" ]; then
    echo "rebuild RPL DAG!"
    sleep 10
  fi
done

for i in `seq 1 10`; do
  for i in `seq 2 3`; do
    (sh curl-caller.sh "aaaa::c30c:0:0:$i:8080/h" $interval &)
  done
  sleep $interval;
done

