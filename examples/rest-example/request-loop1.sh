#!/bin/bash

#trap "killall curl" 2

available=0

while [ "$available" = "0" ]; do
  available=1
  (ping6 -c 1 "aaaa::c30c:0:0:2")
  if [ "$?" != "0" ]; then
    available=0
  echo "rebuild RPL DAG!"
  sleep 10
  fi
done

for i in `seq 1 10`; do
  (curl "aaaa::c30c:0:0:2:8080/h" &)
  sleep 10;
done

