/******************************************************************************

 @File Name : {PROJECT_DIR}/parser.c

 @Creation Date : 05-17-2013

 @Created By: Zhai Yan

 @Purpose :

*******************************************************************************/

#include <stdio.h>
#include <string.h>
#include <inttypes.h>

int main()
{
  uint64_t dummy[14];
  uint64_t s_total, s_private_other, s_public_other, s_ssh, s_meta, s_target, s_http;
  uint64_t r_total, r_private_other, r_public_other, r_ssh, r_meta, r_target, r_http;

  scanf("%"SCNu64"%"SCNu64"%"SCNu64"%"SCNu64"%"SCNu64"%"SCNu64"%"SCNu64"%"SCNu64"%"SCNu64"%"SCNu64"%"SCNu64"%"SCNu64"%"SCNu64"%"SCNu64"",
      &dummy[0], &s_total, 
      &dummy[1], &s_private_other, 
      &dummy[2], &s_public_other, 
      &dummy[3], &s_target, 
      &dummy[4], &s_http, 
      &dummy[5], &s_ssh, 
      &dummy[6], &s_meta);

  scanf("%"SCNu64"%"SCNu64"%"SCNu64"%"SCNu64"%"SCNu64"%"SCNu64"%"SCNu64"%"SCNu64"%"SCNu64"%"SCNu64"%"SCNu64"%"SCNu64"%"SCNu64"%"SCNu64" ",
      &dummy[0], &r_total, 
      &dummy[1], &r_private_other, 
      &dummy[2], &r_public_other, 
      &dummy[3], &r_target, 
      &dummy[4], &r_http, 
      &dummy[5], &r_ssh, 
      &dummy[6], &r_meta);

  printf("\noutbound:\n");
  printf("total: %"PRIu64"\n", s_total);
  printf("target: %"PRIu64"\n", s_target);
  printf("public: %"PRIu64"\n", s_public_other + s_ssh + s_http);
  printf(" ssh: %"PRIu64"\n", s_ssh);
  printf(" http: %"PRIu64"\n", s_http);
  printf(" other: %"PRIu64"\n", s_public_other);
  printf("private: %"PRIu64"\n", s_private_other + s_meta);
  printf(" meta: %"PRIu64"\n", s_meta);
  printf(" other: %"PRIu64"\n", s_private_other);

  printf("\ninbound:\n");
  printf("total: %"PRIu64"\n", r_total);
  printf("target: %"PRIu64"\n", r_target);
  printf("public: %"PRIu64"\n", r_public_other + r_ssh + r_http);
  printf(" ssh: %"PRIu64"\n", r_ssh);
  printf(" http: %"PRIu64"\n", r_http);
  printf(" other: %"PRIu64"\n", r_public_other);
  printf("private: %"PRIu64"\n", r_private_other + r_meta);
  printf(" meta: %"PRIu64"\n", r_meta);
  printf(" other: %"PRIu64"\n", r_private_other);

  char buffer[1024];
  printf("\nifconfig stats:\n");
  while (fgets(buffer, 1024, stdin) != NULL)
    if (buffer[0] != '\n')
      puts(buffer);

  return 0;
}

