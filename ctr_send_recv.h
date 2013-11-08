#ifndef _CTR_SEND_RECV_H_
#define _CTR_SEND_RECV_H_

#define INIT_WND_SIZE 1
#define INIT_SSTHRESH_SIZE 64
#define TIME_OUT_DUR 2
#define SLOW_START 0
#define CONG_AVOID 1
#define RTT 2
#define TICKSPERSEC 1000 //1000 clicks = 1 second

#include <time.h>
#include "list.h"

struct data_wnd_t {
    
    int connection_peer_id;

    struct list_t *packet_list; // all outbound packet here
    
    int last_packet_acked;
    int last_packet_sent;
    int last_packet_avai;

    //int capacity;
    int wnd_size;
    int ssthresh;
    int mode; // 1 if it's in congestion avoidance mode
    time_t start_t; //for time out function
    clock_t timer; // for RTT estimation
};

struct flow_wnd_t {
    
    struct list_t *packet_list;
   
    int last_packet_read;
    int next_packet_expec;
    int last_packet_recv; 

    //int capacity; // recv - expec
    int wnd_size;

    uint8 hash[HASH_LEN];
};

void init_ctr();
void init_data_wnd(struct data_wnd_t **wnd);
void init_flow_wnd(struct flow_wnd_t **wnd);

int new_flow_wnd(); // call init_flow_wnd to add a new flow_wnd

int general_send(int sock);
struct packet_info_t *general_recv(int sock, bt_config_t *config);

int outbound_list_send(int sock);
int data_wnd_list_send(int sock);
int data_wnd_send(int sock, struct data_wnd_t *wnd);
//int ctr_enlist(struct list_t *out_list);

struct data_wnd *search_data_wnd(int id);


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

int check_out_size();


struct list_t* do_inbound_ACK(struct packet_info_t *info);

/* flow control below  */
int enlist_DATA_info(struct packet_info_t *info, struct GET_request_t *GET_req);
void update_flow_wnd(struct flow_wnd_t *wnd);

//int is_fully_received();
int is_fully_received(struct flow_wnd_t *wnd, uint8 *slot_hash, uint8 **received_chunk);
void set_time(int sentID);
int timeout_check();

#endif
