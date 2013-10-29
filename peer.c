/*
 * peer.c
 *
 * Authors: Ed Bardsley <ebardsle+441@andrew.cmu.edu>,
 *          Dave Andersen
 * Class: 15-441 (Spring 2005)
 *
 * Skeleton for 15-441 Project 2.
 *
 */

#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "debug.h"
#include "spiffy.h"
#include "bt_parse.h"
#include "input_buffer.h"
#include "packet.h"
#include "process_udp.h"

void peer_run(bt_config_t *config);

int main(int argc, char **argv) {

    printf("sizeof(uint8):%ld\n", sizeof(uint8));
    printf("sizeof(uint16):%ld\n", sizeof(uint16));
    printf("sizeof(uint32):%ld\n", sizeof(uint32));

    assert(sizeof(uint8) == 1);
    assert(sizeof(uint16) == 2);
    assert(sizeof(uint32) == 4);

    bt_config_t config;

    bt_init(&config, argc, argv); 

    DPRINTF(DEBUG_INIT, "peer.c main beginning\n");

#ifdef TESTING
    config.identity = 1; // your group number here
    strcpy(config.chunk_file, "chunkfile");
    strcpy(config.has_chunk_file, "haschunks");
#endif

    bt_parse_command_line(&config);

#ifdef DEBUG
    printf("debug:%x\n", debug);
    if (debug & DEBUG_INIT) {
	bt_dump_config(&config);
    }
#endif
  
    peer_run(&config);
    return 0;
}


bt_peer_t *exclude_self(bt_peer_t *all_nodes, short self_id) {
    bt_peer_t *self_p;
    bt_peer_t *prev;
    int found;
    
    found = 0;
    prev = NULL;
    self_p = all_nodes;
    while (self_p != NULL) {
	if (self_p->id == self_id) {
	    if (prev != NULL)
		prev->next = self_p->next;
	    else
		all_nodes = all_nodes->next;
	    found = 1;
	}
	prev = self_p;
	self_p = self_p->next;
    }
    
    if (!found) 
	DPRINTF(DEBUG_PEER, "Warning! exlucde_self, id:%d, not in the peer list\n", self_id);
    
    return all_nodes;
}

// parse has_chunk_file, get a list of id and hash_string
void parse_haschunkfile(bt_config_t *config) {

    FILE *fp;
    int buf_size = 1024;
    char buf[buf_size];
    int id;
    char *hash_string;
    struct id_hash_t *id_hash;
    
    if ((fp = fopen(config->has_chunk_file, "r")) == NULL) {
	DPRINTF(DEBUG_PEER, "Error! add_info, open");
	exit(-1);
    }
    
    memset(buf, 0, buf_size);
    while (fgets(buf, buf_size, fp) != NULL) {
	if (feof(fp))
	    break;

	//DPRINTF(DEBUG_PEER, "add_info, fgets:%s", buf);
	
	hash_string = (char *)calloc(HASH_STR_LEN+1, sizeof(char));
	sscanf(buf, "%d %s", &id, hash_string);
	
	id_hash = (struct id_hash_t *)calloc(1, sizeof(struct id_hash_t));
	id_hash->id = id;
	id_hash->hash_string = hash_string;

	enlist_id_hash(&(config->id_hash_list), id_hash);

	memset(buf, 0, buf_size);
    }
    if (debug & DEBUG_PEER) {
	printf("\nparse_hashchunkfile: config->id_hash_list:\n");
	dump_id_hash_list(config->id_hash_list);
	printf("\n");
    }
    

}



struct GET_request_t *handle_GET_request(char *chunkfile, char *outputfile, struct GET_request_t *GET_request) {

    //struct GET_request_t *GET_request;
    struct packet_info_t *WHOHAS_info_list;

    DPRINTF(DEBUG_PEER, "handle_GET_request:\n");
    DPRINTF(DEBUG_PEER, "chunfile:%s, outputfile:%s\n\n", chunkfile, outputfile);

    GET_request = (struct GET_request_t *)calloc(1, sizeof(struct GET_request_t));
    init_GET_request(GET_request);

    parse_chunkfile(GET_request, chunkfile);

    // a list of WHOHAS pakcet_info
    WHOHAS_info_list = make_WHOHAS_packet_info(GET_request);

    if (debug & DEBUG_PEER) {
	printf("WHOHAS_info_list:\n");
	dump_packet_info_list(WHOHAS_info_list);
    }

    // push to outbound_list for further processing
    enlist_packet_info(&(GET_request->outbound_list), WHOHAS_info_list);
    
