#include <stdio.h>
#include "process_udp.h"
#include "list.h"
#include "ctr_send_recv.h"

static struct list_t *inbound_list = NULL;
static struct list_t *outbound_list = NULL;

void init_ctr() {
    init_list(&inbound_list);
    init_list(&outbound_list);
}
    
int ctr_enlist(struct list_t *out_list) {
    cat_list(outbound_list, out_list);
    return 0;
}



/* Traverse send_list, base on packet_info type to decide put each packet_info to outbound_list directly, or   */
int ctr_send(int sock) {
    
    return process_outbound_udp(sock, outbound_list);

}

int ctr_recv(int sock, struct list_t *in_list) {

    return 42;
}



