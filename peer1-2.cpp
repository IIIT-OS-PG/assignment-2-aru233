
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
#include <sys/stat.h>
#include <pthread.h>

using namespace std;

// #define BUFF_SIZE 2048
#define BUFF_SIZE 512
// #define CHUNK 524288 //512KB
// #define CHUNK 2048
#define CHUNK 512
#define PORT2 2001
#define PORT1 2002

void *serverThreadFunc(void *ptr);
void *clientThreadFunc(void *ptr);
void *serveRequest(void *ptr);

struct threadData{
	int sockfd;
};

int main(){

	pthread_t serverThread, clientThread;
	char *msgServer="In server thread";
	char *msgClient="In client thread";

	int serv_thread_status=pthread_create(&serverThread, NULL, serverThreadFunc, (void*)msgServer);
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
		pthread_exit(NULL);
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
		pthread_exit(NULL);
	}
	cout<<"Connect successful : Client "<<endl;

	/*//Msg receiving code:
	char msg[BUFF_SIZE];
	// int msg;
	// recv(sockfd, &msg_len, sizeof(msg_len), 0);
	if(recv(sockfd , msg , sizeof(msg), 0) < 0){
		perror("Error in receive : Client");
		pthread_exit(NULL);
	}
	cout<<"Msg size received at client: "<< strlen(msg)<<endl;
	cout<<"Msg received at client: "<< msg<<endl;
	*/

	//File receiving code:
	FILE *fp = fopen("textp1CCCCC.txt" , "wb+");
	// int fp= fopen("afile_peer1C.txt" , "wb");
	// int file_desc=fileno(fp);

	int file_size, temp_file_size, n;
	int chunk_no=2;

	if(send(sockfd, &chunk_no, sizeof(chunk_no), 0)<0){
		perror("Error in sending chunk no.: Client");
		pthread_exit(NULL);
	}

	int startPosInFile;
	startPosInFile=CHUNK*(chunk_no-1);
	cout<<"Start pos for chunk : Client "<<startPosInFile<<endl;
	// fseek(fp, startPosInFile, SEEK_SET);

	if(recv(sockfd, &file_size, sizeof(file_size), 0)<0){
		perror("Error in receive file size: Client");
		pthread_exit(NULL);
	}
	cout<<"File size : Client: "<< file_size<<endl;
	temp_file_size=file_size;

	int my_buff_size;
	if(file_size<BUFF_SIZE){
		my_buff_size=file_size;
	}
	else{
		my_buff_size=BUFF_SIZE;
	}
	cout<<"my_buff_size : Client : "<<my_buff_size<<endl;

	char buffer[my_buff_size]={'\0'};

	//creating an empty file
	// int pos=lseek(file_desc, 0, SEEK_SET);
	// cout<<"fp b4 null : CLient: "<<pos<<endl;
	fseek(fp, 0, SEEK_SET);
	cout<<"fp b4 null : CLient: "<<ftell(fp)<<endl;
	

	memset ( buffer , '\0', BUFF_SIZE);
	// exit(0);
	n=my_buff_size;
	while (temp_file_size > 0){
		// cout<<"Buff peer1 client: "<<buffer<<endl;
		fwrite (buffer , sizeof (char), n, fp);
		// write(file_desc, buffer, n);
		// memset ( buffer , '\0', BUFF_SIZE);
		temp_file_size = temp_file_size - n;
		// cout<<"BLAH"<<endl;
	}
	// fclose(fp);
	// close(file_desc);

	// fp = fopen("textp1C.txt" , "ab+");
	// // int fp= fopen("afile_peer1C.txt" , "wb");
	// file_desc=fileno(fp);

	// lseek(file_desc, 0, SEEK_SET);
	// fseek(fp, 0, SEEK_SET);


	//writing specific chunk to file
	
	// lseek(file_desc, startPosInFile, SEEK_SET);
	fseek(fp, startPosInFile, SEEK_SET);

	temp_file_size=CHUNK;
	while ((n = recv(sockfd , buffer , my_buff_size, 0) ) > 0 && file_size > 0){
		cout<<"n : Client: "<<n<<endl;
		cout<<"Buff peer1 : Client: "<<buffer<<endl;
		fwrite (buffer , sizeof (char), n, fp);
		// write(file_desc, buffer, n);
		memset ( buffer , '\0', my_buff_size);
		temp_file_size = temp_file_size - n;
		// cout<<"BLAH"<<endl;
	}

	cout<<"File received!"<<endl;
	
	fclose(fp);
	// close(file_desc);
	
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
		pthread_exit(NULL);
	}
	cout<<"Socket created : server"<<endl;

	memset(&server_addr,'\0',sizeof(server_addr));

	server_addr.sin_family=AF_INET;
	server_addr.sin_port=htons(PORT1);
	server_addr.sin_addr.s_addr=inet_addr("127.0.0.1");
	

	int bindStatus = bind(server_socket_fd,(struct sockaddr*)&server_addr,sizeof(server_addr));
	
	if(bindStatus < 0){
		cout<<"Error in binding"<<endl;
		pthread_exit(NULL);
	}
	cout<<"Bind successful : server"<<endl;

	int listencount=0;

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
	if(listen(server_socket_fd, 5)!=0){
		cout<<"Error in Listening"<<endl;
		pthread_exit(NULL);
	}
	else{
		listencount++;
		cout<<"Listening now : Server "<<endl;
	}

	// int addrlen=sizeof(new_serv_addr);
	int addrlen=sizeof(server_addr);
	while(1){
		if((new_server_socket_fd = accept(server_socket_fd,(struct sockaddr*)&server_addr,(socklen_t*)&addrlen))<0){
		
		// if((new_server_socket_fd = accept(server_socket_fd,(struct sockaddr*)&new_serv_addr,(socklen_t*)&addrlen))<0){
			perror("Error in accept");
			pthread_exit(NULL);
		}
		else{
			cout<<"Accept successful : Server "<<endl;
			pthread_t servethread;
			struct threadData tdata;
			tdata.sockfd=new_server_socket_fd;

			int serv_thread_status=pthread_create(&servethread, NULL, serveRequest, (void*)&tdata);
			if(serv_thread_status<0){
				perror("Error in creating thread to serve request : Server");
				pthread_exit(NULL);
			}

			// pthread_detach(servethread);
			pthread_join(servethread, NULL);
			
		}
	}
	

	cout<<"Accept successful : Server "<<endl;

	/* 
	Msg passing code:
	char msg[BUFF_SIZE]="10111Servvvvvvvvvver says hi, client";
	// int msg=10;
	// size_t msg_len=strlen(msg);
	if(send(new_server_socket_fd, msg, sizeof(msg), 0)<0){
		perror("error in send : Server");
		pthread_exit(NULL);
	}
	cout<<"Msg size sent by Server "<<strlen(msg)<<endl;
	cout<<"Msg sent by Server "<<msg<<endl;
	*/


	close(server_socket_fd);
	// close(new_server_socket_fd);
}

