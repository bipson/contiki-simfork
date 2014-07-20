#!/bin/bash

available=0

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
