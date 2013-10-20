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

// not used
void process_get(char *chunkfile, char *outputfile) {

    char *WHOHAS_packet;
    
    DPRINTF(DEBUG_PROCESS_GET, "process_get: chunfile:%s, outputfile:%s\n", chunkfile, outputfile);

    // split large chunfile into several WHOHAS packet
    // !!!!handle this later!!!!!
    make_WHOHAS(chunkfile, WHOHAS_packet);
    
    //send_to_peers(WHOHAS_packet);
    
    
}

/* return 0 if GET_queue is empty, a positive number otherwise  */
//int handle_line(struct line_queue_t *line_queue, struct packet_queue_t *GET_queue) {
int handle_line(struct line_queue_t *line_queue) {
    char chunkf[128], outf[128];
    char *WHOHAS_packet;

    struct line_t *line_p;

    while (line_queue->count > 0) {

	line_p= dequeue_line(line_queue);
	DPRINTF(DEBUG_PROCESS_GET, "handle_line: %s\n", line_p->line_buf);
	if (sscanf(line_p->line_buf, "GET %120s %120s", chunkf, outf)) {
	    if (strlen(outf) > 0) {

		// WHOHAS_packet is allocated inside make_WHOHAS, so need to copy it in packet_enqueue
		WHOHAS_packet = make_WHOHAS(chunkf, outf);
		//packet_enqueue(GET_queue, WHOHAS_packet); // not implemented yet
	    }
	}	
	free(line_p);
    }
    

    bzero(chunkf, sizeof(chunkf));
    bzero(outf, sizeof(outf));

    //return GET_queue->count;
    return 0;
}


void peer_run(bt_config_t *config) {
    int sock;
    int max_fd;
    int nfds;
    struct sockaddr_in myaddr;
    fd_set master_readfds, master_writefds;
    fd_set readfds, writefds;
    struct user_iobuf *userbuf;

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

		// handle the GETs already read, set sock in master_writefds
		if (handle_line(userbuf->line_queue) > 0)
		    FD_SET(sock, &master_writefds);
	    }

	    // send WHOHAS or GET or ACK to peers
	    // in cp1, only consider WHOHAS
	    if (FD_ISSET(sock, &writefds)) {
		// not implemented yet
		//send_to_peer(config->peers, userbuf->WHOHAS_queue);
		
	    }

	    // read IHAVE or DATA from peers
	    if (FD_ISSET(sock, &readfds)) {
		process_inbound_udp(sock);
	    }

	    
	}
    }
}
