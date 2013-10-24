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
#include "debug.h"
#include "spiffy.h"
#include "bt_parse.h"
#include "input_buffer.h"
#include "packet.h"
#include "process_udp.h"

void peer_run(bt_config_t *config);

int main(int argc, char **argv) {
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

/*
void process_inbound_udp(int sock) {
#define BUFLEN 1500
    struct sockaddr_in from;
    socklen_t fromlen;
    char buf[BUFLEN];

    fromlen = sizeof(from);
    spiffy_recvfrom(sock, buf, BUFLEN, 0, (struct sockaddr *) &from, &fromlen);

    printf("PROCESS_INBOUND_UDP SKELETON -- replace!\n"
	   "Incoming message from %s:%d\n%s\n\n", 
	   inet_ntoa(from.sin_addr),
	   ntohs(from.sin_port),
	   buf);
}
*/

void add_info(struct GET_request_t *GET_request, bt_config_t *config) {
    // peer_list
    GET_request->peer_list = config->peers;
    
    // parse has_chunk_file
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

	DPRINTF(DEBUG_PEER, "add_info, fgets:%s\n", buf);
	
	hash_string = (char *)calloc(HASH_STR_LEN+1, sizeof(char));
	sscanf(buf, "%d %s", &id, hash_string);
	
	id_hash = (struct id_hash_t *)calloc(1, sizeof(struct id_hash_t));
	id_hash->id = id;
	id_hash->hash_string = hash_string;

	enlist_id_hash(&(GET_request->id_hash_list), id_hash);

	memset(buf, 0, buf_size);
    }
    if (debug & DEBUG_PEER) {
	printf("add_info: GET_request->id_hash_list:\n");
	dump_id_hash_list(GET_request->id_hash_list);
    }
    

}



struct GET_request_t *handle_GET_request(char *chunkfile, char *outputfile) {

    struct GET_request_t *GET_request;
    struct packet_info_t *WHOHAS_packet_info;

    DPRINTF(DEBUG_PEER, "handle_GET_request:\n");
    DPRINTF(DEBUG_PROCESS_GET, "chunfile:%s, outputfile:%s\n", chunkfile, outputfile);

    GET_request = (struct GET_request_t *)calloc(1, sizeof(struct GET_request_t));
    init_GET_request(GET_request);

    parse_chunkfile(GET_request, chunkfile);

    // ???????? several packet ?????????????
    WHOHAS_packet_info = make_WHOHAS_packet_info(GET_request);

    if (debug & DEBUG_PEER) {
	printf("handle_GET_request: WHOHAS_packet_info list:\n");
	dump_packet_info(WHOHAS_packet_info);
    }

    enlist_packet_info(&(GET_request->outbound_list), WHOHAS_packet_info);
    //dump_info_list(GET_request->outbound_list);
    
    return GET_request;
}

/* return 0 on success*/
//int handle_line(struct line_queue_t *line_queue, struct packet_queue_t *GET_queue) {
struct GET_request_t *handle_line(struct line_queue_t *line_queue) {

    struct GET_request_t *GET_request;
    char chunkf[128], outf[128];
    struct line_t *line_p;

    GET_request = NULL;

    if (line_queue->count == 0) {
	DPRINTF(DEBUG_PACKET, "handle_line: line_queue->count is 0\n");
	return NULL;
    }

    // assume count == 1
    if (line_queue->count > 1) {
	DPRINTF(DEBUG_PACKET, "Warning! handle_line: line_queue->count is %d, >1, only first GET request will be handled\n", line_queue->count);
    }

    line_p= dequeue_line(line_queue);
    DPRINTF(DEBUG_PROCESS_GET, "handle_line: %s\n", line_p->line_buf);
    if (sscanf(line_p->line_buf, "GET %120s %120s", chunkf, outf)) {
	if (strlen(outf) > 0) {
		
	    GET_request = handle_GET_request(chunkf, outf);
	    
	}
    }	
    free(line_p);

    bzero(chunkf, sizeof(chunkf));
    bzero(outf, sizeof(outf));

    return GET_request;
}


void peer_run(bt_config_t *config) {
    int sock;
    int max_fd;
    int nfds;
    struct sockaddr_in myaddr;
    fd_set master_readfds, master_writefds;
    fd_set readfds, writefds;
    struct user_iobuf *userbuf;
    
    struct GET_request_t *GET_request = NULL;
        

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


    userbuf = create_userbuf();
    while (1) {

	readfds = master_readfds;
	writefds = master_writefds;
    
	nfds = select(sock+1, &readfds, &writefds, NULL, NULL);
    
	if (nfds > 0) {

	    // read user input, "GET chunkfile newfile"
	    if (FD_ISSET(STDIN_FILENO, &readfds)) {
		// read user input, there might be several GETs, and there might several inputs
		process_user_input(STDIN_FILENO, userbuf); 

		// handle the GET requests already read, set sock in master_writefds 
		// assume there is only one GET request

		if ((GET_request = handle_line(userbuf->line_queue)) != NULL) {
		    // add some other info to GET_request
		    add_info(GET_request, config);
		    
		    FD_SET(sock, &master_writefds); 
		    /*
#ifdef _PEER_TEST_
		    FD_CLR(sock, &master_writefds);
		    printf("in test:\n");
		    process_outbound_udp(sock, GET_request);
		    //process_inbound_udp(sock, )
#endif
		    */
		    
		} else {
		    DPRINTF(DEBUG_PACKET, "peer: handle_line, return NULL\n");
		}
	    }

	    // all five kinds of packets
	    if (FD_ISSET(sock, &readfds)) { 
		process_inbound_udp(sock, GET_request); // packet2info, check packet type and take different action
	    }

	    // all five kinds of packet
	    // in cp1, only consider WHOHAS
	    if (FD_ISSET(sock, &writefds)) {
		// if it's WHOHAS packet, just send to all peers
		process_outbound_udp(sock, GET_request);
		FD_CLR(sock, &master_writefds);
		//send_to_peers(config->peers, userbuf->WHOHAS_queue);
		
	    }


	    
	}
    }
}
