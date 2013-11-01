

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
 * Return -1 on type error, 0 on sending no packets, 1 on sending a packet
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
	    break;
	case IHAVE:
	    DPRINTF(DEBUG_PROCESS_UDP, "switch case IHAVE\n");
	    break;
	case ACK:
	    DPRINTF(DEBUG_PROCESS_UDP, "switch case ACK\n");
	    break;
	case GET:
	    DPRINTF(DEBUG_PROCESS_UDP, "switch case ACK\n");
	    break;
	case DATA:
	    DPRINTF(DEBUG_PROCESS_UDP, "switch case DATA\n");
	default:
	    DPRINTF(DEBUG_PROCESS_UDP, "Error! process_outbound_udp, type does not match\n");
	    return -1; // invalid type
	    break;
	}

	return send_info(sock, packet_info);
    }

    return 0;
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

/* Return 1 on sending packet to at least one peer, 0 on failing packet to any peer, -1 on peer_list is empty  */
int send_info(int sock, struct packet_info_t *packet_info){

    uint8 *packet = NULL;
    struct list_item_t *iterator = NULL;
    bt_peer_t *peer = NULL;
    int count;

    packet = info2packet(packet_info);    
    
    // send to the peers in the peer_list of the packet_info
    iterator = get_iterator(packet_info->peer_list);
    if (iterator == NULL) {
	DPRINTF(DEBUG_PROCESS_UDP, "Warning! send_info, info->peer_list is null\n");
	return -1;
    }

    count = 0;
    while (has_next(iterator)) {
	peer = next(&iterator);
	assert(peer != NULL);
	DPRINTF(DEBUG_PROCESS_UDP, "send_info: send to peer %d\n", peer->id);
	send_packet(peer, packet, packet_info->packet_len, sock);
	peer = peer->next;
	count++;
    }
    
    if (count == 0)
	return 0;
    return 1;
}


/* Return 1 on sending, -1 on error  */
int send_packet(bt_peer_t *peer, uint8 *packet, int packet_len, int sock) {
      //while the whole data isn't sent completely
    if (spiffy_sendto(sock, packet, packet_len, 0, (struct sockaddr *)&(peer->addr), sizeof(struct sockaddr_in)) < 0){
        DEBUG_PERROR("Error! send_packet error\n");
	return -1;
    }

    return 1;
}

/* Convert packet into packet_info, and save sender's addr 
 * Return pointer to packet_info 
 */
struct packet_info_t *process_inbound_udp(int sock, bt_config_t *config) {
    
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

/* Parse WHOHAS pacekt and make IHAVE packet info
 * Return pointer to IHAVE pacekt info on success, return NULL if IAHVE packet info is empty
 */
struct packet_info_t *process_inbound_WHOHAS(struct packet_info_t *packet_info, bt_config_t *config) {

    uint8 *target_hash = NULL;
    uint8 *chunk_p = NULL;
    struct packet_info_t *IHAVE_packet_info = NULL;
    uint8 chunk_data[MAX_PACKET_LEN];
    int count = 0;
    int i;
    int chunk_size;

    IHAVE_packet_info = (struct packet_info_t *)calloc(1, sizeof(struct packet_info_t));

    // earch for matching hash
    count = 0;
    chunk_p = chunk_data;

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
	return NULL;
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

    IHAVE_packet_info->peer_list = packet_info->peer_list;
    

    if (debug & DEBUG_PROCESS_UDP) {
	printf("process_udp: make IAHVE_packet_info:\n");
	info_printer(IHAVE_packet_info);
    }

    // enlist IHAVE_packet_info to the outbound_list
    //outbound_list_en(IHAVE_packet_info);

    return IHAVE_packet_info;
}

/* Save inbound DATA packet, and generate ACK packet  */
struct packet_info_t *process_inbound_DATA(struct GET_request_t *GET_requeset, struct packet_info_t *info) {
    
    

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
