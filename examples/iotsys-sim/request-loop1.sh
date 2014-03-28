#!/bin/bash

#trap "killall curl" 2

interval=$1

while [ "0" ]; do
  (ping6 -c 1 "aaaa::c30c:0:0:2" -W $interval)
  if [ "$?" = "0" ]; then
    break
  fi
  sleep 10
done

for i in `seq 1 10`; do
  echo "next round"
    (sh coap-caller.sh coap://[aaaa::c30c:0:0:2]/h $interval &)
  sleep $interval;
done

