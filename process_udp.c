

#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "debug.h"
#include "process_udp.h"

int process_outbound_udp(int sock, struct GET_request_t *GET_request) {
    
    struct packet_info_t *packet_info;

    if (GET_request == NULL) {
	DPRINTF(DEBUG_PACKET, "Warning! process_outbound_udp: GET_request is NULL\n");
	return -1;
    }

    packet_info = GET_request->outbound_list;
    
    while (packet_info != NULL) {
	// check type and process
	switch(packet_info->type) {
	case WHOHAS:
	    process_outbound_WHOHAS(sock, packet_info, GET_request->peer_list);
	}
    }

    return 0;
}

int process_outbound_WHOHAS(int sock, struct packet_info_t *packet_info, bt_peer_t *peer_list){

    char *packet;
    bt_peer_t* peer;

    info2packet(packet, packet_info);
    
    peer = peer_list;
    while (peer != NULL) {
	send_packet(peer, packet, packet_info->packet_len);
	peer = peer_list->next;
    }
    
    return 0;
}

int send_packet(bt_peer_t *peer, char *packet, int packet_len) {
    printf("send_packet not implemented yet\n");
    return 0;
}


int process_inbound_udp(int sock, struct GET_request_t *GET_request) { 
    
    char buf[MAX_PACKET_LEN+1];
    socklen_t addr_len;
    struct sockaddr_in addr;
    struct packet_info_t *info;
    
    
    recvfrom(sock, buf, MAX_PACKET_LEN, 0, (struct sockaddr *)&addr, &addr_len);
    
    info = packet2info(buf);
    dump_packet_info(info);

    switch(info->type) {
    case WHOHAS:
	process_inbound_WHOHAS();

    }

    return 0;

}

int process_inbound_WHOHAS(){
    printf("process_inbound_WHOHAS not implemnted yet\n");
    return 0;
}


#ifdef TESTING
int main(){
    ;

}

#endif
