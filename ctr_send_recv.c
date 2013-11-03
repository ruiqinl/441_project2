#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <assert.h>
#include "process_udp.h"
#include "list.h"
#include "ctr_send_recv.h"
#include "debug.h"
#include "spiffy.h"

/* one outbound_list for out non-data packet, a list of data_wnd for out data packet
 * one inbound_list for in packet, a list of flow_wnd for in data packet
 */
static struct list_t *inbound_list = NULL;
static struct list_t *outbound_list = NULL;

static struct list_t *data_wnd_list = NULL;
static struct list_t *flow_wnd_list = NULL;

void init_ctr() {
    init_list(&inbound_list);
    init_list(&outbound_list);
    init_list(&data_wnd_list);
    init_list(&flow_wnd_list);
}

void init_data_wnd(struct data_wnd_t **wnd) {
    
    *wnd = (struct data_wnd_t *)calloc(1, sizeof(struct data_wnd_t));

    (*wnd)->connection_peer_id = -1;

    init_list(&(*wnd)->packet_list);
    (*wnd)->last_packet_acked = -1;
    (*wnd)->last_packet_sent = -1;
    (*wnd)->last_packet_avai = -1;

    (*wnd)->capacity = INIT_WND_SIZE; // when do congestion control, window size changes
    
}

void init_flow_wnd(struct flow_wnd_t **wnd) {

    *wnd = (struct flow_wnd_t *)calloc(1, sizeof(struct flow_wnd_t));

    (*wnd)->connection_peer_id = -1;    

    init_list(&(*wnd)->packet_list);
    (*wnd)->last_packet_read = -1;
    (*wnd)->next_packet_expec = -1;
    (*wnd)->last_packet_recv = -1;

    (*wnd)->capacity = INIT_WND_SIZE;
    
}

int enlist_data_wnd(struct data_wnd_t *wnd, struct packet_info_t *info) {
    int id;
    
    id = peerlist_id(info->peer_list);
    
    if (wnd->connection_peer_id == -1) {
	DPRINTF(DEBUG_CTR, "enlist_data_wnd: first enlist, set conneciton_peer_id to %d\n", id);
	wnd->connection_peer_id = id;
    }

    assert(id == wnd->connection_peer_id);

    enlist(wnd->packet_list, info);
    
    if (wnd->last_packet_avai < wnd->capacity) {
	(wnd->last_packet_avai)++;
	DPRINTF(DEBUG_CTR, "enlist_data_wnd: wnd_%d, avai increase to: %d\n", wnd->connection_peer_id, wnd->last_packet_avai);
	return 0;
    }
    DPRINTF(DEBUG_CTR, "enlist_data_wnd: wnd_%d, avai remains unchanged: %d\n", wnd->connection_peer_id, wnd->last_packet_avai);
	return 0;
}

int general_send(int sock) {
    int count = 0;

    printf("general_send: try outbound_list_send:\n");
    if ((count = outbound_list_send(sock)) != 0) {
	assert(count == 1);
	printf("general_send: outbound_list_send sends a packet\n");
	return 0;
    }

    printf("general_send: try data_wnd_list_send:\n");
    if ((count = data_wnd_list_send(sock)) != 0) {
	assert(count == 1);
	printf("general_send: data_wnd_list_send sends a packet\n");
	return 0;
    }
    
    return 0;
}

/* Return -1 on type error, 0 on sending no packets, 1 on sending a packet */
int outbound_list_send(int sock) {
    assert(outbound_list != NULL);

    return process_outbound_udp(sock, outbound_list);    
}

/* Traverse all data_list, and try to send a packet out
 * Return 1 on successfully sending one packet out
 * Return 0 on sending no packet out
 */
int data_wnd_list_send(int sock) {
    struct data_wnd_t *data_wnd = NULL;
    struct list_item_t *iterator = NULL;

    iterator = get_iterator(data_wnd_list);
        
    while (has_next(iterator)) {
	data_wnd = next(&iterator);
	printf("data_wnd_list_send: wnd_%d\n", data_wnd->connection_peer_id);
	if (data_wnd_send(sock, data_wnd) == 1) {
	    // send a packet inside data_list out
	    return 1;
	}
    }

    // did not send any packet out
    return 0;
}


