#ifndef _PACKET_H_
#define _PACKET_H_

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

#define MAX_CHUNK_COUNT (1500/20)
#define HASH_LEN 20
#define HEADER_LEN 16
#define BYTE_LEN 4

struct packet_info_t {
    uint16 magic;
    uint8 version;
    uint8 type;
    uint16 header_len;
    uint16 packet_len;
    uint32 seq_num;
    uint32 ack_num;
    
    struct chunk_t *chunk_p;
};

struct chunk_t {
    int hash_count;
    char **hash_pp;
};


char *make_WHOHAS(char *chunkfile, char *packet);
struct chunk_t *parse_chunkfile(struct chunk_t * chunk_p, char *chunkfile);
void init_chunk_t(struct chunk_t *chunk_p);
void dump_chunk(struct chunk_t *chunk_p);
void dump_packet(char *packet_p);
int make_packet(char *packet, struct packet_info_t *packet_p);

#endif
