#ifndef _CTR_SEND_RECV_H_
#define _CTR_SEND_RECV_H_

#define INIT_WND_SIZE 2

#include "list.h"

struct data_wnd_t {
    
    int connection_peer_id;

    struct list_t *packet_list; // all outbound packet here
    
    int last_packet_acked;
    int last_packet_sent;
    int last_packet_avai;

    int capacity;
};

struct flow_wnd_t {
    
    int connection_peer_id;   

    struct list_t *packet_list;
   
    int last_packet_read;
    int next_packet_expec;
    int last_packet_recv;

    int capacity;
};

void init_ctr();
void init_data_wnd(struct data_wnd_t **wnd);
void init_flow_wnd(struct flow_wnd_t **wnd);
int enlist_data_wnd (struct data_wnd_t *wnd, struct packet_info_t *info);

int general_send(int sock);
struct packet_info_t *general_recv(int sock, bt_config_t *config);

int outbound_list_send(int sock);
int data_wnd_list_send(int sock);
int data_wnd_send(int sock, struct data_wnd_t *wnd);
//int ctr_enlist(struct list_t *out_list);

int outbound_list_cat(struct list_t *out_list);
int outbound_list_en(void *data);

int general_enlist(struct packet_info_t *info);
int data_wnd_list_en(struct packet_info_t *info);

#endif
