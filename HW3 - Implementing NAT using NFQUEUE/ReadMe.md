**Environment**
<pre>
C
Linux
</pre>

**Background**
<pre>
Network Address Translation (NAT) is a method of mapping an IP address space
into another by modifying network address information in the IP header of packets
while they are in transit across a traffic routing device.

This was orginially used to avoid the need to assign a new address to every host.
But then, IPv4 address exhaustion becomes new problem, so NAT has become popular solution.

This programme will implement a NAT application, using software library NFQUEUE.
The NAT application can forward UDP traffic.
</pre>

**Topology**
<pre>
Inbound --> | <-- Outbound
packets     | packets
--------    | ---------
Dept |      | | VM A | <----- VM B / VM C
network|    | | (NAT) | UDP
--------    | ---------

VM A has a network card configured with a public IPaddress (i.e., Apublic),
and it will serve as the NAT gateway.

By properly configuring the route tables of VM B and VM C, 
VM A can relay any inbound/outbound traffic for VM B and VM C.
Let Ain be the internal IP address of VM A. 
Then we execute the following command in both VM B and VM C to add the default gateway.
</pre>
```
sudo route add default gw Ain
```

**Usage**
<pre>
The NAT programme will relay on UDP traffic and drop any unexpected packet,
with token bucket traffic shaping, and multi-threading.

*Only relay to outbound traffic to one of the reachable workstations in the department by default.
</pre>

**iptables config**
<pre>
IP="10.3.1.54" # public interface
LAN="10.0.54.0" # private LAN network address (without subnet mask)
MASK="24" # subnet mask
echo "1" > /proc/sys/net/ipv4/ip_forward
iptables -t nat -F
iptables -t filter -F
iptables -t mangle -F
iptables -t filter -A FORWARD -j NFQUEUE --queue-num 0 -p udp -s ${LAN}/${MASK} ! -d ${IP} --dport 10000:12000
iptables -t mangle -A PREROUTING -j NFQUEUE --queue-num 0 -p udp -d ${IP} --dport 10000:12000
</pre>

**Implementation Details**
<pre>
Please view the pdf.
</pre>

**Command-line interface**
```
sudo ./nat <IP> <LAN> <MASK> <bucket size> <fill rate>
```
<pre>
run.sh is for iptables configuration for covenient in testing at department environment
please replace to your desired config if you need.
</pre>