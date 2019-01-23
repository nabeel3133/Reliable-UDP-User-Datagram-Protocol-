#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>
#include <arpa/inet.h>

//DEFINING VARIABLES
#define PACKETSIZE 500
#define MAX_PACKETS_LIMIT 500000
#define DATA 0
#define ACK 1
#define FIN 2
#define FIN_ACK 3

//PROTOTYPES
//FUNCTION FOR RELIABLE DATA TRANSFER USING UDP 
int RELIABLE_UDP(char * , char* );

int NFE = 0, LFA = -1;
//INITIALIZING ARRAY FOR RECEIVING PACKETS
uint8_t RECEIVED_PACKETS[MAX_PACKETS_LIMIT];

FILE * RECEIVED_FILE;

//PACKET HEADER
typedef struct {   
	uint64_t sent_time; 
	uint16_t seq_no; 
	uint16_t CODE;   
}Packet_Header;

//MAIN FUNCTION
int main(int argc, char** argv){
	//THREE ARGUMENTS ARE REQUIRED TO BE ENETERED BY USER IN COMMAND LINE
	if(argc != 3){
		fprintf(stderr, "usage: %s port filename_to_save\n\n", argv[0]);
		exit(1);
	}
	//CALLING FUNCTION FOR RELIABLE DATA TRANSFER
	RELIABLE_UDP(argv[1], argv[2]);
}

