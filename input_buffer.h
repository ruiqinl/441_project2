#include <stdlib.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

#define USERBUF_SIZE 8191
#define LINE_BUF 1024

struct line_t {
char *line_buf;
struct line_t *next;
};

struct line_queue_t{
struct line_t *head;
int count;
};

struct packet_t {
char *packet_buf;
struct packet_t *next;
};

struct packet_queue {
struct packet_t *head;
int count;
};

struct user_iobuf {
char *buf;
unsigned int cur;
    
struct line_queue_t *line_queue;
struct packet_queue_t *WHOHAS_queue;
struct packet_queue_t *IHAVE_queue;

};


struct user_iobuf *create_userbuf();
void process_user_input(int fd, struct user_iobuf *userbuf);
void enqueue_line(struct line_queue_t *line_queue, char *buf);
struct line_t *dequeue_line(struct line_queue_t *line_queue);	
/* not implemented yet	
void enqueue_packet(struct packet_queue_t *packet_queue, char *packet);
struct packet_t *dequeue_packet(struct packet_queue_t *packet_queue);		
*/
void print_queue(struct line_queue_t *line_queue);
	

