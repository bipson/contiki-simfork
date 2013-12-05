#!/bin/bash

interval=$2

(curl -m $2 -s "$1" > /dev/null)

if [ "$?" != "0" ]; then
  echo "error!"
else
  echo "OK"
fi