//FUNCTION FOR RELIABLE DATA TRANSFER USING UDP 
int RELIABLE_UDP(char * udpPort, char* destinationFile) {
	int sockfd, rv, PACKET_BYTES, i;
	struct addrinfo myinfo;
	struct addrinfo *srvr_info;
	struct addrinfo *selected_info;
	struct sockaddr sender_addr;
	char buffer[PACKETSIZE];
	socklen_t their_addr_len = sizeof(sender_addr);

	memset(&myinfo, 0, sizeof myinfo);
	//set ai_family to AF_INET for IPv4 address
    myinfo.ai_family = AF_INET; 
    myinfo.ai_socktype = SOCK_DGRAM;
    //USING OUR OWN IP ADDRESS
    myinfo.ai_flags = AI_PASSIVE; 

    if ((rv = getaddrinfo(NULL, udpPort, &myinfo, &srvr_info)) != 0) {
    	printf("error: %s\n", gai_strerror(rv));
    	return 1;
    }

    for(selected_info = srvr_info; selected_info != NULL; selected_info = selected_info->ai_next) {
    	if ((sockfd = socket(selected_info->ai_family, selected_info->ai_socktype, selected_info->ai_protocol)) == -1) {
    		perror("receiver: socket creation failed");
    		continue;
    	}

    	//BINDING THE SOCKET
    	if (bind(sockfd, selected_info->ai_addr, selected_info->ai_addrlen) == -1) {
    		close(sockfd);
    		perror("receiver: binding failed");
    		continue;
    	}
    	break;
    }

    if (selected_info == NULL) {
    	fprintf(stderr, "receiver: failed to bind socket\n");
    	return 2;
    }

    freeaddrinfo(srvr_info);

	printf("Waiting to receive file from sender...\n");

    Packet_Header Pkt_Header;
    //STORING SIZE OF PACKET HEADER INTO HEADER_SIZE VARIABLE
    int HEADER_SIZE = (int) sizeof(Pkt_Header);

	//PACKET SIZE SUBTRACT HEADER SIZE GIVES THE DATA SIZE
	int DATA_SIZE = PACKETSIZE - HEADER_SIZE;

	RECEIVED_FILE = fopen(destinationFile, "wb");

	uint16_t CODE, seq, length;

	//THIS RUNS FOR RECEIVING DATA PACKETS AND SENDING THE ACKS BACK TO SENDER
	while(1){
		PACKET_BYTES = recvfrom(sockfd, buffer, PACKETSIZE, 0, (struct sockaddr *)&sender_addr, &their_addr_len);
		//IF BYTES RECIEVED ARE LESS THAN HEADER SIZE MEANS PACKET IS INVALID  
		if (PACKET_BYTES < HEADER_SIZE)
			continue;
		//COPYING HEADER INFORMATION FROM RECEIVED BUFFER INTO PACKET HEADER
		memcpy(&Pkt_Header, buffer, HEADER_SIZE);
		//COPYING CODE OF PACKET INTO CODE
		CODE = Pkt_Header.CODE;
		
		//CHECK FOR FIN PACKET TO BREAK THE LOOP
		if (CODE == FIN){
			break;		
		}

		//CHECK IF PACKET TYPE IS NOT DATA TOO THEN JUST SKIP ITERATION
		if (CODE != DATA){
			continue;
		}

		//COPYING SEQ# OF PACKET FROM PACKET HEADER
		seq = Pkt_Header.seq_no;
		//CALCULATING LENGTH OF DATA PACKET
		length = PACKET_BYTES - HEADER_SIZE;

		//CHECK IF PACKET IS RECEIVED FIRST TIME
		if(RECEIVED_PACKETS[seq] == 0){
			printf("Packet(%d) is received!\n", seq);
			printf("*****************************\n\n");
			RECEIVED_PACKETS[seq] = 1;
			//CHECKING FOR CORRECT POSITION OF CURSOR
			if (SEEK_CUR != seq * DATA_SIZE){
				//MOVING CURSOR TO CORRECT LOCATION
				//Here the receiver is reordering the packets 
				fseek(RECEIVED_FILE, seq * DATA_SIZE, SEEK_SET);
			}
			//WRITING RECEIVED DATA INTO FILE FROM RECEIVED BUFFER (AFTER HEADER INFORMATION)
			fwrite(buffer + HEADER_SIZE, 1, length, RECEIVED_FILE);
			while(RECEIVED_PACKETS[NFE]){
				NFE++;
			}
		//ELSE IF PACKET IS RECEIVED NO NEED TO REWRITE IT
		} else{
			printf("Duplicate Packet(%d) is received!\n", seq);
			printf("*****************************\n\n");
		}
		
		//SETTING CODE OF HEADER TO ACK TO SEND AS INDICATION FOR PACKET RECEIVED
		Pkt_Header.CODE = ACK;
		//PUT HEADER INFORMATION IN START OF BUFFER
		memcpy(buffer, &Pkt_Header, HEADER_SIZE);
		//SENDING THIS ACK PACKET
		if ((PACKET_BYTES = sendto(sockfd, buffer, HEADER_SIZE, 0, (struct sockaddr *)&sender_addr, their_addr_len)) == -1){
			perror("receiver: error sending ACK packet");
			exit(2);
		}
		printf("Acknowledgement[ACK(%d)] is sent!\n", seq);
		printf("*****************************\n\n");
		//UPDATING LAST FRAME ACK VARIABLE
		if(seq > LFA){
			LFA = seq;	
		}
	}

	//SETTING HEADER'S CODE TO FIN ACK TO REPLY FIN PACKET
	Pkt_Header.CODE = FIN_ACK;
	//PUT HEADER INFORMATION IN START OF BUFFER 
	memcpy(buffer, &Pkt_Header, HEADER_SIZE);

	//WAIT FOR RECEIVING ACK FROM OTHER PART TO SEND FIN ACK AND KEEP SENDING FIN ACK
	while(Pkt_Header.CODE != ACK){
		//SENDING PACKET CONTAINING FIN ACK
		if ((PACKET_BYTES = sendto(sockfd, buffer, HEADER_SIZE, 0, (struct sockaddr *)&sender_addr, 
			their_addr_len)) == -1) 
		{
			perror("receiver: error sending FIN_ACK packet");
			exit(2);
		}
		//WAITING FOR ACK PACKET IN RESPONSE OF SENT PACKET
		PACKET_BYTES = recvfrom(sockfd, buffer, PACKETSIZE, 0, (struct sockaddr *)&sender_addr, &their_addr_len);
		if (PACKET_BYTES == HEADER_SIZE){
			//COPYING HEADER INFORMATION FROM BUFFER INTO PACKET HEADER
			memcpy(&Pkt_Header, buffer, HEADER_SIZE);
			//CHECK FOR ACK
			if (Pkt_Header.CODE == ACK){
			printf("Acknowledgement[ACK] is received!\n");
			}
		}
	}
	printf("*****************************\n\n");
	//CLOSING FILE
	fclose(RECEIVED_FILE);
	//CLOSING SOCKET
	close(sockfd);
}

