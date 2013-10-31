

#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "list.h"
#include "debug.h"
#include "process_udp.h"
#include "packet.h"
#include "spiffy.h"

/* Delist a packet_info, convert it into packet and send it out 
 * Return length of the reamining list
 */
int process_outbound_udp(int sock, struct list_t *list) {
    
    struct packet_info_t *packet_info = NULL;
    
    assert(list != NULL);

    if ((packet_info = delist(list)) != NULL) {
	if(debug & DEBUG_PROCESS_UDP) {
	    printf("\nprocess_outbound_udp: process packet_info:\n");
	    info_printer(packet_info);
	}

	// check type and process
	switch(packet_info->type) {
	case WHOHAS:
	    DPRINTF(DEBUG_PROCESS_UDP, "switch case WHOHAS\n");
	    send_info(sock, packet_info);
	    break;
	case IHAVE:
	    DPRINTF(DEBUG_PROCESS_UDP, "switch case IHAVE\n");
	    send_info(sock, packet_info);
	    break;
	default:
	    DPRINTF(DEBUG_PROCESS_UDP, "Error! process_outbound_udp, type does not match\n");
	    break;
	}
    }

    return list->length;
}

/*
int send_info(int sock, struct packet_info_t *packet_info) {
    char *packet;
    
    packet = info2packet(packet_info);
    
    if (spiffy_sendto(sock, packet, packet_info->packet_len, 0, (struct sockaddr *)packet_info->addr_list->sock_addr, sizeof(struct sockaddr_in)) < 0) {

	DEBUG_PERROR("Error! process_outbound_IHAVE\n");
	exit(-1);
    }

    return 0;
}
*/

int send_info(int sock, struct packet_info_t *packet_info){

    uint8 *packet = NULL;
    struct list_item_t *iterator = NULL;
    bt_peer_t *peer = NULL;

    packet = info2packet(packet_info);    
    
    iterator = get_iterator(packet_info->peer_list);
    if (iterator == NULL) {
	DPRINTF(DEBUG_PROCESS_UDP, "Warning! send_info, info->peer_list is null\n");
	return -1;
    }

    while (has_next(iterator)) {
	peer = next(&iterator);
	assert(peer != NULL);
	DPRINTF(DEBUG_PROCESS_UDP, "send_info: send to peer %d\n", peer->id);
	send_packet(peer, packet, packet_info->packet_len, sock);
	peer = peer->next;
    }

    return 0;
}


int send_packet(bt_peer_t *peer, uint8 *packet, int packet_len, int sock) {
      //while the whole data isn't sent completely
    if (spiffy_sendto(sock, packet, packet_len, 0, (struct sockaddr *)&(peer->addr), sizeof(struct sockaddr_in)) < 0){
        DEBUG_PERROR("Error! send_packet error\n");
    }

    return 0;
}

/* Process received packets based on packet type  */
int process_inbound_udp(int sock, bt_config_t *config, struct list_t *outbound_list) { 
    
    uint8 buf[MAX_PACKET_LEN+1];
    socklen_t addr_len;
    struct sockaddr_in *addr;
    struct packet_info_t *info;
    
    addr = (struct sockaddr_in *)calloc(1, sizeof(struct sockaddr_in));
    addr_len = sizeof(struct sockaddr);    
    
    spiffy_recvfrom(sock, buf, MAX_PACKET_LEN, 0, (struct sockaddr *)addr, &addr_len);
    
    info = packet2info(buf);

    if (debug & DEBUG_PROCESS_UDP) {
	printf("\nprocess_inboud_udp: received packet\n");
	info_printer(info);
    }

    // handle packets of different types seperately
    switch(info->type) {
    case WHOHAS:
	process_inbound_WHOHAS(info, config, addr, addr_len, outbound_list);
	break;
    case IHAVE:
	printf("switch case IHAVE: not implemented yet\n");
    default:
	DPRINTF(DEBUG_PROCESS_UDP, "process_inbound_udp: switch case, type is not WHOHAS or IHAVE\n");
	break;
    }

    return 0;

}

/* Parse WHOHAS pacekt and make IHAVE packet info
 * Return IHAVE_packet_info->hash_count
 */