void *serveRequest(void *ptr){
	char *filenm="text.txt";
	cout<<"Inside serveRequest funct"<<endl;
	FILE *fp=fopen(filenm, "rb");


	/* SEEK_END : It denotes end of the file.
	   SEEK_SET : It denotes starting of the file.
	   SEEK_CUR : It denotes file pointer’s current position.*/

	struct threadData *tstruct;
	tstruct=(struct threadData*)ptr;
	cout<<"Have typecasted the void* "<<endl;

	int fd=tstruct->sockfd;

	// cout<<"received fd in parameter is: "<<fd<<endl;

	int chunk_no;
	if(recv(fd, &chunk_no, sizeof(chunk_no), 0)<0){
		perror("Error in receive chunk no.: Server");
		pthread_exit(NULL);
	}

	cout<<"Chunk no. recvd in ServReq "<<chunk_no<<endl;
	

	/*File size:*/
	struct stat st;
	stat(filenm, &st);
	int file_size = st.st_size;
	cout<<"File size : ServReq : "<<file_size<<endl;

	/*Finding file size:	
	// fseek(fp, 0, SEEK_END);
	// //above means that we move the pointer by 0 distance with respect to end of file i.e pointer now points to end of the file. 
	// int file_size=ftell(fp);
	// cout<<"File size : ServReq : "<<file_size<<endl;
	// rewind(fp);
	*/

	if(send(fd, &file_size, sizeof(file_size), 0)<0){
		perror("error in sending file size : ServReq");
		pthread_exit(NULL);
	}
	int my_buff_size;
	if(file_size<BUFF_SIZE){
		my_buff_size=file_size;
	}
	else{
		my_buff_size=BUFF_SIZE;
	}
	char Buffer[my_buff_size];
	int n;

	if(CHUNK*(chunk_no-1)<=file_size){
		int startPosInFile=CHUNK*(chunk_no-1);
		fseek(fp, startPosInFile, SEEK_SET);
		cout<<"File ptr pos before : ServReq : "<<ftell(fp)<<endl;

		cout<<"fp start pos : ServReq : "<<startPosInFile<<endl;
		// cout<<"SEEK_SET : ServReq : "<<SEEK_SET<<endl;


		int temp_file_size=CHUNK;

		cout<<"my_buff_size : ServReq : "<<my_buff_size<<endl;
		cout<<"temp_file_size : ServReq :"<<temp_file_size<<endl;

		// while(())
		while(temp_file_size>0){
			n=fread(Buffer, sizeof(char), my_buff_size, fp);
			cout<<"in buff: ServReq: "<<Buffer<<endl;
			if(n < 0) {
				cout << "extra read !!" << endl;
				break;
			}
			
			if(send(fd, Buffer, n, 0)<0){
				perror("error in sending file : ServReq");
				pthread_exit(NULL);
			}
			
			memset(Buffer, '\0', my_buff_size);
			temp_file_size=temp_file_size-n;
		}

		cout<<"File ptr pos now : ServReq : "<<ftell(fp)<<endl;
	}
	close(fd);	
	fclose(fp);
	
		
	cout<<"Send succesful : ServeReq"<<endl;
}
