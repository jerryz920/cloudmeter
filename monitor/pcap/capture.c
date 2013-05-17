/******************************************************************************

 @File Name : {PROJECT_DIR}/capture.c

 @Creation Date : 05-12-2013

 @Created By: Zhai Yan

 @Purpose :

*******************************************************************************/

#include <pcap.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <inttypes.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/if_ether.h> 
#include <net/ethernet.h>
#include <netinet/ether.h> 
#include <netinet/ip.h> 
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <ifaddrs.h>
#include <fcntl.h>
#include <sys/file.h>
#include <time.h>


/* tcpdump header (ether.h) defines ETHER_HDRLEN) */
#ifndef ETHER_HDRLEN 
#define ETHER_HDRLEN 14
#endif

typedef enum {
  TOTAL = 0,
  PUBLIC,
  PRIVATE,
  META,
  SSH,
  HTTP,
  TARGET,
  EC2_FLOW_TYPE_NUM
} EC2FlowType ;


typedef struct EC2Stat {
  uint64_t bytes_counter[EC2_FLOW_TYPE_NUM];
  uint64_t pkt_counter[EC2_FLOW_TYPE_NUM];
} EC2Stat;

static void init_ec2_stat(EC2Stat* s)
{
  memset(s, 0, sizeof(*s));
}

static void fout_ec2_stat(EC2Stat* s_stat, EC2Stat* r_stat, const char* outfile)
{
  /*
   *  First get lock of the file then begin to write
   */
  FILE* fout = fopen(outfile, "w");
  if (!fout) {
    perror("fopen");
    return ;
  }
  int fd = fileno(fout);
  int retry_count = 5;

retry:
  if (flock(fd, LOCK_EX) < 0) {
    switch(errno) {
      case EINTR: case EAGAIN:
        if (retry_count--)
          goto retry;
      default:
        /*
         *  Give up update this time.
         */
        perror("locking");
        return;
    }
  }

  int i;

  for (i = 0; i < EC2_FLOW_TYPE_NUM; i++) {
    fprintf(fout, "%"PRIu64" %"PRIu64" ", s_stat->pkt_counter[i], s_stat->bytes_counter[i]);
  }
  fprintf(fout, "\n");
  for (i = 0; i < EC2_FLOW_TYPE_NUM; i++) {
    fprintf(fout, "%"PRIu64" %"PRIu64" ", r_stat->pkt_counter[i], r_stat->bytes_counter[i]);
  }
  fprintf(fout, "\n");
  flock(fd, LOCK_UN);
  fclose(fout);
}

static uint32_t sniffer_addr = -1;
static uint16_t target_flow_port = 1985;
static uint32_t meta_addr = 0xfea9fea9;
static time_t last_update = 0;
static struct EC2Stat r_stat;
static struct EC2Stat s_stat;
static char* stat_path = "/var/run/cloudmeter/netstat";

static int is_private_addr(uint32_t addr)
{
  return (addr & 0xa) != 0;
}

static int is_meta_addr(uint32_t addr)
{
  return addr == meta_addr;
}

static uint32_t fetch_ifaddr(const char* dev)
{
  struct ifaddrs* addrs;
  struct ifaddrs* head;
  uint32_t devaddr = -1;

  if (getifaddrs(&addrs) < 0) {
    perror("getifaddrs");
    return -1;
  }

  head = addrs;
  while (head) {
    if (strcmp(head->ifa_name, dev) == 0) {
      if (head->ifa_addr->sa_family == AF_INET) {
        devaddr = ((struct sockaddr_in*)head->ifa_addr)->sin_addr.s_addr;
        goto cleanup;
      }
    }
    head = head->ifa_next;
  }

cleanup:
  freeifaddrs(addrs);
  return devaddr;
}

static void update_r_stat(struct ip* iph)
{
  struct tcphdr* tcph =(struct tcphdr*) ((char*) iph + 4 * iph->ip_hl);
  struct udphdr* udph =(struct udphdr*) ((char*) iph + 4 * iph->ip_hl);
  uint64_t total_size = ntohs(iph->ip_len) + ETHER_HDRLEN;

  r_stat.bytes_counter[TOTAL] += total_size;
  r_stat.pkt_counter[TOTAL]++;

  if (iph->ip_p == IPPROTO_UDP && 
      (udph->source == htons(target_flow_port) || udph->dest == htons(target_flow_port))) {
    r_stat.bytes_counter[TARGET] += total_size;
    r_stat.pkt_counter[TARGET]++;

  } else if (iph->ip_p == IPPROTO_TCP &&
      tcph->source == htons(80)) {

    if (is_meta_addr(iph->ip_src.s_addr)) {
      r_stat.bytes_counter[META] += total_size;
      r_stat.pkt_counter[META]++;
    } else {
      r_stat.bytes_counter[HTTP] += total_size;
      r_stat.pkt_counter[HTTP]++;
    }

  } else if (iph->ip_p == IPPROTO_TCP &&
      tcph->dest == htons(22)) {
    r_stat.bytes_counter[SSH] += total_size;
    r_stat.pkt_counter[SSH]++;

  } else if (is_private_addr(iph->ip_src.s_addr))  {
    r_stat.bytes_counter[PRIVATE] += total_size;
    r_stat.pkt_counter[PRIVATE]++;

  } else {
    r_stat.bytes_counter[PUBLIC] += total_size;
    r_stat.pkt_counter[PUBLIC]++;
  }
}

