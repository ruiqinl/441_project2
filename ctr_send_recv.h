#ifndef _CTR_SEND_RECV_H_
#define _CTR_SEND_RECV_H_

#include "list.h"

int ctr_enlist(struct list_t *out_list);
int ctr_send(int sock);
int ctr_recv(int sock, struct list_t *in_list);


#endif
