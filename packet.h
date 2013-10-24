#ifndef _PACKET_H_
#define _PACKET_H_

#include <sys/socket.h>
#include <netinet/in.h>
#include "bt_parse.h"

#define uint8 unsigned char
#define uint16 unsigned short
#define uint32 unsigned int

#define WHOHAS 0
#define IHAVE 1
#define GET 2
#define DATA 3
#define ACK 4
#define DENIED 5

#define VERSION 1
#define MAGIC 15441

#define MAX_PACKET_LEN 1500
#define MAX_SLOT_COUNT (MAX_PACKET_LEN/20)
#define HASH_LEN 20
#define HEADER_LEN 16
#define BYTE_LEN 4
#define HASH_STR_LEN 40

#define INIT_ARRAY_SIZE 128


struct addr_t {
    struct sockaddr *addr;
    socklen_t addr_len;    
    
    struct addr_t *next;
};

struct packet_info_t {
    uint16 magic;
    uint8 version;
    uint8 type;
    uint16 header_len;
    uint16 packet_len;
    uint32 seq_num;
    uint32 ack_num;

    // not necessarily to be used
    uint8 hash_count;
    uint8 *hash_chunk;
    
    struct addr_t* addr_list;

    struct packet_info_t *next;
};


struct slot_t {

    int hash_id;
    char hash_str[HASH_STR_LEN+1];
    uint8 hash_hex[HASH_LEN];
    
    struct peer_t *peer_list; // var length of peers, which graually grows by analyzing IHAVE_packet received, and the analysis is done in GET_req_t
    struct peer_t *selected_peer; // select peer, slot to peer
    
    struct packet_info_t *GET_packet_info;
    struct packet_info_t *DATA_packet_info;
    struct packet_info_t *ACK_packet_info;
    struct packet_info_t *DENIED_packet_info;
};

struct peer_to_slot_t {
    // how to get peerlist ?

};

struct peer_t {
    bt_peer_t bt_peer;
    int slot;
    struct peer_t *next;
};

struct id_hash_t {
    int id;
    char* hash_string;
    
    struct id_hash_t *next;
};

struct GET_request_t {

    int slot_count;
    //int slot_capacity;
    struct slot_t *slot_array[MAX_SLOT_COUNT];
    struct id_hash_t *id_hash_list;

    bt_peer_t *peer_list;
    struct peer_to_slot_t peer_to_slot; // for demultiplexing, and prevent simutaneous download from a peer

    struct packet_info_t *inbound_list; // all five kinds of packets
    struct packet_info_t *outbound_list; // all five kinds of packets
    
};


struct packet_info_t* make_WHOHAS_packet_info(struct GET_request_t *GET_request);
void parse_chunkfile(struct GET_request_t * GET_request, char *chunkfile);
char *info2packet(struct packet_info_t *packet_info);
struct packet_info_t *packet2info(char *packet);
void str2hex(char *str, uint8 *hex);
uint8 *array2chunk(struct GET_request_t *GET_request);
void dump_packet_info(struct packet_info_t *p);
void dump_hex(uint8 *hex);

void init_GET_request(struct GET_request_t *p);
void enlist_packet_info(struct packet_info_t **packet_info_list, struct packet_info_t *packet_info);
struct packet_info_t *delist_packet_info(struct packet_info_t **list);
void dump_info_list(struct packet_info_t *list);

void enlist_id_hash(struct id_hash_t **id_hash_list, struct id_hash_t *id_hash);
struct id_hash_t *delist_id_hash(struct id_hash_t **list);
void dump_id_hash_list(struct id_hash_t *list);

#endif
