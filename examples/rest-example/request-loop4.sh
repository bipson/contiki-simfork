#!/bin/bash

#trap "killall curl" 2

available=0
interval=$1

while [ "$available" = "0" ]; do
  available=1
  for i in `seq 2 5`; do
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
  echo "next round"
  for i in `seq 2 5`; do
    (sh curl-caller.sh "aaaa::c30c:0:0:$i:8080/h" $interval &)
    #(curl -m 10 "aaaa::c30c:0:0:$i:8080/h" > /dev/null 2| grep "curl" &)
  done
  sleep $interval;
done

