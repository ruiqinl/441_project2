

#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include "debug.h"
#include "process_udp.h"
#include "packet.h"

int process_outbound_udp(int sock, struct GET_request_t *GET_request) {
    
    struct packet_info_t *packet_info;
    

    if (GET_request == NULL) {
	DPRINTF(DEBUG_PACKET, "\nprocess_outbound_udp: GET_request is NULL\n");
	return -1;
    }
    
    while ((packet_info = delist_packet_info(&(GET_request->outbound_list))) != NULL) {
	if(debug & DEBUG_PROCESS_UDP) {
	    printf("\nprocess_outbound_udp: process packet_info:\n");
	    dump_packet_info(packet_info);
	}

	// check type and process
	switch(packet_info->type) {
	case WHOHAS:
	    DPRINTF(DEBUG_PROCESS_UDP, "switch case WHOHAS\n");
	    process_outbound_WHOHAS(sock, packet_info, GET_request->peer_list);
	    break;
	case IHAVE:
	    DPRINTF(DEBUG_PROCESS_UDP, "switch case IHAVE\n");
	    process_outbound_IHAVE(sock, packet_info);
	    break;
	default:
	    DPRINTF(DEBUG_PROCESS_UDP, "Error! process_outbound_udp, type does not match\n");
	    break;
	}
    }

    return 0;
}

int process_outbound_IHAVE(int sock, struct packet_info_t *packet_info) {
    char *packet;
    
    packet = info2packet(packet_info);
    
    if (sendto(sock, packet, packet_info->packet_len, 0, (struct sockaddr *)packet_info->addr_list->sock_addr, sizeof(struct sockaddr_in)) < 0) {

	DEBUG_PERROR("Error! process_outbound_IHAVE\n");
	exit(-1);
    }

    return 0;
}

int process_outbound_WHOHAS(int sock, struct packet_info_t *packet_info, bt_peer_t *peer_list){

    char *packet;
    bt_peer_t* peer;

    packet = info2packet(packet_info);    
    
    peer = peer_list;
    while (peer != NULL) {
	DPRINTF(DEBUG_PROCESS_UDP, "process_outbound_WHOHAS:send to peer %d\n", peer->id);
	send_packet(peer, packet, packet_info->packet_len, sock);
	peer = peer->next;
    }

    return 0;
}


int send_packet(bt_peer_t *peer, char *packet, int packet_len, int sock) {
      //while the whole data isn't sent completely
    if (sendto(sock, packet, packet_len, 0, (struct sockaddr *)&(peer->addr), sizeof(struct sockaddr_in)) < 0){
        DEBUG_PERROR("Error! send_packet error\n");
    }

    return 0;
}

int process_inbound_udp(int sock, bt_config_t *config, struct packet_info_t **outbound_list) { 
    
    char buf[MAX_PACKET_LEN+1];
    socklen_t addr_len;
    struct sockaddr_in *addr;
    struct packet_info_t *info;
    
    addr = (struct sockaddr_in *)calloc(1, sizeof(struct sockaddr_in));
    addr_len = sizeof(struct sockaddr);
    
    recvfrom(sock, buf, MAX_PACKET_LEN, 0, (struct sockaddr *)addr, &addr_len);
    
    info = packet2info(buf);

    if (debug & DEBUG_PROCESS_UDP) {
	printf("\nprocess_inboud_udp: received packet\n");
	dump_packet_info(info);
    }

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

// parse WHOHAS packet and make IHAVE packet
int process_inbound_WHOHAS(struct packet_info_t *packet_info, bt_config_t *config, struct sockaddr_in *sockaddr, socklen_t addr_len, struct packet_info_t **outbound_list){

    uint8 *target_hash, *chunk_p;
    struct packet_info_t *IHAVE_packet_info;
    uint8 chunk_data[MAX_PACKET_LEN];
    int count, i;
    struct addr_t *addr;
    int chunk_size;

    IHAVE_packet_info = (struct packet_info_t *)calloc(1, sizeof(struct packet_info_t));

    // save reply address into IHAVE packet
    addr = (struct addr_t *)calloc(1, sizeof(struct addr_t));
    addr->sock_addr = sockaddr;
    addr->addr_len = addr_len;

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
    
    IHAVE_packet_info->addr_list = addr;

    IHAVE_packet_info->next = NULL;

    if (debug & DEBUG_PROCESS_UDP) {
	printf("process_udp: make IAHVE_packet_info:\n");
	dump_packet_info(IHAVE_packet_info);
    }

    // enlist
    //printf("*outbound_list:%p\n", *outbound_list);
    enlist_packet_info(outbound_list, IHAVE_packet_info);

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
