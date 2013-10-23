#ifndef _PROCESS_UDP_H_
#define _PROCESS_UDP_H_

#include "packet.h"

int process_outbound_udp(int sock, struct GET_request_t *GET_request);
int process_outbound_WHOHAS(int sock, struct packet_info_t *packet_info, bt_peer_t *peer_list);
int send_packet(bt_peer_t *peer, char *packet, int packet_len);
int process_inbound_udp(int sock, struct GET_request_t *GET_request);
int process_inbound_WHOHAS();


#endif
