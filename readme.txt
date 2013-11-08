################################################################################
# README                                                                       #
#                                                                              #
# Description: This file serves as a README and documentation for project2     #
#                                                                              #
#                                                                              #
################################################################################




[TOC-1] Table of Contents
--------------------------------------------------------------------------------

        [TOC-1] Table of Contents
        [DES-2] Description of Files
        [RUN-3] How to Run




[DES-2] Description of Files
--------------------------------------------------------------------------------

Here is a listing of all files associated with Recitation 1 and what their'
purpose is:

                    Makefile            - Contains rules for make
                    readme.txt              - Current document 

		    vulnerabilities.txt     - File containing documentation of at 
						  one vulnerability I identify at each
						  stage
		    peer.c		- The main file
	  	    bt_parser.[c,h]	- Parse configuration files
		    chunk.[c,h]		- Generate chunks based on provided files
		    debug.[c,h]		- Utility debug functions
		    input_buffer.[c,h]  - Parse user input
		    make_chunks.c	- Generate chunks
		    packet.[c,h]	- Generate various kinds of packets
		    processs_udp.[c,h]  - Process udp input and output based on the packet 
					   type
		    ctr_send_recv.[c,h] - Contains all recv and send functions, and related data structure
		    list.[c,h]		- List library
		    
		    server.c
		    sha.[c,h]
		    spiffy.[c,h]
		    test_input_buffer.c



[RUN-3] How to Run
--------------------------------------------------------------------------------

Building code by running make:

                    make

Running code by inputing:
		    peer -p <peer-list-file> -c <has-chunk-file> -m <max-downlaods>
			 -i <peer-identity>  -f <master-chunk-file> -d <debug-mode>
		// in cp1, debug mode is hard coded to 0xff, i.e. all debug info. are printed