/* Send the a packet if exits
 * Return 1 on sending a packet, 0 on send no packet or such packet does not exist
 */
int data_wnd_send(int sock, struct data_wnd_t *wnd) {
    // last_packet_acked/avai/sent
    struct packet_info_t *info = NULL;
    int id;
    
    id = wnd->connection_peer_id;

    printf("data_wnd_send: wnd_%d, before sending, sent:%d avai:%d\n", id, wnd->last_packet_sent, wnd->last_packet_avai);
    //while (wnd->last_packet_sent < wnd->last_packet_avai) { // this line for test
    if (wnd->last_packet_sent < wnd->last_packet_avai) {

	info = list_ind(wnd->packet_list, wnd->last_packet_sent+1);
	if (send_info(sock, info) == 1) {

	    (wnd->last_packet_sent)++;
	    printf("data_wnd_send: wnd_%d, after sending, sent:%d avai:%d\n", id, wnd->last_packet_sent, wnd->last_packet_avai);// DPRINTF????
	    return 1;
	}

    } 

    DPRINTF(DEBUG_CTR, "data_wnd_send: send no packet\n");
    return 0;
}

    
/* Return 0 on success, -1 on error  */
int outbound_list_cat(struct list_t *out_list) {

    if (out_list == NULL)
	return 0;

    if (cat_list(&outbound_list, &out_list) != NULL)
	return 0;

    return -1;
}

/* Return 0 on success, -1 on error  */
int outbound_list_en(void *data) {
    assert(data != NULL);
    assert(outbound_list != NULL);

    if (enlist(outbound_list, data) != NULL)
	return 0;
    return -1;
}

int general_enlist(struct packet_info_t *info) {
    assert(info != NULL);

    switch(info->type) {
    case WHOHAS:
    case IHAVE:
    case GET:
    case ACK:
    case DENIED:
	DPRINTF(DEBUG_CTR, "general_enlist: outbound_list_en\n");
	return outbound_list_en(info);
	break;
    case DATA:
	DPRINTF(DEBUG_CTR, "general_enlist: data_wnd_list_en\n");
	return data_wnd_list_en(info);
	break;
    default:
	DPRINTF(DEBUG_CTR, "general_enlist: wrong type\n");
	break;
    }
    
    return 0;
}

/* Enlist DATA packet_info
 * Find an existing wnd_list, or create one
 */
int data_wnd_list_en(struct packet_info_t *info) {
   
    struct list_item_t *iterator = NULL;
    struct data_wnd_t *data_wnd = NULL;
    int found = 0;
    int id;

    id = peerlist_id(info->peer_list);

    iterator = get_iterator(data_wnd_list);
    while (has_next(iterator)) {
	data_wnd = next(&iterator);
	if (id == data_wnd->connection_peer_id) {
	    DPRINTF(DEBUG_CTR, "data_wnd_list_en: find existing data_wnd with id=%d\n", id);
	    enlist_data_wnd(data_wnd, info);
	    
	    found = 1;
	    break;
	}
    }
    
    if (!found) {
	DPRINTF(DEBUG_CTR, "data_wnd_list_en: no existing data_wnd with id=%d, create a new data_wnd\n", id);
	
	data_wnd = NULL;
	init_data_wnd(&data_wnd);
	enlist_data_wnd(data_wnd, info);

	enlist(data_wnd_list, data_wnd);
    }

    return 0;
}


/* Traverse send_list, base on packet_info type to decide put each packet_info to outbound_list directly, or  
int ctr_send(int sock) {

    return process_outbound_udp(sock, outbound_list);

}
*/

/* Recv packet, return packet_info */
struct packet_info_t *general_recv(int sock, bt_config_t *config) {