    return GET_request;
}

struct GET_request_t *handle_line(struct line_queue_t *line_queue, struct GET_request_t*GET_request) {

    //struct GET_request_t *GET_request;
    char chunkf[128], outf[128];
    struct line_t *line_p;

    if (line_queue->count == 0) {
	DPRINTF(DEBUG_PACKET, "handle_line: line_queue->count is 0\n");
	return NULL;
    }

    // assume count == 1
    if (line_queue->count > 1) {
	DPRINTF(DEBUG_PACKET, "Warning! handle_line: line_queue->count is %d, >1, only first GET request will be handled\n", line_queue->count);
    }

    line_p= dequeue_line(line_queue);
    DPRINTF(DEBUG_PROCESS_GET, "handle_line:\n%s\n", line_p->line_buf);

    if (strlen(line_p->line_buf) != 0) {
	if (sscanf(line_p->line_buf, "GET %120s %120s", chunkf, outf)) {
	    if (strlen(outf) > 0) {
		
		GET_request = handle_GET_request(chunkf, outf, GET_request);
	    
	    }
	}	
	free(line_p);

	bzero(chunkf, sizeof(chunkf));
	bzero(outf, sizeof(outf));
	return GET_request;
    }

    return NULL;
}


void peer_run(bt_config_t *config) {
    int sock;
    int max_fd;
    int nfds;
    struct sockaddr_in myaddr;
    fd_set master_readfds, master_writefds;
    fd_set readfds, writefds;
    struct user_iobuf *userbuf;
    struct GET_request_t *GET_request;

    GET_request = NULL;

    if ((sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP)) == -1) {
	perror("peer_run could not create socket");
	exit(-1);
    }
  
    bzero(&myaddr, sizeof(myaddr));
    myaddr.sin_family = AF_INET;
    myaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    myaddr.sin_port = htons(config->myport);
    
    if (bind(sock, (struct sockaddr *) &myaddr, sizeof(myaddr)) == -1) {
	perror("peer_run could not bind socket");
	exit(-1);
    }
  
    //spiffy_init(config->identity, (struct sockaddr *)&myaddr, sizeof(myaddr));

    FD_ZERO(&master_readfds);
    FD_ZERO(&master_writefds);

    FD_SET(STDIN_FILENO, &master_readfds);
    FD_SET(sock, &master_readfds);

    max_fd = sock;
    if (STDIN_FILENO > sock)
	max_fd = STDIN_FILENO;

    // all sorts of preparation
    parse_haschunkfile(config);

    //GET_request = (struct GET_request_t *)calloc(1, sizeof(struct GET_request_t));
    //init_GET_request(GET_request); // actually do nothing
    

    userbuf = create_userbuf();

    while (1) {

	readfds = master_readfds;
	writefds = master_writefds;
    
	nfds = select(max_fd+1, &readfds, &writefds, NULL, NULL);
    
	if (nfds > 0) {

	    // read user input, "GET chunkfile newfile"
	    if (FD_ISSET(STDIN_FILENO, &readfds)) {
		// read user input, there might be several GETs, and there might several inputs
		printf("stdin, read\n");
		process_user_input(STDIN_FILENO, userbuf); 

		// handle the GET requests already read, set sock in master_writefds 
		// there is only one GET request
		if ((GET_request = handle_line(userbuf->line_queue, GET_request)) != NULL) {
		    GET_request->peer_list = exclude_self(config->peers, config->identity);
		    FD_SET(sock, &master_writefds); 

		} else {
		    DPRINTF(DEBUG_PACKET, "peer: nothing to handle\n");
		}
	    }

	    // all five kinds of packets
	    if (FD_ISSET(sock, &readfds)) { 
		printf("sock, read\n");
		printf("GET_req:%p\n", GET_request);

		if (GET_request == NULL) {
		    GET_request = (struct GET_request_t *)calloc(1, sizeof(struct GET_request_t));
		    init_GET_request(GET_request);
		}

		process_inbound_udp(sock, config, &(GET_request->outbound_list)); // packet2info, check packet type and take different action
		FD_SET(sock, &master_writefds);
	    }

	    // all five kinds of packet
	    // in cp1, only consider WHOHAS
	    if (FD_ISSET(sock, &writefds)) {
		// if it's WHOHAS packet, just send to all peers
		printf("sock, write\n");
		process_outbound_udp(sock, GET_request);
		FD_CLR(sock, &master_writefds);
	    }


	    
	}
    }
}
