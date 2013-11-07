#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <assert.h>
#include <tgmath.h>
#include "process_udp.h"
#include "list.h"
#include "ctr_send_recv.h"
#include "debug.h"
#include "spiffy.h"
#include "sha.h"
#include "chunk.h"


/* one outbound_list for out non-data packet, a list of data_wnd for out data packet
 * one inbound_list for in packet, a list of flow_wnd for in data packet
 */
static struct list_t *inbound_list = NULL; // all five kinds
static struct list_t *outbound_list = NULL; // non-data packets

static struct list_t *data_wnd_list = NULL;
static struct list_t *flow_wnd_list = NULL;

static struct list_t *ACK_list = NULL;

void init_ctr() {
    init_list(&inbound_list);
    init_list(&outbound_list);
    init_list(&data_wnd_list);
    init_list(&flow_wnd_list);
    init_list(&ACK_list);
}

// last_pack_xx starts from 0
void init_data_wnd(struct data_wnd_t **wnd) {
    
    *wnd = (struct data_wnd_t *)calloc(1, sizeof(struct data_wnd_t));

    (*wnd)->connection_peer_id = -1;

    init_list(&(*wnd)->packet_list);
    (*wnd)->last_packet_acked = 0;
    (*wnd)->last_packet_sent = 0;
    (*wnd)->last_packet_avai = 0;

    (*wnd)->capacity = INIT_WND_SIZE; // when do congestion control, window size changes
    
}

// packet_xx starts from 0
void init_flow_wnd(struct flow_wnd_t **wnd) {

    *wnd = (struct flow_wnd_t *)calloc(1, sizeof(struct flow_wnd_t));

    init_list(&(*wnd)->packet_list);
    (*wnd)->last_packet_read = 0; 
    (*wnd)->next_packet_expec = 1; // in initial state, next epxec is 0th packet
    (*wnd)->last_packet_recv = 0;

    (*wnd)->capacity = INIT_WND_SIZE;

    // push capacity # of empty info
    struct packet_info_t *info = NULL;
    int ind = 0;
    
    while (ind < (*wnd)->capacity) {
	ind++;
	info = NULL;
	init_packet_info(&info);
	enlist((*wnd)->packet_list, info);
    }
    (*wnd)->last_packet_recv = ind;

}

