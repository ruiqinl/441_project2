#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "packet.h"
#include "debug.h"



struct packet_info_t* make_WHOHAS_packet_info(struct GET_request_t * GET_request){

    struct packet_info_t *packet_info_list, *packet_info;
    int list_size, i, packet_len;
    int slot_begin, slot_end; // indices of first and off-1 slots

    packet_info_list = NULL;
    
    if (GET_request->slot_count % MAX_HASH != 0)
	list_size = GET_request->slot_count / MAX_HASH + 1; 
    else
	list_size = GET_request->slot_count / MAX_HASH; 
	    
    // make a list_size list of packet_info
    for (i = 0; i < list_size; i++) {
	slot_begin = i * MAX_HASH;
	slot_end = (i+1) * MAX_HASH; // does not include slot_end
	if (slot_end > GET_request->slot_count)
	    slot_end = GET_request->slot_count;

	// first figure out size of the packet_info
	packet_len = (slot_end - slot_begin) * HASH_LEN + BYTE_LEN + HEADER_LEN;

	// save headers and chunk_t into struct packet_info
	packet_info = (struct packet_info_t *)calloc(1, sizeof(struct packet_info_t));
	
	packet_info->magic = (uint16)MAGIC;
	packet_info->version = (uint8)VERSION;
	packet_info->type = (uint8)WHOHAS;
	packet_info->header_len = (uint16)HEADER_LEN;
	packet_info->packet_len = (uint16)packet_len;
	packet_info->seq_num = (uint32)0;
	packet_info->ack_num = (uint32)0;
	packet_info->hash_count = (uint8)(slot_end - slot_begin);
	// include slot end
	packet_info->hash_chunk = array2chunk(GET_request, slot_begin, slot_end); 

	enlist_packet_info(&packet_info_list, packet_info);
    }

    return packet_info_list;
}

/* make block of bytes which can be transmited  */
uint8 *info2packet(struct packet_info_t *packet_info){
    
    uint8 *packet, *packet_head;

    packet_head = (uint8 *)calloc(packet_info->packet_len, sizeof(uint8));
    packet = packet_head;
 
    uint16 magic = htons(packet_info->magic);
    memcpy(packet, &magic, 2);
    packet += 2;
    
    memcpy(packet, &(packet_info->version), 1);
    packet += 1;

    memcpy(packet, &(packet_info->type), 1);
    packet += 1;

    uint16 header_len = htons(packet_info->header_len);
    memcpy(packet, &header_len, 2);
    packet += 2;

    uint16 packet_len = htons(packet_info->packet_len);
    memcpy(packet, &packet_len, 2);
    packet += 2;

    uint32 seq_num = htons(packet_info->seq_num);
    memcpy(packet, &seq_num, 4);
    packet += 4;

    uint32 ack_num = htons(packet_info->ack_num);
    memcpy(packet, &ack_num, 4);
    packet += 4;

    memcpy(packet, &(packet_info->hash_count), 1);
    packet += 1;
    memset(packet, 0, 3);
    packet += 3;

    int chunk_size = packet_info->hash_count * HASH_LEN;
    memcpy(packet, packet_info->hash_chunk, chunk_size);
    
    return packet_head;
}

void dump_packet(uint8 *packet) {
    uint8 buf;
    int i;

    for (i = 0; i < 16; i++) {
	memcpy(&buf, packet, 1);
	printf("%x\n", buf);
	packet += 1;
    }

}



