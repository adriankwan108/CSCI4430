#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <netinet/in.h> // required by "netfilter.h"
#include <arpa/inet.h> // required by ntoh[s|l]()
#include <signal.h> // required by SIGINT
#include <string.h> // required by strerror()
#include <sys/time.h> // required by gettimeofday()
#include <time.h> // required by nanosleep()
#include <errno.h> // required by errno
# include <pthread.h>
#include <netinet/ip.h>        // required by "struct iph"
#include <netinet/tcp.h>    // required by "struct tcph"
#include <netinet/udp.h>    // required by "struct udph"
#include <netinet/ip_icmp.h>    // required by "struct icmphdr"

extern "C" {
#include <linux/netfilter.h> // required by NF_ACCEPT, NF_DROP, etc...
#include <libnetfilter_queue/libnetfilter_queue.h>
}

#include "checksum.h"

#define BUF_SIZE 1500

struct nat_entry{
  unsigned int WANip;     //network order
  unsigned short WANport;     //host order
  unsigned int LANip;     //network order
  unsigned short LANport;     //host order
  struct timeval timer;
  struct nat_entry* next;
};typedef struct nat_entry nat_entry;

nat_entry* nat_Table = NULL;

//global variables
unsigned int gw_public_IP;
unsigned int gw_private_IP;
int mask_int;
unsigned int local_mask;
int bucket_size;
float fill_rate;

int mytokenbucket;

struct timeval begin_Time;
struct timeval end_Time;

int consume()
{
  gettimeofday(&end_Time, NULL);
  double time_spend = ((end_Time.tv_sec - begin_Time.tv_sec)+ (1e-6)*(end_Time.tv_usec - begin_Time.tv_usec));
  //printf("time_spend: %f\n", time_spend);
  
  if(time_spend >= 1)
  {
    int num = (int)time_spend;
    gettimeofday(&begin_Time, NULL);

    mytokenbucket += fill_rate*num;
    if(mytokenbucket >= bucket_size)
    {
      mytokenbucket = bucket_size;
    }
  }

  if( mytokenbucket >0 )
  {
    //calculate the time
    mytokenbucket --;
    //printf("Remain token: %d\n", mytokenbucket);
    return 1; //send the packet
  }else if(mytokenbucket==0){
    return 0; //no token
  }else{
    printf("Token error\n");
    exit(1);
    return -1;
  }
}

void ShowNAT()
{
  printf("--------------------NAT-------------------\n");
  printf("  LAN IP  | LAN Port |  WAN IP  | WAN Port\n");
  nat_entry* ptr = nat_Table;
  while( ptr != NULL)
  {
    printf("%s |  %u |  ", inet_ntoa(*(struct in_addr*)&( ptr->LANip) ), ptr->LANport); 
    printf("%s |  %u \n", inet_ntoa(*(struct in_addr*)&(ptr->WANip)), ptr->WANport);
    
    ptr = ptr->next;
  }
  printf("------------------------------------------\n");
}

int CheckExpireEntry(struct timeval tm, nat_entry** table)
{
  nat_entry* curr_ptr = *table;
  nat_entry* prev_ptr = NULL;

  int shouldPrint = 0;

  if(curr_ptr == NULL)
  {
    return 0;
  }
  
  if(curr_ptr->next == NULL)
  {
    double result = ((tm.tv_sec - curr_ptr->timer.tv_sec)+(1e-6)*(tm.tv_usec - curr_ptr->timer.tv_usec));
    if( result > 10  )
    {
      printf("[WANport:%u]: expired, entry has existed for %f\n", curr_ptr->WANport, result);
      //delete this entry
      
      shouldPrint = 1;
      
      free(*table);
      *table = NULL;
    }
    return shouldPrint;
  }else
  {
    //now prev_ptr = NULL, curr_ptr = *table
    
    while(curr_ptr!=NULL)
    {
      double result = ((tm.tv_sec - curr_ptr->timer.tv_sec)+(1e-6)*(tm.tv_usec - curr_ptr->timer.tv_usec));
      if(  result >10  )
      {
        printf("[WANport:%u]: expired, entry has existed for %f\n", curr_ptr->WANport, result);
        
        shouldPrint = 1;
        
        nat_entry* temp = curr_ptr;
        if(prev_ptr == NULL)
        {
          prev_ptr = NULL;   
          (*table) = curr_ptr->next;
          curr_ptr = (*table);
        }else
        {
          prev_ptr->next = curr_ptr->next;
          curr_ptr = curr_ptr->next;
        }
        free(temp);
      }else
      {
        if(prev_ptr==NULL)
        {
          prev_ptr = *table;
        }else
        {
          prev_ptr = prev_ptr->next;
        }
       
        curr_ptr = curr_ptr->next;
      }
    }
    return shouldPrint;
  }

}

