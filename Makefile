# Some variables
CC 		= gcc
CFLAGS		= -g -Wall -DDEBUG
LDFLAGS		= -lm
#TESTDEFS	= -DTESTING			# comment this out to disable debugging code
OBJS		= peer.o bt_parse.o spiffy.o debug.o input_buffer.o chunk.o sha.o packet.o process_udp.o list.o
MK_CHUNK_OBJS   = make_chunks.o chunk.o sha.o

BINS            = peer make-chunks
TESTBINS        = test_debug test_input_buffer test_list

# suffix rule
.c.o:
	$(CC) $(TESTDEFS) -c debug.c $(CFLAGS) $<


# Explit build and testing targets

all: ${BINS} ${TESTBINS}

run: peer_run
	./peer_run

test: peer_test
	./peer_test

peer.o: peer.c
	$(CC) -D_PEER_TEST_ -c debug.c $(CFLAGS) $<

peer: $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $@ $(LDFLAGS)

make-chunks: $(MK_CHUNK_OBJS)
	$(CC) $(CFLAGS) $(MK_CHUNK_OBJS) -o $@ $(LDFLAGS)

clean:
	rm -f *.o $(BINS) $(TESTBINS)

bt_parse.c: bt_parse.h

# The debugging utility code

debug-text.h: debug.h
	./debugparse.pl < debug.h > debug-text.h

test_debug: debug.c debug-text.h
	${CC} debug.c ${INCLUDES} ${CFLAGS} -c -D_TEST_DEBUG_ -o $@

#test_input_buffer:  test_input_buffer.o input_buffer.o debug.o
test_input_buffer: input_buffer.c debug.c
	$(CC) $^ -D_TEST_INPUT_BUFFER_ $(CFLAGS) -o $@

test_packet: packet.c debug.c
	$(CC) $^ $(CFLAGS) -D_TEST_PACKET_ -o $@

test_list: list.c debug.c
	$(CC) $^ $(CFLAGS) -D_TEST_LIST_ -o $@

#test_process_udp: process_udp.c debug.c packet.c
#	$(CC) $^ $(CFLAGS) -D_TEST_PROCESS_UDP_ -o $@

