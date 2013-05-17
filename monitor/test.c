/******************************************************************************

 @File Name : {PROJECT_DIR}/test.c

 @Creation Date : 06-05-2013

 @Created By: Zhai Yan

 @Purpose :

*******************************************************************************/

#include <pcap.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>


int main()
{
  in_addr_t s = inet_addr("10.1.2.3");
  if ((s & 0xa) == 0xa)
    printf("happy\n");

  printf("%x\n", s);
  return 0;
}