int SearchEntry(unsigned int sourceIP, unsigned short sourcePort, struct timeval tm)
{
  //both sourceIP in network byte, soucePort in host byte
  //CheckExpireEntry(tm, &nat_Table);
  nat_entry* ptr = nat_Table;
  while(ptr!=NULL)
  {
    if( (ptr->LANip == sourceIP) && (ptr->LANport == sourcePort))
    {
      ptr->timer = tm;
      return 1;
    }else
    {
      ptr = ptr->next;
    }
  }

  printf("Search entry: search nothing\n");
  return 0;
}

int SearchSmallestPort()
{
  int num = 10000;
  nat_entry* ptr = nat_Table;

  while(ptr!=NULL)
  {
    if(num == ptr->WANport)
    {
      num++;
      ptr = ptr->next;
    }else
    {
      return num;
    }
  }

  return num;
}

nat_entry* MergeSortedList(nat_entry* lst1, nat_entry* lst2)
{
  nat_entry* result = NULL;

  if(lst1 == NULL)
  {
    return (lst2);
  }else if(lst2 == NULL)
  {
    return (lst1);
  }

  if(lst1->WANport <= lst2->WANport)
  {
    result = lst1;
    result->next = MergeSortedList(lst1->next, lst2);
  }else
  {
    result = lst2;
    result->next = MergeSortedList(lst1, lst2->next);
  }
  return result;
}

void SplitList(nat_entry* source, nat_entry** front, nat_entry** back)
{
  nat_entry* ptr1;
  nat_entry* ptr2;
  ptr2 = source;
  ptr1 = source->next;

  while(ptr1 != NULL)
  {
    ptr1 = ptr1->next;
    if(ptr1 != NULL)
    {
      ptr2 = ptr2->next;
      ptr1 = ptr1->next;
    }
  }

  *front = source;
  *back = ptr2->next;
  ptr2->next = NULL;
}

void MergeSort(nat_entry** table)
{
  nat_entry* head = *table;
  nat_entry* ptr1;
  nat_entry* ptr2;

  if( (head==NULL) || (head->next)==NULL )
  {
    return;
  }

  //split
  SplitList(head, &ptr1, &ptr2);
  MergeSort(&ptr1);
  MergeSort(&ptr2);
  *table = MergeSortedList(ptr1, ptr2);
}

void AppendEntry(nat_entry** table, unsigned int sourceIP, unsigned short sourcePort, struct timeval arriveTime)
{
  printf("Create Entry\n");

  nat_entry* node = (nat_entry*)malloc(sizeof(nat_entry));
  nat_entry* last = (*table);

  node->WANip = gw_public_IP;
  node->WANport = SearchSmallestPort();
  node->LANip = sourceIP;
  node->LANport = sourcePort;
  node->timer = arriveTime;
  node->next = NULL;

  if( (*table) == NULL)  //first entry
  {
    (*table) = node;
    return;
  }
  
  while( (last->next) != NULL)
  {
    last = last->next;
  }

  (last->next) = node; 

  //sorting in ascending order in WLAN:port
  printf("Merge Sort...\n");
  MergeSort(&nat_Table);
}

unsigned short SearchEntryPort(unsigned int sourceIP, unsigned short sourcePort){
  //both sourceIP in network byte, soucePort in host byte
  nat_entry* ptr = nat_Table;

  while(ptr!=NULL)
  {
    if( (ptr->LANip == sourceIP) && (ptr->LANport == sourcePort))
    {
      return ptr->WANport;
    }else
    {
      ptr = ptr->next;
    }
  }
  //it should find something, if no, 0
  return 0;
}

nat_entry* SearchInbound(nat_entry** table,unsigned int W_IP,unsigned short W_port, struct timeval tm)
{
  //CheckExpireEntry(tm,table, 1);
  nat_entry* ptr = *table;

  while(ptr!=NULL)
  {
    if(ptr->WANport == W_port && ptr->WANip == W_IP)
    {
      ptr->timer = tm;
      return ptr;
    }
    ptr = ptr->next;
  }
  return NULL;
}

