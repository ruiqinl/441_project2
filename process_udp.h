#ifndef _PROCESS_UDP_H_
#define _PROCESS_UDP_H_

#include "packet.h"

int process_outbound_udp(int sock, struct list_t *outbound_list);
//int process_outbound_WHOHAS(int sock, struct packet_info_t *packet_info, bt_peer_t *peer_list);
int send_info(int sock, struct packet_info_t *packet_info);
int send_packet(bt_peer_t *peer, uint8 *packet, int packet_len, int sock);
struct list_t *process_inbound_udp(struct packet_info_t *info, int sock, bt_config_t *config, struct GET_request_t *GET_request);
struct list_t *process_inbound_WHOHAS(struct packet_info_t *packet_info, bt_config_t *config);
int search_hash(uint8 *target_hash, struct list_t *id_hash_list);
struct list_t *process_inbound_IHAVE(struct packet_info_t *info, struct GET_request_t *GET_request);
void compare_hash(struct list_t *slot_list, struct packet_info_t *info);

struct list_t *process_inbound_GET(struct packet_info_t *info, bt_config_t *config);

#endif