/* parse chunk file, get hash_id, hash in string form and hex form  */
void parse_chunkfile(struct GET_request_t *GET_request, char *chunkfile) {
    
    FILE *fp;
    char *p, *p1;
    struct slot_t *slot_p;
    char *buf_p;
    int buf_len = 1024;
    char id_buf[8];

    if ((fp= fopen(chunkfile, "r")) == NULL) {
	DEBUG_PERROR("Error! parse_chunkfile, fopen");
	exit(-1);
    }

    buf_p = (char *)calloc(buf_len, sizeof(char));
    while ((p = fgets(buf_p, buf_len, fp)) != NULL) {

	if (feof(fp)) 
	    break;

	DPRINTF(DEBUG_PACKET, "parse_chunkfile: p:%s", p);

	if (++(GET_request->slot_count) == MAX_SLOT_COUNT) {
	    DPRINTF(DEBUG_PACKET, "parse_chunkfile: slot_chunk == MAX_SLOT_COUNT, \n");
	}

	if (strchr(p, '\n') == NULL) {
	    DPRINTF(DEBUG_PACKET, "ERROR! parse_chunfile, fgets: line length of chunfile is greater than buf_len:%d\n", buf_len);
	    exit(-1);
	}
	
	if ((p1 = strchr(p, ' ')) == NULL) {
	    DPRINTF(DEBUG_PACKET, "ERROR! parse_chunkfile, fgets: chunk file is of wrong fomat\n");
	    exit(-1);
	}


	slot_p = (struct slot_t *)calloc(1, sizeof(struct slot_t));
	GET_request->slot_array[GET_request->slot_count-1] = slot_p;
	//init_slot(slot_p);// nothing to init
	
	strncpy(id_buf, p, p1-p);
	slot_p->hash_id = atoi(id_buf);
	DPRINTF(DEBUG_PACKET, "slot_p->hash_id:%d\n", slot_p->hash_id);
	
	p = p1 + 1;
	p1 = strchr(p, '\n'); // should not be null
	if (p1 - p != HASH_STR_LEN) {
	    DPRINTF(DEBUG_PACKET, "ERROR! parse_chunkfile: p1-p != HASH_STR_LEN\n");
	    exit(-1);
	}
	strncpy(slot_p->hash_str, p, HASH_STR_LEN);
	slot_p->hash_str[HASH_STR_LEN] = '\0';
	DPRINTF(DEBUG_PACKET, "slot_p->hash-str:%s\n", slot_p->hash_str);

	str2hex(slot_p->hash_str, slot_p->hash_hex);
	if (debug & DEBUG_PACKET) {
	    printf("slot_p->hash_hex:");
	    dump_hex(slot_p->hash_hex); 
	    printf("\n");
	}
	
	free(buf_p);
	buf_p = (char *)calloc(buf_len, sizeof(char));
    }

}

void init_GET_request(struct GET_request_t *p) {
    // nothing to do
}

/* parse packet, save fields into  packet_info_t */
struct packet_info_t *packet2info(uint8 *packet) {

    uint8 *p;
    struct packet_info_t *packet_info;
    int chunk_size;

    p = packet;
    packet_info = (struct packet_info_t *)calloc(1, sizeof(struct packet_info_t));

    uint16 magic = *(uint16 *)p;
    packet_info->magic = ntohs(magic);
    p += 2;

    packet_info->version = *(uint8 *)p;
    p += 1;

    packet_info->type = *(uint8 *)p;
    p += 1;

    uint16 header_len = *(uint16 *)p;
    packet_info->header_len = ntohs(header_len);
    p += 2;
    
    uint16 packet_len =  *(uint16 *)p;
    packet_info->packet_len = ntohs(packet_len);
    p += 2;

    uint32 seq_num = *(uint32 *)p;
    packet_info->seq_num = ntohl(seq_num);
    p += 4;
    
    uint32 ack_num = *(uint32 *)p;
    packet_info->ack_num = ntohl(ack_num);
    p += 4;

    chunk_size = packet_info->packet_len - packet_info->header_len;
    
    switch (packet_info->type) {
    case WHOHAS:
    case IHAVE:
	// hash_count
	packet_info->hash_count = *(uint8 *)p;
	p += 4;
	chunk_size -= 4;
	// hash_chunk
	packet_info->hash_chunk = (uint8 *)calloc(chunk_size, sizeof(uint8));
	memcpy(packet_info->hash_chunk, p, chunk_size);	
	break;
    case GET:
	packet_info->hash_count = 0; // actually no such field 
	packet_info->hash_chunk = (uint8 *)calloc(HASH_LEN, sizeof(uint8));
	memcpy(packet_info->hash_chunk, p, HASH_LEN); // only one hash
	break;
    case DATA:
	packet_info->hash_count = 0; // actually no such field 
	packet_info->hash_chunk = (uint8 *)calloc(chunk_size, sizeof(uint8));
	memcpy(packet_info->hash_chunk, p, chunk_size); // only one hash
	break;
    case ACK:
    case DENIED:
	packet_info->hash_count = 0; // actually no such field 
	packet_info->hash_chunk = 0; // actually no such field 
	break;
    default:
	fprintf(stderr, "Error! packet2info, type does no match\n");
	break;
    }

    return packet_info;
}

void dump_packet_info_list(struct packet_info_t *packet_info) {

    int i;
    struct packet_info_t *p;

    i = 0;
    p = packet_info;
    while (p != NULL) {
	printf("packet_info_list[%d]:\n", i++);

	dump_packet_info(p);
	p = p->next;
    }
}

