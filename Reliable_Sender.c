#include <signal.h>
#include <sys/time.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

#define PACKETSIZE 500  //Defining packet size
#define DATA 0
#define ACK 1
#define FIN 2
#define FIN_ACK 3
int TOTAL_ACKS = 0;
int TOTAL_SEGMENTS = 0;
int WINDOW_SIZE = 0; 
int FILE_SIZE = 0;

//PROTOTYPES FOR ALL THE FUNCTIONS
//TIMEOUT HANDLER
void Handle_TimeOut(int);
//SETTER FUNCTION FOR TIMEOUT
void TIMER_STARTS();
//COUNTER FOR THE TOTAL ACKS RECEIVED
int ACKS_COUNTER(int[] , int);
//Fuction for transferring data reliablly using UDP
//FUNCTION FOR TRANSFERRING THE DATA RELIABLY OVER UDP.
int RELIABLE_UDP(char* , char * , char* );

int64_t Time_Out = 1;
int L_Ack_R = -1; 
const int Window_Size = 5;  

//Packet_Header
typedef struct {   
	uint64_t sent_time;    //initializing sent time
	uint16_t seq_no;   //initializing sequence number
	uint16_t CODE;   //initializing content type
}Packet_Header;

//MAIN FUNCTION
int main(int argc, char** argv){
	//FOUR INPUTS ARE REQUIRED TO BE ENTERED BY USER IN COMMAND LINE
	if(argc != 4){
		fprintf(stderr, "Please type in terminal: %s hostname port filename_to_transfer\n\n", argv[0]);
		exit(1);
	}
	//CALLING FUNCTION FOR RELIABLE DATA TRANSFER
	RELIABLE_UDP(argv[1], argv[2], argv[3]);

	printf("\n\n*******************SUMMARY OF THE RELIABLE UDP*******************\n");
   			puts("\t\t\t      _____");   
   			puts("\t\t\t     |TABLE|");
    		puts("|----------------|----------------|-------------|------------------|");
    		puts("|   Total ACKS   | Total Segments | Window Size |     File Size    |");
    		puts("|----------------|----------------|-------------|------------------|");
        	printf("| %8d       |     %2d      |     %3d     |     %2d      |\n"
               , (TOTAL_ACKS), TOTAL_SEGMENTS, WINDOW_SIZE, FILE_SIZE);
        	puts("--------------------------------------------------------------------");
}

//handler for Time_Out
void Handle_TimeOut(int signum){
	printf("Timeout has occured!\n"); //Printing on screen for time out
}

//setting Time_Out timer
void TIMER_STARTS(){
	printf("Timer has started!\n");
	struct itimerval timeout_timer;
	
 	//expire timer after 3 seconds
 	timeout_timer.it_value.tv_sec = 3;
 	timeout_timer.it_value.tv_usec = 0;
 	
 	timeout_timer.it_interval.tv_sec = 0;
 	timeout_timer.it_interval.tv_usec = 0;
 	
 	//STARTING THE TIMER
 	setitimer (ITIMER_REAL, &timeout_timer, NULL);
}

//COUNTER FOR TOTAL ACKS THAT ARE RECEIVED
int ACKS_COUNTER(int ACKED_SEGMENTS[], int size){
	int acks_count = 0,a;
	for ( a = 0; a < size; a++){
		if(ACKED_SEGMENTS[a]==1){
			acks_count++;
		}
	}
	TOTAL_ACKS = acks_count;
	return acks_count;
}

