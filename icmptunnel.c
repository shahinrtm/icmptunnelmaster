/**
 * icmp_tunnel.c
 */

#include "tunnel.h"
#include "socks5.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#define ARG_SERVER_MODE "-s"
#define ARG_CLIENT_MODE "-c"
#define ARG_CS_MODE     "-cs"

void usage()
{
  printf("Wrong argument\n");
  fprintf(stdout, "usage: icmptunnel [-s serverip] | [-c clientip] | [-cs clientip]\n");
}

struct tunnel_thread_arg {
  char ip[100];
};

static void *tunnel_thread(void *arg)
{
  struct tunnel_thread_arg *t = (struct tunnel_thread_arg *)arg;
  char ip[100];
  memcpy(ip, t->ip, sizeof(ip));
  free(t);
  run_tunnel(ip, 0);
  return NULL;
}

int main(int argc, char *argv[])
{
  char ip_addr[100] = {0,};
  if ((argc < 3) || ((strlen(argv[2]) + 1) > sizeof(ip_addr))) {
    usage();
    exit(EXIT_FAILURE);
  }
  memcpy(ip_addr, argv[2], strlen(argv[2]) + 1);

  if (strncmp(argv[1], ARG_SERVER_MODE, strlen(argv[1])) == 0) {
    run_tunnel(ip_addr, 1);
  }
  else if (strncmp(argv[1], ARG_CLIENT_MODE, strlen(argv[1])) == 0) {
    run_tunnel(ip_addr, 0);
  }
  else if (strncmp(argv[1], ARG_CS_MODE, strlen(argv[1])) == 0) {
    pthread_t tid;
    struct tunnel_thread_arg *param = malloc(sizeof(*param));
    if (!param) {
      perror("malloc");
      exit(EXIT_FAILURE);
    }
    strncpy(param->ip, ip_addr, sizeof(param->ip) - 1);
    if (pthread_create(&tid, NULL, tunnel_thread, param) != 0) {
      perror("pthread_create");
      free(param);
      exit(EXIT_FAILURE);
    }
    run_socks5_server(2020);
  }
  else {
    usage();
    exit(EXIT_FAILURE);
  }

  return EXIT_SUCCESS;
}
