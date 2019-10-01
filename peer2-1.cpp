#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <iostream>
#include <string.h>
#include <bits/stdc++.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <pthread.h>

using namespace std;

#define BUFF_SIZE 2048
#define PORT2 2002
#define PORT1 2001

void *serverThreadFunc(void *ptr);
void *clientThreadFunc(void *ptr);
void serveClientRequest(struct sockaddr_in new_sock_addr, int new_sock_fd);

int main(){

	pthread_t serverThread, clientThread;
	char *msgServer="In server thread";
	char *msgClient="In client thread";

	int serv_thread_status=pthread_create(&serverThread, NULL, serverThreadFunc, (void*)msgServer);
	// thread clientThread(clientThreadFunc);
	int client_thread_status=pthread_create(&clientThread, NULL, clientThreadFunc, (void*)msgClient);
	

	pthread_join(serverThread, NULL);
	pthread_join(clientThread, NULL);

	cout<<"Server thread is back"<<endl;
	cout<<"Client thread is back"<<endl;
	exit(0);
}

void *clientThreadFunc(void *ptr){
	cout<<(char*)ptr<<endl;

	int sockfd=socket(AF_INET, SOCK_STREAM, 0);
	if(sockfd<0){
		perror("Socket creation failed : client");
		exit(EXIT_FAILURE);
	}
	cout<<"Socket created : client"<<endl;

	struct sockaddr_in serv_addr;
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons( PORT1 );
	serv_addr.sin_addr.s_addr=inet_addr("127.0.0.1");

	cout<<"Enter no."<<endl;
	int num;
	cin>>num;

	//In (struct sockaddr*)&serv_addr, sockaddr_in doesn't work!
	if(connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr))<0){
		perror("Connect failed : Client");
		exit(EXIT_FAILURE);
	}
	cout<<"Connect successful : Client "<<endl;

	/*//Msg receiving code:
	char msg[BUFF_SIZE];
	// int msg;
	// recv(sockfd, &msg_len, sizeof(msg_len), 0);
	if(recv(sockfd , msg , sizeof(msg), 0) < 0){
		perror("Error in receive : Client");
		exit(EXIT_FAILURE);
	}
	cout<<"Msg size received at client: "<< strlen(msg)<<endl;
	cout<<"Msg received at client: "<< msg<<endl;
	*/

	//File receiving code:
	FILE *fp = fopen("pd_2.pdf" , "wb");

	char buffer[BUFF_SIZE]={0};

	int file_size, n;
	if(recv(sockfd, &file_size, sizeof(file_size), 0)<0){
		perror("Error in receive file size: Client");
		exit(EXIT_FAILURE);
	}
	cout<<"File size : Client: "<< file_size<<endl;
	while ((n = recv(sockfd , buffer , BUFF_SIZE, 0) ) > 0 && file_size > 0){
		// cout<<"Buff "<<buffer<<endl;
		fwrite (buffer , sizeof (char), n, fp);
		memset ( buffer , '\0', BUFF_SIZE);
		file_size = file_size - n;
	}

	cout<<"File received!"<<endl;
	
	fclose(fp);
	
	close(sockfd);
}

void *serverThreadFunc(void *ptr){
	cout<<(char*)ptr<<endl;
	struct sockaddr_in server_addr;
	struct sockaddr_in new_serv_addr;
	int newsocket;

	int server_socket_fd, new_server_socket_fd;
	if((server_socket_fd=socket(AF_INET,SOCK_STREAM,0))<0){
		cout<<"Error in socket creation"<<endl;
		exit(1);
	}
	cout<<"Socket created : server"<<endl;

	memset(&server_addr,'\0',sizeof(server_addr));

	server_addr.sin_family=AF_INET;
	server_addr.sin_port=htons(PORT2);
	server_addr.sin_addr.s_addr=inet_addr("127.0.0.1");
	

	int bindStatus = bind(server_socket_fd,(struct sockaddr*)&server_addr,sizeof(server_addr));
	
	if(bindStatus < 0){
		cout<<"Error in binding"<<endl;
		exit(1);
	}
	cout<<"Bind successful : server"<<endl;

	/*int listen(int sockfd, int backlog);
	   The sockfd argument is a file descriptor that refers to a socket of
       type SOCK_STREAM or SOCK_SEQPACKET.

       The backlog argument defines the maximum length to which the queue of
       pending connections for sockfd may grow. 
       When multiple clients connect to the server, the server then holds the 
       incoming requests in a queue. The clients are arranged in the queue, and 
       the server processes their requests one by one as and when queue-member proceeds. 
       The nature of this kind of connection is called queued connection.

	   If a connection request arrives when the queue is full, the client may receive an error with
       an indication of ECONNREFUSED or, if the underlying protocol supports
       retransmission, the request may be ignored so that a later reattempt
       at connection succeeds.

       Min backlog value can be: 0. There is a system-defined number of maximum connections 
       on any one queue. This prevents processes from hogging system resources by setting 
       the backlog value to be very large and then ignoring all connection requests.*/
	if(listen(server_socket_fd, 4)!=0){
		cout<<"Error in Listening"<<endl;
		exit(1);
	}
	else{
		cout<<"Listening now : Server"<<endl;
	}

	// int addrlen=sizeof(new_serv_addr);
	int addrlen=sizeof(server_addr);
	// while(1){
	if((new_server_socket_fd = accept(server_socket_fd,(struct sockaddr*)&server_addr,(socklen_t*)&addrlen))<0){
	
	// if((new_server_socket_fd = accept(server_socket_fd,(struct sockaddr*)&new_serv_addr,(socklen_t*)&addrlen))<0){
		perror("Error in accept");
		exit(EXIT_FAILURE);
	}

	cout<<"Accept successful : Server "<<endl;

	/* 
	Msg passing code:
	char msg[BUFF_SIZE]="10111Servvvvvvvvvver says hi, client";
	// int msg=10;
	// size_t msg_len=strlen(msg);
	if(send(new_server_socket_fd, msg, sizeof(msg), 0)<0){
		perror("error in send : Server");
		exit(EXIT_FAILURE);
	}
	cout<<"Msg size sent by Server "<<strlen(msg)<<endl;
	cout<<"Msg sent by Server "<<msg<<endl;
	*/


	FILE *fp=fopen("afile_peer2S.txt", "rb");


	/* SEEK_END : It denotes end of the file.
	   SEEK_SET : It denotes starting of the file.
	   SEEK_CUR : It denotes file pointer’s current position.*/


	fseek(fp, 0, SEEK_END);
	//above means that we move the pointer by 0 distance with respect to end of file i.e pointer now points to end of the file. 
	int file_size=ftell(fp);
	cout<<"File size : Server : "<<file_size<<endl;
	rewind(fp);

	if(send(new_server_socket_fd, &file_size, sizeof(file_size), 0)<0){
		perror("error in sending file size : Server");
		exit(EXIT_FAILURE);
	}

	char Buffer[BUFF_SIZE];
	int n;
	while((n=fread(Buffer, sizeof(char), BUFF_SIZE, fp)) > 0 && file_size>0){
		if(send(new_server_socket_fd, Buffer, n, 0)<0){
			perror("error in sending file : Server");
			exit(EXIT_FAILURE);
		}
		// cout<<"in buff"<<Buffer<<endl;
		memset(Buffer, '\0', BUFF_SIZE);
		file_size=file_size-n;
	}
		
	cout<<"Send succesful : Server"<<endl;



	close(server_socket_fd);
	close(new_server_socket_fd);
}

// void serveClientRequest(struct sockaddr_in new_sock_addr, int new_sock_fd){

// }
