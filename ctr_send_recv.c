#include <stdio.h>
#include <stdlib.h>
#include "process_udp.h"
#include "list.h"
#include "ctr_send_recv.h"

static struct list_t *outbound_list = NULL;
static struct list_t *inbound_list = NULL;

void init_inout() {
    init_list(&outbound_list);
    init_list(&inbound_list);
}

void init_cong(struct cong_list_t **cong_list) {
    *cong_list = (struct cong_list_t *)calloc(1, sizeof(struct cong_list_t));
    
    //init_list(&(cong_list->outbound_list));
    
    (*cong_list)->last_packet_acked = NULL;
    (*cong_list)->last_packet_sent = NULL;
    (*cong_list)->last_packet_avai = NULL;

    (*cong_list)->wnd_size = INIT_WND_SIZE; // when do congestion control, window size changes
    
}

void init_flow(struct flow_list_t **flow_list) {
    *flow_list = (struct flow_list_t *)calloc(1, sizeof(struct flow_list_t));
    
    //init_list(&(flow_list->inbound_list));
    
    (*flow_list)->last_packet_read = NULL;
    (*flow_list)->next_packet_expec = NULL;
    (*flow_list)->last_packet_recv = NULL;
    
}

    
int ctr_enlist(struct list_t *out_list) {

    cat_list(&outbound_list, &out_list);
    return 0;
}

/* Traverse send_list, base on packet_info type to decide put each packet_info to outbound_list directly, or   */
int ctr_send(int sock) {

    return process_outbound_udp(sock, outbound_list);

}

int ctr_recv(int sock, bt_config_t *config) {
    
    return process_inbound_udp(sock, config, outbound_list);

}


