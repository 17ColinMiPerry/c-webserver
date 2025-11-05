#ifndef _NET_H_
#define _NET_H_

#include <sys/socket.h>

void *get_in_addr(struct sockaddr *sa);
int get_listener_socket(char *port);

#endif
