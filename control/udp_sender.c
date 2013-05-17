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
#define HDR_LEN 66 //(14 eth header + 20 + 8)

static int client_loop(int s, uint64_t total_byte)
{
  uint64_t i;
  uint64_t total_pkt = (total_byte + (PKT_LEN + HDR_LEN) - 1) / (PKT_LEN + HDR_LEN);
  printf("total pkt: %"PRIu64"\n", total_pkt);

  for (i = 0; i < total_pkt; i++) {
    char buf[PKT_LEN] = {1};
    send(s, buf, PKT_LEN, 0);
    if (i % 10 == 1)
      usleep(10000);
  }

  /*
   *  Should not return
   */
  return close(s);
}

int main(int argc, char** argv)
{
  int i;
  if (argc < 2) {
    fprintf(stderr, "usage: ./udp_sender <host> <port> <total_size>\n");
    return -1;
  }

  struct addrinfo hint, *res = NULL;

  hint.ai_family = AF_INET;
  hint.ai_flags = 0;
  hint.ai_protocol = IPPROTO_UDP;
  hint.ai_socktype = SOCK_DGRAM;

  int ret = getaddrinfo(argv[1], argv[2], &hint, &res);
  if (ret != 0) {
    fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(ret));
    return -2;
  }

  int s = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
  if (s < 0) {
    perror("socket");
    return -3;
  }

  if (connect(s, res->ai_addr, res->ai_addrlen) < 0) {
    perror("connect");
    return -4;
  }
  freeaddrinfo(res);

  return client_loop(s, strtoll(argv[3], NULL, 10));
}