static int Callback(struct nfq_q_handle *myQueue, struct nfgenmsg *msg, struct nfq_data *pkt, void *cbData) 
{
  
  // Get the id in the queue
  unsigned int id = 0;
  struct nfqnl_msg_packet_hdr *header;
  header = nfq_get_msg_packet_hdr(pkt);
  id = ntohl(header->packet_id);

  //get time
  struct timeval arriveTime;
  gettimeofday(&arriveTime, NULL);

  // Access IP Packet
  unsigned char *pktData;
  int ip_pkt_len;
  struct iphdr *ipHeader;

  ip_pkt_len = nfq_get_payload(pkt, &pktData);
  ipHeader = (struct iphdr*)pktData;

  printf("Received packet: id = %d, ts = %ld.%ld\n", id, arriveTime.tv_sec, arriveTime.tv_usec);
  
  int bound_mode = -1;
  int shouldPrintNat = 0;

  if (ipHeader->protocol == IPPROTO_UDP) {
    printf("Is UDP\n");
    //get source IP, source port, dest IP, dest port
    unsigned int iphdr_sourceIP = ipHeader->saddr;  //network byte order
    unsigned int iphdr_destIP = ipHeader->daddr;
    
    struct udphdr *udph;
    udph = (struct udphdr*)(((char*)ipHeader)+ipHeader->ihl*4);
    
    unsigned short src_port = ntohs(udph->source);
    unsigned short dst_port = ntohs(udph->dest);

    printf("src[%s:%u] ", inet_ntoa(*(struct in_addr*)&iphdr_sourceIP), src_port);
    printf("dest[%s:%u] ",inet_ntoa(*(struct in_addr*)&iphdr_destIP), dst_port);
  
    unsigned int prev_sourceIP = iphdr_sourceIP;
    unsigned int prev_destIP = iphdr_destIP;
    unsigned short prev_src_port = src_port;
    unsigned short prev_dst_port = dst_port;

    //determine inbound/outbound
    if( (ntohl(iphdr_sourceIP) & ntohl(local_mask)) == (ntohl(gw_private_IP) & ntohl(local_mask)))
    {
      printf("outbound\n");
      bound_mode = 0;
    }else{
      printf("inbound\n");
      bound_mode = 1;
    }

    if(CheckExpireEntry(arriveTime,&nat_Table))
    {
      shouldPrintNat = 1;
    }

    //seperate methods for inbound / outbound
    if(bound_mode == 0)
    {
      //outbound
      //searches if sourceIP-port pair has already stored in NAT table
      if(SearchEntry(iphdr_sourceIP, src_port, arriveTime) == 0)
      {
        //no result, create new entry
        AppendEntry(&nat_Table, iphdr_sourceIP, src_port, arriveTime);
        shouldPrintNat = 1;
        //change address and port
        ipHeader->saddr = gw_public_IP;
        udph->source = htons(SearchEntryPort(iphdr_sourceIP, src_port));
        //printf("Creat successful\n");
      }else
      {
        //searched
        //printf("searched\n");
        ipHeader->saddr = gw_public_IP;
        udph->source = htons(SearchEntryPort(iphdr_sourceIP, src_port));
      }

      //for printing
      iphdr_sourceIP = ipHeader->saddr;  //network byte order 

      udph = (struct udphdr*)(((char*)ipHeader)+ipHeader->ihl*4);
      src_port = ntohs(udph->source);

      printf("  [OUTBOUND]: translate: src: [%s:%u] -> ", inet_ntoa(*(struct in_addr*)&prev_sourceIP), prev_src_port);
      printf("[%s:%u]\n", inet_ntoa(*(struct in_addr*)&iphdr_sourceIP), src_port);

    }else if(bound_mode == 1)
    {
      //inbound
      //printf("inbound mode\n");
      //search for destination port
      nat_entry* temp = SearchInbound(&nat_Table, prev_destIP, prev_dst_port, arriveTime);
      if(temp != NULL)
      {
        ipHeader->daddr = temp->LANip;
        udph->dest = htons(temp->LANport);

        //for printing
        iphdr_destIP = ipHeader->daddr;
        udph = (struct udphdr*)(((char*)ipHeader)+ipHeader->ihl*4);
        dst_port = ntohs(udph->dest);
        printf("  [INBOUND]: translate: dst: [%s:%u] -> ", inet_ntoa(*(struct in_addr*)&prev_destIP), prev_dst_port);
        printf("[%s:%u]\n", inet_ntoa(*(struct in_addr*)&iphdr_destIP), dst_port);
      }else
      {
        if(shouldPrintNat == 1)
        {
          ShowNAT();
        }
        printf("Drop this packet\n");
        printf("_______________________________\n");
        return nfq_set_verdict(myQueue, id, NF_DROP, ip_pkt_len, pktData);
      }
    }

    if(shouldPrintNat == 1)
    {
      ShowNAT();
    }

    // re-calculate checksum in that packet
    udph->check = 0;
    ipHeader->check = 0;

    udph->check = udp_checksum(pktData);
    ipHeader->check = ip_checksum(pktData);

  }else{
    printf("packet is not UDP, ignored\n");
  }
  printf("_______________________________\n");
  
  //traffic shaping
  struct timespec tim1, tim2;
  tim1.tv_sec = 0;
  tim1.tv_nsec = 5000;

  while(consume() == 0){
    if(nanosleep(&tim1, &tim2)<0){
      printf("ERROR: nanosleep() system call failed!\n");
      exit(1);
    }
  }
  return nfq_set_verdict(myQueue, id, NF_ACCEPT, ip_pkt_len, pktData);
}


