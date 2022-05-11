#!/bin/bash

IP="10.3.1.16"
LAN="10.0.16.0"
MASK="24"
bucket="1024"
rate="1024"
echo "1" > /proc/sys/net/ipv4/ip_forward
iptables -t nat -F
iptables -t filter -F
iptables -t mangle -F
iptables -t filter -A FORWARD -j NFQUEUE --queue-num 0 -p udp -s ${LAN}/${MASK} ! -d ${IP} --dport 10000:12000
iptables -t mangle -A PREROUTING -j NFQUEUE --queue-num 0 -p udp -d ${IP} --dport 10000:12000

./nat ${IP} ${LAN} ${MASK} ${bucket} ${rate}
