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


int general_send(int sock);
struct packet_info_t *general_recv(int sock, bt_config_t *config);

int outbound_list_send(int sock);
int data_wnd_list_send(int sock);
int data_wnd_send(int sock, struct data_wnd_t *wnd);
//int ctr_enlist(struct list_t *out_list);


// basic "enlist" functions: outbound_list_en, outbound_list_cat, enlsit_data_wnd

// *reply on enlist
int outbound_list_en(void *data); 

// *rely on cat_list
int outbound_list_cat(struct list_t *out_list); 

// *rely on enlist
int enlist_data_wnd (struct data_wnd_t *wnd, struct packet_info_t *info);

// rely on general_enlist
int general_list_cat(struct list_t *info_list);

// rely on outbound_list_en, data_wnd_list_en
int general_enlist(struct packet_info_t *info);

// rely on data_wnd_list_en
int data_wnd_list_cat(struct list_t *info_list);

// rely on enlist_data_wnd
int data_wnd_list_en(struct packet_info_t *info);

void check_out_size();
int more_out();

#endif
