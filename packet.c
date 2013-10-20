#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "packet.h"
#include "debug.h"



char *make_WHOHAS(char *chunkfile, char *packet){
    struct packet_info_t *packet_p;
    struct chunk_t *chunk_p;
    int packet_len;

    // parse chunfile, save hashes in chunk file into struct chunk_t
    chunk_p = (struct chunk_t *)calloc(1, sizeof(struct chunk_t));
    init_chunk_t(chunk_p);
    parse_chunkfile(chunk_p, chunkfile);

    
    // save headers and chunk_t into struct packet_info_t
    packet_len = chunk_p->hash_count * HASH_LEN + BYTE_LEN + HEADER_LEN;
    packet = (char *)calloc(packet_len, sizeof(char));

    packet_p = (struct packet_info_t *)calloc(1, sizeof(struct packet_info_t));
    packet_p->magic = (uint16)MAGIC;
    packet_p->version = (uint8)VERSION;
    packet_p->type = (uint8)GET;
    packet_p->header_len = (uint16)HEADER_LEN;
    packet_p->packet_len = (uint16)packet_len;
    packet_p->seq_num = (uint32)0;
    packet_p->ack_num = (uint32)0;
    packet_p->chunk_p = chunk_p;

    // save members of packet_t into a block of bytes pointed by packet
    make_packet(packet, packet_p);

    return packet;

}

/* make block of bytes which can be transmited  */
int make_packet(char *packet, struct packet_info_t *packet_p){

    struct chunk_t *chunk_p;
    
    memcpy(packet, &(packet_p->magic), 2);
    packet += 2;
    
    memcpy(packet, &(packet_p->version), 1);
    packet += 1;

    memcpy(packet, &(packet_p->type), 1);
    packet += 1;

    memcpy(packet, &(packet_p->header_len), 2);
    packet += 2;

    memcpy(packet, &(packet_p->packet_len), 2);
    packet += 2;

    memcpy(packet, &(packet_p->seq_num), 4);
    packet += 4;

    memcpy(packet, &(packet_p->ack_num), 4);
    packet += 4;

    chunk_p = packet_p->chunk_p;
    uint8 count = (uint8)chunk_p->hash_count;
    memcpy(packet, &count, 1);
    packet += 1;
    memset(packet, 0, 3);
    packet += 3;

    // convert 40 bytes of string into 20 bytes of hex numbers
    int i,j;
    unsigned int k;
    uint8 uint8_k;
    char hex[3];
    for(i = 0; i < chunk_p->hash_count; i++) {
	for (j = 0; j < 20; j++) {
	    memset(hex, 0, 3);
	    strncpy(hex, chunk_p->hash_pp[i] + 2*j, 2);
	    sscanf(hex, "%x", &k);
	    uint8_k = (uint8)k;
	    memcpy(packet, &uint8_k, sizeof(uint8));
	    packet += sizeof(uint8);
	}
    }
    

    return 0;
}

/* Read chunkfile line by line, save the hash in string form, and count the number of hashes  */
struct chunk_t *parse_chunkfile(struct chunk_t *chunk_p, char *chunkfile) {
    
    FILE *fp;
    char *p;
    int buf_len = 1024;

    if ((fp= fopen(chunkfile, "r")) == NULL) {
	DEBUG_PERROR("Error! parse_chunkfile, fopen");
	return NULL;
    }

    p = (char *)calloc(buf_len, sizeof(char));

    while ((p = fgets(p, buf_len, fp)) != NULL) {
	if (feof(fp)) 
	    break;

	if (chunk_p->hash_count == MAX_CHUNK_COUNT) {
	    DPRINTF(DEBUG_PACKET, "Warning! parse_chunkfile, fgets: hash_chunk == MAX_CHUNK_COUNT\n");
	}

	if (strchr(p, '\n') == NULL) {
	    DPRINTF(DEBUG_PACKET, "Warning! parse_chunfile, fgets: line length of chunfile is greater than buf_len:%d\n", buf_len);
	}
	
	if ((p = strchr(p, ' ')) == NULL) {
	    DPRINTF(DEBUG_PACKET, "Warning! parse_chunkfile, fgets: chunk file is of wrong fomat\n");
	}
	p += 1;
	
	chunk_p->hash_pp[chunk_p->hash_count++] = p;

	p = (char *)calloc(buf_len, sizeof(char));
    }

    return chunk_p;
}


void init_chunk_t(struct chunk_t *chunk_p) {
    
    chunk_p->hash_count = 0;
    chunk_p->hash_pp = (char **)calloc(MAX_CHUNK_COUNT, sizeof(char *));
}

void dump_chunk(struct chunk_t *chunk_p) {
    int i;

    printf("chunk_p->hash_count:%d\n", chunk_p->hash_count);
    
    for (i = 0; i < chunk_p->hash_count; i++)
	printf("%s\n", chunk_p->hash_pp[i]);
	
}

void dump_packet(char *packet) {

    int hash_count;
    printf("dump_packet:\n");

    char *p = packet;
    printf("magic:%d\n", *(uint16 *)p);
    p += 2;
    
    printf("version:%d\n", *(uint8 *)p);
    p += 1;

    printf("type:%d\n", *(uint8 *)p);
    p += 1;

    printf("header_len:%d\n", *(uint16 *)p);
    p += 2;
    
    printf("packet_len:%d\n", *(uint16 *)p);
    p += 2;

    printf("seq_num:%d\n", *(uint32 *)p);
    p += 4;
    
    printf("ack_num:%d\n", *(uint32 *)p);
    p+= 4;

    hash_count = *(uint8 *)p;
    printf("hash_count:%d\n", hash_count);
    p += 4;
    
    int i, j;
    unsigned int k;
    for (i = 0; i < hash_count; i++) {
	for (j = 0; j < HASH_LEN; j++) {
	    k = *(uint8 *)p;
	    printf("%2x ", k);
	    p += 1;
	}
	printf("\n");
    }

    
}



#ifdef TESTING
int main(){

    printf("sizeof(uint8):%ld\n", sizeof(uint8));
    printf("sizeof(uint16):%ld\n", sizeof(uint16));
    printf("sizeof(uint32):%ld\n", sizeof(uint32));

    char *p, *packet;
    p = make_WHOHAS("A.chunks", packet);
    dump_packet(p);

    return 0;
}
#endif