int main(int argc, char** argv) {

  if(argc != 6)
  {
    printf("Usage: sudo ./nat <IP> <LAN> <MASK> <bucket size> <fill rate>\n");
    exit(-1);
  }else{

    inet_aton(argv[1] , (struct in_addr*)&gw_public_IP); //saved in network byte order
    inet_aton(argv[2] , (struct in_addr*)&gw_private_IP);
    mask_int = atoi(argv[3]);
    local_mask = (0xffffffff << (32 - mask_int)); //host byte order
    bucket_size = atoi(argv[4]);
    fill_rate = atof(argv[5]);
    local_mask = htonl(local_mask); //network byte order now
    mytokenbucket = bucket_size;
    printf("public ip: %s\n", inet_ntoa( *(struct in_addr*)&gw_public_IP ) );
    printf("local ip: %s\n", inet_ntoa( *(struct in_addr*)&gw_private_IP ) );
    printf("local mask: %s\n", inet_ntoa( *(struct in_addr*)&local_mask ) );
    printf("bucket size: %d\n", bucket_size);
    printf("fill rate: %f\n", fill_rate);
  }

  printf("-------NFQUEUE config-------\n");
  // Get a queue connection handle from the module
  struct nfq_handle *nfqHandle;
  if (!(nfqHandle = nfq_open())) {
    fprintf(stderr, "Error in nfq_open()\n");
    exit(-1);
  }else{
    printf("getting queue connection: OK\n");
  }

  // Unbind the handler from processing any IP packets
  if (nfq_unbind_pf(nfqHandle, AF_INET) < 0) {
    fprintf(stderr, "Error in nfq_unbind_pf()\n");
    exit(1);
  }else{
    printf("Unbinding handler from processing any IP packets: OK\n");
  }

  // Install a callback on queue 0
  struct nfq_q_handle *nfQueue;
  if (!(nfQueue = nfq_create_queue(nfqHandle,  0, &Callback, NULL))) {
    fprintf(stderr, "Error in nfq_create_queue()\n");
    exit(1);
  }else{
    printf("installing callback on queue: OK\n");
  }
  // nfq_set_mode: I want the entire packet 
  if(nfq_set_mode(nfQueue, NFQNL_COPY_PACKET, BUF_SIZE) < 0) {
    fprintf(stderr, "Error in nfq_set_mode()\n");
    exit(1);
  }else{
    printf("nfq_set_mode: Entire packet: OK\n");
  }
  printf("----------------------------\n");
  struct nfnl_handle *netlinkHandle;
  netlinkHandle = nfq_nfnlh(nfqHandle);

  int fd;
  fd = nfnl_fd(netlinkHandle);

  int res;
  char buf[BUF_SIZE];

  //traffic shaping
  gettimeofday(&begin_Time, NULL);

  while ((res = recv(fd, buf, sizeof(buf), 0)) && res >= 0) 
  {
    nfq_handle_packet(nfqHandle, buf, res);
  }

  nfq_destroy_queue(nfQueue);
  nfq_close(nfqHandle);

}