int check_out_size() {
    long size = 0;
    struct list_item_t *ite = NULL;
    struct data_wnd_t *wnd = NULL;
    
    ite = get_iterator(data_wnd_list);
    while (has_next(ite)) {
	wnd = next(&ite);
	size += wnd->packet_list->length;
    }

    size += outbound_list->length;

    return size;
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

/* return #left packet, so select knows if sock needs to be cleared from writefds
 * return -1 on error
 */
int general_send(int sock) {
    int count;

    // send
    printf("general_send: try outbound_list_send:\n");
    if ((count = outbound_list_send(sock)) != 0) {
	assert(count == 1);
	printf("general_send: outbound_list_send sends a packet\n");
    } else {
	printf("general_send: try data_wnd_list_send:\n");
	if ((count = data_wnd_list_send(sock)) != 0) {
	    assert(count == 1);
	    printf("general_send: data_wnd_list_send sends a packet\n");
	}
    }

    return check_out_size();
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
    struct list_item_t *old_iterator = NULL;

    iterator = get_iterator(data_wnd_list);
        
    while (has_next(iterator)) {

	old_iterator = iterator;

	data_wnd = next(&iterator);
	printf("data_wnd_list_send: try wnd_%d\n", data_wnd->connection_peer_id);
	//if (data_wnd->packet_list->length == 0) {
	if (data_wnd->last_packet_avai == data_wnd->last_packet_sent) {
	    printf("wnd_%d has no packet to send, delist it\n", data_wnd->connection_peer_id);
	    delist_item(data_wnd_list, old_iterator);
	} else {
	    printf("wnd_%d has packet to send, send it\n", data_wnd->connection_peer_id);
	    if (data_wnd_send(sock, data_wnd) == 1) {
		// send a packet inside data_list out
		return 1;
	    }
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

    assert(wnd->last_packet_sent < wnd->last_packet_avai);

    struct packet_info_t *info = NULL;
    int id;
    int list_size = get_list_size();
    
    id = wnd->connection_peer_id;

    printf("data_wnd_send: wnd_%d, before sending, sent:%d avai:%d\n", id, wnd->last_packet_sent, wnd->last_packet_avai);

    info = list_ind(wnd->packet_list, wnd->last_packet_sent); // no plus 1
    if (send_info(sock, info) == 1) {
	
	// slide window
	(wnd->last_packet_sent)++;
	//if (wnd->last_packet_avai < wnd->packet_list->length) 
	if (wnd->last_packet_avai < list_size) 
	    wnd->last_packet_avai += 1;


	printf("data_wnd_send: wnd_%d, after sending, sent:%d avai:%d\n", id, wnd->last_packet_sent, wnd->last_packet_avai);
	return 1;
    }

    DPRINTF(DEBUG_CTR, "data_wnd_send: send no packet\n");
    return 0;
}

    
/* Return 0 on success, -1 on error  */
int outbound_list_cat(struct list_t *out_list) {

    if (out_list == NULL)
	return 0;

    if (cat_list(&outbound_list, &out_list) != NULL) {
	return 0;
    }

    return -1;
}

/* Return 0 on success, -1 on error  */
int outbound_list_en(void *data) {
    assert(data != NULL);
    assert(outbound_list != NULL);
    
    if (enlist(outbound_list, data) != NULL) {
	return 0;
    }
    return -1;
}

int is_fully_received() {
    
    struct flow_wnd_t *wnd = NULL;
    struct packet_info_t *info = NULL;
    int list_size;
    int ind; 
    uint8 buf[CHUNK_SIZE];
    uint8 *p = NULL;
    int data_size;
    struct list_item_t *ite = NULL;

    printf("???????????????\n");
    
    if (flow_wnd_list->end == NULL) {
	DPRINTF(DEBUG_CTR, "is_fully_received: no flow_wnd, no need to check\n");
	return 0;
    }

    wnd = (struct flow_wnd_t *)flow_wnd_list->end->data;
    assert(wnd != NULL);

    // check size
    list_size = get_list_size();

    DPRINTF(DEBUG_CTR, "is_fully_received: last_pack_read:%d, list_size:%d\n", wnd->last_packet_read, list_size);
    if (wnd->last_packet_read != list_size) {
	DPRINTF(DEBUG_CTR, "is_fully_received: last_packet_read != list_size, no need to check\n");
	return 0;
    }
    DPRINTF(DEBUG_CTR, "is_fully_received: last_packet_read== list_size, check\n");

    assert(wnd->last_packet_read == list_size);
    
    // check hash
    
    // gather data
    memset(buf, 0, CHUNK_SIZE);
    p = buf;
    ite = get_iterator(wnd->packet_list);
    int length = 0;
    for (ind = 0; ind < list_size; ind++) {
	assert(ite != NULL);

	info = next(&ite);

	assert(info != NULL);
	assert(info->type == DATA);
	assert(info->data_chunk != NULL);

	data_size = info->packet_len - info->header_len;
	memcpy(p, info->data_chunk, data_size);
	p += data_size;
	length += data_size;

    }
    assert(length == CHUNK_SIZE);

    // compute hash
    uint8_t *hash = malloc((SHA1_HASH_SIZE)*sizeof(uint8_t));
    shahash(buf, CHUNK_SIZE, hash);

    char ascii[SHA1_HASH_SIZE*2+1]; // the ascii string.
    hex2ascii(hash, SHA1_HASH_SIZE, ascii);
    printf("hash:%s\n",ascii);

    return 0;
}



int general_list_cat(struct list_t *info_list) {

    assert(info_list != NULL);// could be empty

    struct list_item_t *ite = NULL;
    struct packet_info_t *info = NULL;

    ite = get_iterator(info_list);
    while (has_next(ite)) {
	info = next(&ite);

	general_enlist(info);
    }

    return 0;
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

/* enlist a data packet list */
int data_wnd_list_cat(struct list_t *info_list) {
    assert(info_list != NULL);
    assert(info_list->length > 0);

    struct list_item_t *ite = NULL;
    struct packet_info_t *info = NULL;
    
    ite = get_iterator(info_list);
    while (has_next(ite)) {
	info = next(&ite);

	data_wnd_list_en(info);
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


/* Recv packet, replace peer_list with the sending peer, return packet_info */
struct packet_info_t *general_recv(int sock, bt_config_t *config) {
    
    assert(config != NULL);

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
	printf("\ngeneral_recv: received packet\n");
	info_printer(info);
    }

    return info;
}


/* Locate the last flow_wnd in the flow_wnd_list, enlist the info, and adjust next_expec and last_recv of the flow_wnd 
 * Always return the next_expec number 
 */
int enlist_DATA_info(struct packet_info_t *info) {
    assert(info != NULL);
    assert(flow_wnd_list != NULL);
    assert(info->type == DATA);

    struct flow_wnd_t *flow_wnd = NULL;
    struct list_item_t *ite = NULL;
    int ind = 0;

    // if empty flow_wnd_list, init one flow_wnd
    if (flow_wnd_list->length == 0) {
	DPRINTF(DEBUG_CTR, "enlist_data_info: create a new data_wnd\n");
	init_flow_wnd(&flow_wnd);	
	enlist(flow_wnd_list, flow_wnd);
    }
    
    // go to last flow_wnd, since this is the list we are dealing with
    flow_wnd = (struct flow_wnd_t *)flow_wnd_list->end->data;
    
    // check if seq_num > last_packet_d

    if (info->seq_num > flow_wnd->last_packet_recv) {
	// if greater, abandon it, 
	DPRINTF(DEBUG_CTR, "enlist_DATA_info: DATA_info->seq_num:%d > last_packet_recv:%d, drop it\n", info->seq_num, flow_wnd->last_packet_recv);

    } else {

	// if smaller, accept it
	// go to position seq_num and save it
	DPRINTF(DEBUG_CTR, "enlist_DATA_info: DATA_info->seq_num:%d <= last_packet_recv:%d, save it to flow_wnd_%d:position_%d\n", info->seq_num, flow_wnd->last_packet_recv, flow_wnd_list->length-1, info->seq_num-1);
    
	ite = get_iterator(flow_wnd->packet_list);
	while (ind < (info->seq_num-1)) {
	    assert(ite != NULL);
	    ind++;
	    ite = ite->next;
	}
	assert(ite != NULL);
	ite->data = info;

	// update 
	DPRINTF(DEBUG_CTR, "enlist_DATA_info: before updating, last_packet_recv:%d, next_expec%d\n", flow_wnd->last_packet_recv, flow_wnd->next_packet_expec);

	update_flow_wnd(flow_wnd);

	DPRINTF(DEBUG_CTR, "enlist_DATA_info: after updating, last_packet_recv:%d, next_expec%d\n", flow_wnd->last_packet_recv, flow_wnd->next_packet_expec);
    
    }

    return flow_wnd->next_packet_expec;
}


/* next_expec, last_recvd needs to be updated, we do not care about last_read */
void update_flow_wnd(struct flow_wnd_t *wnd) {
    struct list_item_t *ite = NULL;
    int new_expec = 1;
    int new_recv = 0;
    struct packet_info_t *info = NULL;
    int more = 0;
    int count;
    
    // update expec, it's just the packet right before the first hole
    ite = get_iterator(wnd->packet_list);
    while (has_next(ite)) {
	info = next(&ite);
	if (info->data_chunk != NULL)
	    ++new_expec;
	else
	    break;
    }
    wnd->next_packet_expec = new_expec;    // new_expec might still be 1
    
    // update last_pck_recv
    new_recv = new_expec + INIT_WND_SIZE - 1;
    more = new_recv - wnd->last_packet_recv;

    wnd->last_packet_recv = new_recv;

    //push more # of empty info into data_wnd
    count = 0;
    while(count < more) {
	count++;
	info = NULL;
	init_packet_info(&info);
	enlist(wnd->packet_list, info);
    }

    //update last_read
    wnd->last_packet_read = wnd->next_packet_expec - 1;

}


struct list_t* do_inbound_ACK(struct packet_info_t *info){
    struct list_item_t *iterator_wnd = NULL;
    struct data_wnd_t *data_wnd = NULL;
    struct packet_info_t *data_pckt = NULL;
    struct list_t *ret_list = NULL;
    int valid = 0;
    int ackNum1, ackNum2, ackNum3;

    iterator_wnd = get_iterator(data_wnd_list);
    printf("step1\n");
    while (has_next(iterator_wnd)) {
        DPRINTF(DEBUG_CTR, "iter\n");
        data_wnd = next(&iterator_wnd);

        if (((bt_peer_t*)(info->peer_list->head->data))->id == data_wnd->connection_peer_id) {
            enlist(ACK_list, info);
            data_wnd->last_packet_avai += (info->ack_num - data_wnd->last_packet_acked);
            data_wnd->last_packet_acked = info->ack_num;
            valid += 1;
            break;
        }
    }

    if (!valid){
        DPRINTF(DEBUG_CTR, "ACK packet with peer id is not found in data_wnd_list\n");
    }
    if (DEBUG_CTR && ACK_list->length == 1){
        printf("ack_number1 = %d\n",((struct packet_info_t*) (ACK_list->head->data))->ack_num);
    }
    if (DEBUG_CTR && ACK_list->length == 2){
        printf("ack_number1 = %d\n",((struct packet_info_t*) (ACK_list->head->data))->ack_num);
        printf("ack_number2 = %d\n",((struct packet_info_t*) (ACK_list->head->next->data))->ack_num);
    }
    if (DEBUG_CTR && ACK_list->length == 3){
        printf("ack_number1 = %d\n",((struct packet_info_t*) (ACK_list->head->data))->ack_num);
        printf("ack_number2 = %d\n",((struct packet_info_t*) (ACK_list->head->next->data))->ack_num);
        printf("ack_number3 = %d\n",((struct packet_info_t*) (ACK_list->head->next->next->data))->ack_num);
    }
    if (DEBUG_CTR && ACK_list->length == 4){
        printf("ack_number1 = %d\n",((struct packet_info_t*) (ACK_list->head->data))->ack_num);
        printf("ack_number2 = %d\n",((struct packet_info_t*) (ACK_list->head->next->data))->ack_num);
        printf("ack_number3 = %d\n",((struct packet_info_t*) (ACK_list->head->next->next->data))->ack_num);
        printf("ack_number3 = %d\n",((struct packet_info_t*) (ACK_list->head->next->next->next->data))->ack_num);
    }

    if (ACK_list->length > 3){
        DPRINTF(DEBUG_CTR, "removing one ACK packet\n");
        delist(ACK_list);
    }
    if (ACK_list->length == 3 && valid){
        ackNum1 = ((struct packet_info_t*) (ACK_list->head->data))->ack_num;
        ackNum2 = ((struct packet_info_t*) (ACK_list->head->next->data))->ack_num;
        ackNum3 = ((struct packet_info_t*) (ACK_list->head->next->next->data))->ack_num;
        if (ackNum1 == ackNum2 && ackNum2 == ackNum3){
            DPRINTF(DEBUG_CTR, "three same ACK packets\n");
            init_list(&ret_list);
            data_pckt = list_ind(data_wnd->packet_list, ackNum1-1);
            enlist(ret_list, data_pckt);
            return ret_list;
                
            
        }
    }
    return NULL;

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
    data_wnd_1->connection_peer_id = 1;
    init_data_wnd(&data_wnd_2);
    data_wnd_2->connection_peer_id = 2;
    init_data_wnd(&data_wnd_3);
    data_wnd_3->connection_peer_id = 3;

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
    
    bt_peer_t peer3;
    peer3.id = 3;
    peer3.addr = myaddr;
    peer3.next = NULL;
    
    struct list_t *peer_list1 = NULL;
    init_list(&peer_list1);
    enlist(peer_list1, &peer1);

    struct list_t *peer_list3 = NULL;
    init_list(&peer_list3);
    enlist(peer_list3, &peer3);

    //
    info_1 = (struct packet_info_t *)calloc(1, sizeof(struct packet_info_t));
    info_1->type = ACK;
    info_1->peer_list = peer_list3;
    info_1->ack_num = 4;

    info_2 = (struct packet_info_t *)calloc(1, sizeof(struct packet_info_t));
    info_2->type = ACK;
    info_2->peer_list = peer_list3;
    info_2->ack_num = 2;

    info_3 = (struct packet_info_t *)calloc(1, sizeof(struct packet_info_t));
    info_3->type = ACK;
    info_3->peer_list = peer_list3;
    info_3->ack_num = 2;

    info_4 = (struct packet_info_t *)calloc(1, sizeof(struct packet_info_t));
    info_4->type = ACK;
    info_4->peer_list = peer_list3;
    info_4->ack_num = 2;

    // general
    general_enlist(info_1);
    general_enlist(info_2);
    general_enlist(info_3);
    general_enlist(info_4);

    enlist(data_wnd_list, data_wnd_1);
    enlist(data_wnd_list, data_wnd_2);
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

    struct list_t *packetsList = NULL;
    init_list(&packetsList);
    enlist(packetsList, info_1);
    enlist(packetsList, info_2);
    enlist(packetsList, info_3);
    enlist(packetsList, info_4);


    data_wnd_3->packet_list = packetsList;

    struct list_t *ret_list1 = NULL;
    struct list_t *ret_list2 = NULL;
    struct list_t *ret_list3 = NULL;
    struct list_t *ret_list4 = NULL;

    ret_list1 = do_inbound_ACK(info_1);
    ret_list2 = do_inbound_ACK(info_2);
    ret_list3 = do_inbound_ACK(info_3);
    ret_list4 = do_inbound_ACK(info_4);
    if (ret_list1 == NULL){
        printf("ret_list1 == NULL\n");
    }
    if (ret_list2 == NULL){
        printf("ret_list2 == NULL\n");
    }
    if (ret_list3 == NULL){
        printf("ret_list3 == NULL\n");
    }else{
        info_printer((struct packet_info_t*)(ret_list3->head->data));
    }
    if (ret_list4 == NULL){
        printf("ret_list4 == NULL\n");
    }else{
        info_printer((struct packet_info_t*)(ret_list4->head->data));
    }

    printf("ack_num1 = %d\n", ((struct packet_info_t*) (ACK_list->head->data))->ack_num);
    printf("ack_num2 = %d\n", ((struct packet_info_t*) (ACK_list->head->next->data))->ack_num);
    printf("ack_num3 = %d\n", ((struct packet_info_t*) (ACK_list->head->next->next->data))->ack_num);
    return 0;
}


#endif
