#!/bin/sh

# Server details
server="localhost"
port="3000"

# Data to send
data="Hello, Server!"

# Number of connections to create
num_connections=5

# Create connections and send data
for i in $(seq 1 $num_connections)
do
    (echo "$data"; sleep 1; echo "$data"; sleep 1; echo "$data") | nc $server $port &
done