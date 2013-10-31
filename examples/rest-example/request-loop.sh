#!/bin/bash

CURL='curl aaaa::c30c:0:0:2:8080/h'

(curl "aaaa::c30c:0:0:2:8080/h" &)
while sleep 20; do
  (curl "aaaa::c30c:0:0:2:8080/h" &)
done
