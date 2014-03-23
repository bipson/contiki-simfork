#!/bin/bash

interval=$2

(bash timeout3 -t $interval java -jar cf-client.jar -t GET "$1")

if [ "$?" != "0" ]; then
  echo "error!"
else
  echo "OK"
fi
