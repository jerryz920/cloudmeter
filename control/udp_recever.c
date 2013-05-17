/******************************************************************************

 @File Name : {PROJECT_DIR}/server.c

 @Creation Date : 06-05-2013

 @Created By: Zhai Yan

 @Purpose :

*******************************************************************************/


#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <syslog.h>
#include <inttypes.h>
#include <errno.h>

/*
 *  We will use fixed-length packet
 */
#define PKT_LEN 512

static int server_loop(int s)
{
  uint64_t total_byte;
  uint64_t total_pkt;
  openlog("udp_sink", LOG_CONS, LOG_USER);

  syslog(LOG_NOTICE, "beginning server loop\n");
  while (1) {
    struct sockaddr_in caddr;
    socklen_t caddrlen = sizeof(caddr);
    char pktbuf[PKT_LEN];

    /*
     *  Just a sink server
     */
    int amount = recvfrom(s, pktbuf, PKT_LEN, 0, &caddr, &caddrlen);
    if (amount < 0) {
      syslog(LOG_ERR, "error in receiving: %s", strerror(errno));
      continue;
    }

    total_byte += amount;
    total_pkt++;

    if ((total_pkt + 1) % 1001 == 0) {
      syslog(LOG_DEBUG, "received %"PRIu64" packets, %"PRIu64" bytes\n",
          total_pkt, total_byte);
    }
  }

  closelog();
  /*
   *  Should not return
   */
  return -1;
}

int main(int argc, char** argv)
{
  int i;
  if (argc < 2) {
    fprintf(stderr, "usage: ./server <port> \n");
    return -1;
  }

  struct addrinfo hint, *res = NULL;

  hint.ai_family = AF_INET;
  hint.ai_flags = AI_PASSIVE;
  hint.ai_protocol = IPPROTO_UDP;
  hint.ai_socktype = SOCK_DGRAM;

  int ret = getaddrinfo("0.0.0.0", argv[1], &hint, &res);
  if (ret != 0) {
    fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(ret));
    return -2;
  }

  int s = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
  if (s < 0) {
    perror("socket");
    return -3;
  }

  if (bind(s, res->ai_addr, res->ai_addrlen) < 0) {
    perror("bind");
    return -4;
  }
  freeaddrinfo(res);

  return server_loop(s);
}
