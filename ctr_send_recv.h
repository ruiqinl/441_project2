#ifndef _CTR_SEND_RECV_H_
#define _CTR_SEND_RECV_H_

#define INIT_WND_SIZE 8

#include "list.h"

struct cong_list_t {
    struct list_t *outbound_list; // all outbound packet here
    
    struct list_item_t *last_packet_acked;
    struct list_item_t *last_packet_sent;
    struct list_item_t *last_packet_avai;

    int wnd_size;
};

struct flow_list_t {
    struct list_t *inbound_list;
   
    struct list_item_t *last_packet_read;
    struct list_item_t *next_packet_expec;
    struct list_item_t *last_packet_recv;
};

void init_inout(void);
void init_cong(struct cong_list_t **cong_list);
void init_flow(struct flow_list_t **flow_list);
int ctr_enlist(struct list_t *out_list);
int ctr_send(int sock);
int ctr_recv(int sock, bt_config_t *config);


#endif