static void update_s_stat(struct ip* iph)
{
  struct tcphdr* tcph =(struct tcphdr*) ((char*) iph + 4 * iph->ip_hl);
  struct udphdr* udph =(struct udphdr*) ((char*) iph + 4 * iph->ip_hl);
  uint64_t total_size = ntohs(iph->ip_len) + ETHER_HDRLEN;

  s_stat.bytes_counter[TOTAL] += total_size;
  s_stat.pkt_counter[TOTAL]++;

  if (iph->ip_p == IPPROTO_UDP && 
      (udph->source == htons(target_flow_port) || udph->dest == htons(target_flow_port))) {
    s_stat.bytes_counter[TARGET] += total_size;
    s_stat.pkt_counter[TARGET]++;

  } else if (iph->ip_p == IPPROTO_TCP &&
      tcph->dest == htons(80)) {
    if (is_meta_addr(iph->ip_dst.s_addr)) {
      s_stat.bytes_counter[META] += total_size;
      s_stat.pkt_counter[META]++;
    } else {
      s_stat.bytes_counter[HTTP] += total_size;
      s_stat.pkt_counter[HTTP]++;
    }

  } else if (iph->ip_p == IPPROTO_TCP &&
      tcph->source == htons(22)) {
    s_stat.bytes_counter[SSH] += total_size;
    s_stat.pkt_counter[SSH]++;

  } else if (is_private_addr(iph->ip_dst.s_addr))  {
    s_stat.bytes_counter[PRIVATE] += total_size;
    s_stat.pkt_counter[PRIVATE]++;

  } else {
    s_stat.bytes_counter[PUBLIC] += total_size;
    s_stat.pkt_counter[PUBLIC]++;
  }

}

static void update_stat(struct ip* iph)
{
  time_t cur = time(NULL);
  if (iph->ip_dst.s_addr == sniffer_addr) {
    update_r_stat(iph);
  } else {
    update_s_stat(iph);
  }

  /*
   *  FIXME: very bad hard coding. But then what?
   */
  if (cur - last_update > 25) {
    fout_ec2_stat(&s_stat, &r_stat, stat_path);
    last_update = cur;
  }
}


void handle_IP(u_char *args,const struct pcap_pkthdr* pkthdr,const u_char* packet)
{
  struct ip* ip;
  u_int version;

  /* jump pass the ethernet header */
  ip = (struct ip*)(packet + sizeof(struct ether_header));

  /* check to see we have a packet of valid length */
  if (pkthdr->len < sizeof(struct ip)) {
    return ;
  }

  version = ip->ip_v;/* ip version */
  /* check version */
  if(version != 4) {
    fprintf(stdout,"Unknown version %d\n",version);
    return ;
  }

  update_stat(ip);
}

/* handle ethernet packets, much of this code gleaned from
 *  * print-ether.c from tcpdump source
 *   */
u_int16_t handle_ethernet
(u_char *args,const struct pcap_pkthdr* pkthdr,const u_char*
 packet)
{
  u_int caplen = pkthdr->caplen;
  struct ether_header *eptr;  /* net/ethernet.h */
  u_short ether_type;

  if (caplen < ETHER_HDRLEN)
  {
    fprintf(stderr,"Packet length less than ethernet header length\n");
    return -1;
  }
  /* lets start with the ether header... */
  eptr = (struct ether_header *) packet;
  ether_type = ntohs(eptr->ether_type);

  return ether_type;
}

/* looking at ethernet headers */
void my_callback(u_char *args,const struct pcap_pkthdr* pkthdr,const u_char*
    packet)
{
  u_int16_t type = handle_ethernet(args,pkthdr,packet);

  if(type == ETHERTYPE_IP)
  {/* handle IP packet */
    handle_IP(args,pkthdr,packet);
  }
}

int main(int argc,char **argv)
{ 
  char *dev; 
  char errbuf[PCAP_ERRBUF_SIZE];
  pcap_t* descr;
  bpf_u_int32 maskp;          /* subnet mask               */
  bpf_u_int32 netp;           /* ip                        */
  u_char* args = NULL;

  /* Options must be passed in as a string because I am lazy */
  if(argc < 2){ 
    fprintf(stdout,"Usage: %s stat_path [device]\n",argv[0]);
    return 0;
  }

  init_ec2_stat(&s_stat);
  init_ec2_stat(&r_stat);
  stat_path = argv[1];
  sniffer_addr = fetch_ifaddr(argc > 2? argv[2]: "eth0");
  if (sniffer_addr == -1) {
    fprintf(stderr, "can not fetch %s address\n", argc > 2? argv[2]: "eth0");
    exit(2);
  }

  /* grab a device to peak into... */
  dev = pcap_lookupdev(errbuf);
  if(dev == NULL) {
    fprintf(stderr, "%s\n",errbuf);
    exit(1); 
  }


  /* ask pcap for the network address and mask of the device */
  pcap_lookupnet(dev,&netp,&maskp,errbuf);

  /* open device for reading. NOTE: defaulting to
   *      * promiscuous mode*/
  descr = pcap_open_live(dev,1024,1,10,errbuf);
  if(descr == NULL) { 
    fprintf(stderr, "pcap_open_live(): %s\n",errbuf);
    exit(1); 
  }

/*
  if(argc > 2) {
    if(pcap_compile(descr,&fp,argv[2],0,netp) == -1)
    { fprintf(stderr,"Error calling pcap_compile\n"); exit(1); }

    if(pcap_setfilter(descr,&fp) == -1)
    { fprintf(stderr,"Error setting filter\n"); exit(1); }
  }
  */

  /* ... and loop */ 
  pcap_loop(descr,atoi(argv[1]),my_callback,args);

  fprintf(stdout,"\nfinished\n");
  return 0;
}