int process_inbound_WHOHAS(struct packet_info_t *packet_info, bt_config_t *config, struct sockaddr_in *sockaddr, socklen_t addr_len, struct list_t *outbound_list){

    uint8 *target_hash = NULL;
    uint8 *chunk_p = NULL;
    struct packet_info_t *IHAVE_packet_info = NULL;
    uint8 chunk_data[MAX_PACKET_LEN];
    int count = 0;
    int i;
    //struct addr_t *addr = NULL;
    int chunk_size;
    //struct list_t *peer_list = NULL;
    bt_peer_t *peer = NULL;

    IHAVE_packet_info = (struct packet_info_t *)calloc(1, sizeof(struct packet_info_t));

    // identify the peer sending WHOHAS packet
    peer = addr2peer(config, sockaddr); 
    init_list(&(IHAVE_packet_info->peer_list));
    enlist(IHAVE_packet_info->peer_list, peer);
    assert(IHAVE_packet_info->peer_list->length == 1);
    printf("process_inbound_WHOHAS: IHAVE_PACKETInfo->peer_list:\n");
    dump_list(IHAVE_packet_info->peer_list, peer_printer, "\n");
    
    // other fields
    count = 0;
    chunk_p = chunk_data;

    // earch for matching hash
    for (i = 0; i < packet_info->hash_count; i++) {
	target_hash = packet_info->hash_chunk + i * HASH_LEN;
	DPRINTF(DEBUG_PROCESS_UDP, "process_inbound_WHOHAS: search target hash%d\n",i);

	if (search_hash(target_hash, config->id_hash_list) == 1) {
	    DPRINTF(DEBUG_PROCESS_UDP, "process_inbound_WHOAHS: target hash found\n");
	    memcpy(chunk_p, target_hash, HASH_LEN);
	    chunk_p += HASH_LEN;
	    count++;
	}
    }

    // no need to send IHAVE back
    if (count == 0) {
	DPRINTF(DEBUG_PROCESS_UDP, "process_inbound_WHOHAS: 0 matching hash found, do not send IAHVE packet back\n");
	free(IHAVE_packet_info);
	return 0;
    }
    
    // fill packet_info 
    IHAVE_packet_info->hash_count = (uint8)count;

    chunk_size = chunk_p - chunk_data;
    IHAVE_packet_info->hash_chunk = (uint8 *)calloc(chunk_size, sizeof(uint8));
    memcpy(IHAVE_packet_info->hash_chunk, chunk_data, chunk_size);

    IHAVE_packet_info->magic = (uint16)15441;
    IHAVE_packet_info->version = (uint8)1;
    IHAVE_packet_info->type = (uint8)IHAVE;
    IHAVE_packet_info->header_len = HEADER_LEN;
    IHAVE_packet_info->packet_len = HEADER_LEN + BYTE_LEN + IHAVE_packet_info->hash_count * HASH_LEN;
    
    //IHAVE_packet_info->next = NULL;

    if (debug & DEBUG_PROCESS_UDP) {
	printf("process_udp: make IAHVE_packet_info:\n");
	info_printer(IHAVE_packet_info);
    }

    // enlist IHAVE_packet_info to the outbound_list
    enlist(outbound_list, IHAVE_packet_info);

    return count;
}

int search_hash(uint8 *target_hash, struct id_hash_t *id_hash_list) {
    uint8 *has_hash;
    struct id_hash_t *id_hash;
    int match;

    match = 0;
    has_hash = (uint8 *)calloc(HASH_LEN, sizeof(uint8));
    
    id_hash = id_hash_list;
    while (id_hash != NULL) {
	str2hex(id_hash->hash_string, has_hash);

	if (debug & DEBUG_PROCESS_UDP) {
	    printf("search_hash: has_hash   ");
	    dump_hex(has_hash);
	}
	
	if (memcmp(target_hash, has_hash, HASH_LEN) == 0) {
	    match = 1;
	    break;
	}
	
	id_hash = id_hash->next;
    }

    free(has_hash);
    return match;
}



/*
#ifdef _TEST_PROCESS_UDP_
#include "input_buffer.h"

int main(){

    struct user_iobuf *userbuf;
    struct GET_request_t *GET_request;
    int sock = 2;
    
    process_user_input(0, userbuf);
    GET_request = handle_line(userbuf->line_queue);
    process_outbound_udp(sock, GET_request);
    
    return 0;
}

#endif
*/