    uint8 buf[MAX_PACKET_LEN+1];
    socklen_t addr_len;
    struct sockaddr_in *addr = NULL;
    struct packet_info_t *info = NULL;
    bt_peer_t *peer = NULL;
    
    addr = (struct sockaddr_in *)calloc(1, sizeof(struct sockaddr_in));
    addr_len = sizeof(struct sockaddr);    
    
    spiffy_recvfrom(sock, buf, MAX_PACKET_LEN, 0, (struct sockaddr *)addr, &addr_len);
    
    info = packet2info(buf);

    // identify the peer sending the packet
    peer = addr2peer(config, addr); 
    init_list(&(info->peer_list));
    enlist(info->peer_list, peer);
    assert(info->peer_list->length == 1);

    if (debug & DEBUG_PROCESS_UDP) {
	printf("\nprocess_inboud_udp: received packet\n");
	info_printer(info);
    }

    return info;
}


#ifdef _TEST_CTR_
int main(){
    
    struct data_wnd_t *data_wnd_1 = NULL;
    struct data_wnd_t *data_wnd_2 = NULL;
    struct data_wnd_t *data_wnd_3 = NULL;
    
    struct packet_info_t *info_1 = NULL;
    struct packet_info_t *info_2 = NULL;
    struct packet_info_t *info_3 = NULL;
    struct packet_info_t *info_4 = NULL;

    init_ctr();
    init_data_wnd(&data_wnd_1);
    init_data_wnd(&data_wnd_2);
    init_data_wnd(&data_wnd_3);

    // addr
    int sock;
    struct sockaddr_in myaddr;
    
    if ((sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP)) == -1) {
	perror("peer_run could not create socket");
	exit(-1);
    }
  
    memset(&myaddr, 0, sizeof(myaddr));
    myaddr.sin_family = AF_INET;
    myaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    myaddr.sin_port = htons(9999);

    bt_peer_t peer1;
    peer1.id = 1;
    peer1.addr = myaddr;
    peer1.next = NULL;
    
    bt_peer_t peer2;
    peer2.id = 2;
    peer2.addr = myaddr;
    peer2.next = NULL;
    
    struct list_t *peer_list1 = NULL;
    init_list(&peer_list1);
    enlist(peer_list1, &peer1);

    struct list_t *peer_list2 = NULL;
    init_list(&peer_list2);
    enlist(peer_list2, &peer2);

    //
    info_1 = (struct packet_info_t *)calloc(1, sizeof(struct packet_info_t));
    info_1->type = DATA;
    info_1->peer_list = peer_list1;

    info_2 = (struct packet_info_t *)calloc(1, sizeof(struct packet_info_t));
    info_2->type = DATA;
    info_2->peer_list = peer_list2;

    info_3 = (struct packet_info_t *)calloc(1, sizeof(struct packet_info_t));
    info_3->type = DATA;
    info_3->peer_list = peer_list2;

    info_4 = (struct packet_info_t *)calloc(1, sizeof(struct packet_info_t));
    info_4->type = DATA;
    info_4->peer_list = peer_list1;

    // general
    general_enlist(info_1);
    general_enlist(info_2);
    general_enlist(info_3);
    general_enlist(info_4);

    enlist(data_wnd_list, data_wnd_3);

    // not general
    //    enlist_data_wnd(data_wnd_1, info_1);
    //    enlist_data_wnd(data_wnd_2, info_2);
    //    enlist_data_wnd(data_wnd_2, info_3);
    //    enlist_data_wnd(data_wnd_2, info_4);
    // data_wnd_3 empty

    //enlist(data_wnd_list, data_wnd_1);
    //enlist(data_wnd_list, data_wnd_2);
    //enlist(data_wnd_list, data_wnd_3);


    // send
    if (bind(sock, (struct sockaddr *) &myaddr, sizeof(myaddr)) == -1) {
	perror("peer_run could not bind socket");
	exit(-1);
    }

    data_wnd_list_send(sock);

    return 0;
}
#endif