void dump_packet_info(struct packet_info_t *p) {
    int j;
    
    printf("magic:%d ", p->magic);
    printf("version:%d ", p->version);
    printf("type:%d ", p->type);
    printf("header_len:%d ", p->header_len);
    printf("packet_len:%d ", p->packet_len);
    printf("seq_num:%d ", p->seq_num);
    printf("ack_num:%d ", p->ack_num);
    printf("hash_count:%d(not necessarily exist)\n", p->hash_count);
    
    printf("hash_chunk:(not necessarily exist)\n");
    for (j = 0; j < p->hash_count; j++) 
	dump_hex(p->hash_chunk + HASH_LEN * j);

}

void dump_hex(uint8 *hex) {

    int i;
    unsigned int k;

    for (i = 0; i < HASH_LEN; i++) {
	k = *hex;
	printf("%2x ", k);
	hex += 1;
    }
    printf("\n");

}

/*convert 40 bytes of string into 20 bytes of hex numbers*/
void str2hex(char *str, uint8 *hex) {
    int i;
    unsigned int k;
    uint8 uint8_k;
    char buf[3];

    for (i = 0; i < HASH_LEN; i++) {
	memset(buf, 0, 3);
	strncpy(buf, str + 2*i, 2);
	sscanf(buf, "%x", &k);
	uint8_k = (uint8)k;
	memcpy(hex + i, &uint8_k, sizeof(uint8));
    }

}

/* concatenate hash_hex between begin and end into a continuous block  */
uint8 *array2chunk(struct GET_request_t *GET_request, int slot_begin, int slot_end) {
    uint8 *chunk, *p;
    int chunk_size, i, j;
    int slot_count;

    slot_count = slot_end - slot_begin + 1;
    chunk_size = slot_count * HASH_LEN;
    chunk = (uint8 *)calloc(chunk_size, sizeof(uint8));
    p = chunk;

    j = 0;
    for (i = slot_begin; i < slot_end; i++, j++) {
	memcpy(p + j*HASH_LEN, GET_request->slot_array[i]->hash_hex, HASH_LEN);
    }

    return chunk;
}

void enlist_packet_info(struct packet_info_t **packet_info_list, struct packet_info_t *packet_info) {
    struct packet_info_t *p;

    if (*packet_info_list == NULL)
	*packet_info_list = packet_info; 
    else {
	p = *packet_info_list;
	while (p->next != NULL)
	    p = p->next;
	p->next = packet_info;
    }

}

struct packet_info_t *delist_packet_info(struct packet_info_t **list) {
    struct packet_info_t *p;

    if (*list == NULL)
	return NULL;

    p = *list;
    *list = (*list)->next;

    return p;
}

void dump_info_list(struct packet_info_t *list) {
    struct packet_info_t *p;
    
    printf("dump_info_list:\n");
    if (list == NULL) {
	printf("null\n");
    } else {
	p = list;
	while (p != NULL) {
	    dump_packet_info(p);
	    p = p->next;
	}
    }
}

void enlist_id_hash(struct id_hash_t **id_hash_list, struct id_hash_t *id_hash) {
    struct id_hash_t *p;

    if (*id_hash_list == NULL)
	*id_hash_list = id_hash; 
    else {
	p = *id_hash_list;
	while (p->next != NULL)
	    p = p->next;
	p->next = id_hash;
    }
}

struct id_hash_t *delist_id_hash(struct id_hash_t **list) {
    struct id_hash_t *p;
    
    if (*list == NULL)
	return NULL;
    
    p = *list;
    *list = (*list)->next;

    return p;
}

void dump_id_hash_list(struct id_hash_t *list) {
    struct id_hash_t *p;
    
    if (list == NULL) {
	printf("null\n");
    } else {
	p = list;
	while (p != NULL) {
	    printf("%d %s\n", p->id, p->hash_string);
	    p = p->next;
	}
    }
}



#ifdef _TEST_PACKET_
int main(){

    printf("sizeof(uint8):%ld\n", sizeof(uint8));
    printf("sizeof(uint16):%ld\n", sizeof(uint16));
    printf("sizeof(uint32):%ld\n", sizeof(uint32));

    struct GET_request_t * GET_request;
    struct packet_info_t *WHOHAS_packet_info, *packet_info;
    uint8 *packet;

    GET_request = (struct GET_request_t *)calloc(1, sizeof(struct GET_request_t));
    init_GET_request(GET_request);

    parse_chunkfile(GET_request, "A.chunks");

    WHOHAS_packet_info = make_WHOHAS_packet_info(GET_request);
    dump_packet_info(WHOHAS_packet_info);
    
    packet = info2packet(WHOHAS_packet_info);
    dump_packet(packet);
    packet_info = packet2info(packet);
    dump_packet_info(packet_info);

    return 0;
}
#endif