//FUNCTION FOR TRANSFERRING DATA RELIABLY OVER UDP
int RELIABLE_UDP(char* hostname, char * udpPort, char* filename){
	//VARIABLE TO GIVE THE SIGNAL TO HANDLE THE TIMEOUT
	struct sigaction signal_action;
 	memset (&signal_action, 0, sizeof (signal_action));
 	//SETTING THE HANDLE_TIMEOUT FUNCTION AS SIGNAL ACTION HANDLER
 	signal_action.sa_handler = &Handle_TimeOut;
 	sigaction (SIGALRM, &signal_action, NULL);

	int sockfd, rv, bytes_in_packet, i;
	
	struct addrinfo myinfo;
	struct addrinfo *server_info;
	struct addrinfo *selected_info;

	struct sockaddr_in bind_addr, sendto_addr;
	struct sockaddr receiver_addr;
	socklen_t their_addr_len = sizeof(receiver_addr);
	char buffer[PACKETSIZE];

	//SETTING UP THE SENDTO ADDRESS
	uint16_t sendto_port = (uint16_t)atoi(udpPort);
	memset(&sendto_addr, 0, sizeof(sendto_addr));

	sendto_addr.sin_family = AF_INET;
	sendto_addr.sin_port = htons(sendto_port);
	inet_pton(AF_INET, hostname, &sendto_addr.sin_addr);

	memset(&myinfo, 0, sizeof myinfo);
	myinfo.ai_family = AF_INET;
	myinfo.ai_socktype = SOCK_DGRAM;

	if ((rv = getaddrinfo(hostname, udpPort, &myinfo, &server_info)) != 0) {
		printf("error: %s\n", gai_strerror(rv));
		return 1;
	}

	for(selected_info = server_info; selected_info != NULL; selected_info = selected_info->ai_next) {
		if ((sockfd = socket(selected_info->ai_family, selected_info->ai_socktype, selected_info->ai_protocol)) == -1) {
			perror("sender: socket error");
			continue;
		}
		break;
	}

	if (selected_info == NULL) {
		fprintf(stderr, "sender: socket creation failed\n");
		return 2;
	}

	//PORT NUMBER EXTRACTION
	uint16_t my_port = ((struct sockaddr_in *)selected_info->ai_addr )->sin_port; 
	
	freeaddrinfo(server_info);
	
	memset(&bind_addr, 0, sizeof(bind_addr));
	bind_addr.sin_family = AF_INET;
	//PORT NUMBER SETTING
	bind_addr.sin_port = htons(my_port);
	bind_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	//BINDING THE SOCKET
	if (bind(sockfd, (struct sockaddr *)&bind_addr, sizeof(struct sockaddr_in)) == -1) 
	{
		close(sockfd);
		perror("sender: binding failed");
		exit(1);
	}

	//INITIALIZING PACKET HEADERS
	Packet_Header sender_Packet_Header, receiver_Packet_Header;
	//STORE SENDER's PACKET HEADER SIZE IN Header_size
	int Header_size = (int) sizeof(sender_Packet_Header);
	
	FILE* out_file = fopen(filename, "rb");
	
	fseek(out_file, 0, SEEK_END);
	//FILE SIZE STORING IN file_size
    int file_size = ftell(out_file);
    fseek(out_file, 0, SEEK_SET);

    //PACKETSIZE SUBTRACT HEADERSIZE GIVES DATASIZE
	int DATA_SIZE = PACKETSIZE - Header_size;
	//TOTAL SEGMENTS IN WHICH FILE CAN DEVIDED 
	int File_Segments = file_size/DATA_SIZE;
	//SAVING REMAINING BYTES THAT ARE NOT DEVISIBLE IN SEGMENTS 
	int final_seg_size = file_size % DATA_SIZE;

	//IF REAMAINIG BYTES EXIST MAKING A NEW SEGMENT FOR THEM
	if (final_seg_size > 0)
		File_Segments++;

	FILE_SIZE = file_size;
	WINDOW_SIZE = Window_Size;

	//FOR FIRST PACKET SEQ_NO IS 0
	sender_Packet_Header.seq_no = 0;
	//SETTING CODE TO DATA FOR SENDER PACKET HEADER
	sender_Packet_Header.CODE = DATA;
	//VARIABLE FOR COUNTING RETRANSMITTED PACKETS
		int double_sent = 0;

	printf("Sending the file to the receiver...\n");
	printf("-----------------------------------\n");

	uint16_t seq, length;
	
	//ARRAYS FOR ACKS AND SENT TO SAVE SEGMENT STATE
	int *ACKED_SEGMENTS = (int *)malloc(sizeof(int)*File_Segments);
	int *SENT_SEGMENTS = (int *)malloc(sizeof(int)*File_Segments);
	
	//INITIALIZING ARRAYS WITH ZERO
	for (i = 0; i < sizeof(ACKED_SEGMENTS); i++){
		ACKED_SEGMENTS[i] = 0;
		SENT_SEGMENTS[i] = 0;		
	}

	//VARIABLE TO COUNT ACKS
	int ACK_COUNT;

	//LOOP FOR RECEIVING ALL ACKS
	while (ACK_COUNT != File_Segments){
		ACK_COUNT = 0;

		//RUN THIS LOOP FOR TOTAL WINDOW SIZE
		for (i = 0; i < Window_Size; i++){
			//SEQ # FOR NEXT PACKET TO BE SENT
			seq = L_Ack_R+i+1;
			//CHECKING IF ACK IS NOT RECEIVED 
			if (ACKED_SEGMENTS[seq] == 0 && seq < File_Segments){
				//CHECKING FOR RETRANSMISSION
				if (SENT_SEGMENTS[seq] == 1){
					double_sent++;		//INCREMENT FOR RETRANSMITTED PACKETS
				//IF PACKET IS TRANSMITTING FIRST TIME
				} else{
					//SET THAT PACKET AS 1 IN SENT SEGMENT ARRAY
					SENT_SEGMENTS[seq] = 1;
					//SETTING SEQ # FOR PACKET HEADER
					sender_Packet_Header.seq_no = seq;
					//PUTTING PACKET HEADER INTO BUFFER TO BE SENT
					memcpy(buffer, &sender_Packet_Header, Header_size);
					//SETTING CURSOR LOCATION TO PUT DATA INTO BUFFER 
					if (SEEK_CUR != seq * DATA_SIZE)
						//MOVE CURSOR TO CORRECT LOCATION
						fseek(out_file, DATA_SIZE * seq, SEEK_SET);
					//CHECK FOR LAST SEGMENT OF REMAINING BYTES
					if (seq == File_Segments-1 && final_seg_size > 0){
						//SETTING LENGTH OF PACKET TO LAST SEGMENT SIZE
						length = final_seg_size;	
					}
					//SETTING PACKKET LENGTH TO COMPLETE DATA LENGTH
					else{
						length = DATA_SIZE;
					}
					//PUTTING DATA FROM FILE INTO BUFFER AFTER HEADER
					fread(buffer + Header_size, 1, length, out_file);
					//SENDING PACKET TO RECEIVER
					if (bytes_in_packet = sendto(sockfd, buffer, length + Header_size, 0,
				 		(struct sockaddr*)&sendto_addr, sizeof(sendto_addr)) == -1) 
					{	
						perror("sender: error sending data packet");
						exit(1);
					}

					printf("Packet(%d) is sent!\n", seq);	
				}
			}
		} 
		printf("******************************\n");
		//START TIMER FOR TIME OUT
		TIMER_STARTS();

		//RUNS FOR RECEIVING ACKS
		for (i = 0; i < Window_Size; i++){
			//CHECK TO SKIP ITERATION IF ACKS ARE EQUAL TO TOTAL SEGMENTS
			//IF NOT THEN STOP AND WAIT FOR RECEIVING ACKS 
			if(ACKS_COUNTER(ACKED_SEGMENTS,File_Segments) != File_Segments){
				bytes_in_packet = recvfrom(sockfd, buffer, PACKETSIZE, 0, &receiver_addr, &their_addr_len);
				if(bytes_in_packet == Header_size){
					//COPYING HEADER OF RECEIVED BUFFER INTO RECEIVER's HEADER
					memcpy(&receiver_Packet_Header, buffer, Header_size);
					//CHECKING IF RECEIVED PACKET HEADER HAS ACK IN ITS CODE.
					if (receiver_Packet_Header.CODE == ACK){
						printf("Acknowledgment ACK(%d) is received!\n", receiver_Packet_Header.seq_no);
						//SETTING THIS PACKET AS ACKED IN ACKED SEGMENTS ARRAY
						ACKED_SEGMENTS[receiver_Packet_Header.seq_no] = 1;
					}
				}
				else {
					break;	
				}
			}
		}
		printf("******************************\n");

		//ARRAY FOR SAVING MISSED ACKS
		int NOT_RECEIVED_ACKS[Window_Size];
		//INITIALIZING ALL MISSED ACK WITH ZERO
		memset(&NOT_RECEIVED_ACKS, 0, sizeof(NOT_RECEIVED_ACKS));
		//VARIABLE TO COUNT NOT RECEIVED ACKS
		int count = 0;
		for (i=L_Ack_R+1; i < L_Ack_R+Window_Size; i++){
			//BREAKING LOOP IF ALL SEGMENTS ARE TRANSFERRED 
			if(i >= File_Segments){
				break;
			} else{
				//CHECKING IF CURRENT SEGMENT IS ACKED
				if (!ACKED_SEGMENTS[i]){
					//STORING SEQ# OF NOT RECEIVED ACKS
					NOT_RECEIVED_ACKS[count] = i;			
					count++;
					printf("Packet(%d) is lost!\n", i); 
					
					//SETTING THAT SEQUENCE NUMBER IN SENDER's PACKET HEADER
					sender_Packet_Header.seq_no = i;
					//PUTTING THAT PACKET's HEADER INTO BUFFER TO SEND
					memcpy(buffer, &sender_Packet_Header, Header_size);
					//SETTING CURSOR TO LOCATION IN BFFER WHERE DATA NEEDS TO PUT 
					if (SEEK_CUR != i * DATA_SIZE)
						//MOVING CURSOR TO THAT LOCATION
						fseek(out_file, DATA_SIZE * i, SEEK_SET);
					//CHECK FOR LAST SEHMENT OF REMAINING BYTES
					if (i == File_Segments-1 && final_seg_size > 0){
						//SETTING ITS SIZE TO PACKET LENGTH
						length = final_seg_size;	
					}
					//ELSE SET THE LENGTH TO COMPLETE DATA LENGTH
					else{
						length = DATA_SIZE;
					}
					//PUTTING DATA FROM FILE INTO BUFFER AFTER THE HEADER
					fread(buffer + Header_size, 1, length, out_file);
					//SENDING PACKET
					if (bytes_in_packet = sendto(sockfd, buffer, length + Header_size, 0,
				 		(struct sockaddr*)&sendto_addr, sizeof(sendto_addr)) == -1) 
					{	
						perror("sender: sendto");
						exit(1);
					}
					//SETTING THAT PACKET AS SENT IN SENT SEGMENT ARRAY
					SENT_SEGMENTS[i] = 1;
					printf("Packet (%d) is retransmitted!\n", i);	
				}
			}
		}
		printf("******************************\n");

		//CHECKING FOR NOT RECEIVED ACKS
		if (count>0) {
			//STARTING TIMER AGAIN
			TIMER_STARTS();
		}
		
		//RUNNING LOOP FOR ALL NOT RECEIVED ACKS
		for (i=0; NOT_RECEIVED_ACKS[i]!=0; i++){
			//WAITING FOR ACKS
			bytes_in_packet = recvfrom(sockfd, buffer, PACKETSIZE, 0, &receiver_addr, &their_addr_len);
			if(bytes_in_packet == Header_size){
				//COPYING HEADER INFO FROM RECEIVED BUFFER INTO RECEIVER'S PACKET HEADER
				memcpy(&receiver_Packet_Header, buffer, Header_size);
				//CHECKING IF ACK IS RECEIVED
				if (receiver_Packet_Header.CODE == ACK){
					printf("Acknowledgment ACK(%d) is received!\n", receiver_Packet_Header.seq_no);
					ACKED_SEGMENTS[receiver_Packet_Header.seq_no] = 1;
				}
			}
			//CHECKING FOR NOT RECEIVED ACKS
			else if (NOT_RECEIVED_ACKS[i] != 0){
				//SETTING SEQ# OF MISSSED ACKS
				sender_Packet_Header.seq_no = NOT_RECEIVED_ACKS[i];
				//PUTTING SENDER'S PACKET HEADER INTO BUFFER
				memcpy(buffer, &sender_Packet_Header, Header_size);
				//SETTING THE POSITION OF THE CURSOR
				if (SEEK_CUR != NOT_RECEIVED_ACKS[i] * DATA_SIZE)
					//MOVING CURSOR TO THAT LOCATION
					fseek(out_file, DATA_SIZE * NOT_RECEIVED_ACKS[i], SEEK_SET);
				//CHECK FOR LAST SEGMENT OF REMAINING BYTES
				if (NOT_RECEIVED_ACKS[i] == File_Segments-1 && final_seg_size > 0){
					//SETTING THE SIZE OF LAST PACKET
					length = final_seg_size;	
				}
				//ELSE SET IT TO COMPLETE DATA LENGTH
				else{
					length = DATA_SIZE;
				}
				//PUTTING DATA INTO BUFFER FROM FILE AFTER THE HEADER INFORMATION
				fread(buffer + Header_size, 1, length, out_file);
				//SENDING PACKET
				if (bytes_in_packet = sendto(sockfd, buffer, length + Header_size, 0,
			 		(struct sockaddr*)&sendto_addr, sizeof(sendto_addr)) == -1) 
				{	
					perror("sender: error sending the packet");
					exit(1);
				}
				printf("Packet(%d) is retransmitted!\n", NOT_RECEIVED_ACKS[i]);	
				//DECREASING COUNTER TO RECEIVE ACK OF THIS PACKET AGAIN
				i--;
				//RESETTING TIMER
				TIMER_STARTS();
				//CONTINUE THE LOOP
				continue;	
			} else{
				break;
			}
		
		}
		printf("******************************\n");
		TOTAL_SEGMENTS = File_Segments;
		//CEHCK FOR ALL SEGMENTS RECEIVED
		ACK_COUNT = ACKS_COUNTER(ACKED_SEGMENTS, File_Segments);
		L_Ack_R = ACK_COUNT-1;
	}

	//STARTING CONNECTION TEARDOWN
	// printf("----------------------\n");
	// printf("Closing connection...  \n");
	// printf("----------------------\n");
	//LOOP TO RECEIVE FIN ACK
	while(receiver_Packet_Header.CODE != FIN_ACK){
		//SETTING PACKET HEADER's CODE TO FIN
		sender_Packet_Header.CODE = FIN;
		//PUTTING SENDER PACKET's HEADER INFO INTO BUFFER 
		memcpy(buffer, &sender_Packet_Header, Header_size);
		//SEND THE FIN PACKET
		if (bytes_in_packet = sendto(sockfd, buffer, Header_size, 0,
			 (struct sockaddr*)&sendto_addr, sizeof(sendto_addr)) == -1) 
		{	
			perror("sender: error sending FIN packet");
			exit(1);
		}
		//printf("[SND] FIN Sent. \n");
		//WAIT FOR RECEIVING PACKET OF FIN ACK
		bytes_in_packet = recvfrom(sockfd, buffer, PACKETSIZE, 0, &receiver_addr, &their_addr_len);
		if(bytes_in_packet == Header_size){
			memcpy(&receiver_Packet_Header, buffer, Header_size);
			//CHECKING IF PACKET CONTAINS FIN ACK
			if (receiver_Packet_Header.CODE == FIN_ACK){
				//printf("[RCV] FIN_ACK Received. \n");
			}
		}
	}

	//SETTING CODE OF PACKET HEADER TO ACK
	sender_Packet_Header.CODE = ACK;
	//PUTTING SENDER'S PACKET HEADER TO BUFFER
	memcpy(buffer, &sender_Packet_Header, Header_size);
	//SENDING LAST ACK PACKET
	if (bytes_in_packet = sendto(sockfd, buffer, Header_size, 0,
		 (struct sockaddr*)&sendto_addr, sizeof(sendto_addr)) == -1) 
	{	
		perror("sender: error sending last ACK packet");
		exit(1);
	}

	printf("Acknowledgment(ACK) Sent! \n");
	printf("******************************\n");
	//CLOSING THE FILE
	fclose(out_file);
	//CLOSING SOCKET
	close(sockfd);
}

 
