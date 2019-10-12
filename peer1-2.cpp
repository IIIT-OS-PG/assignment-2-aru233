
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
#include <openssl/sha.h>
#include <bits/stdc++.h> 

using namespace std;

// #define BUFF_SIZE 2048
#define BUFF_SIZE 512
// #define CHUNK 524288 //512KB
// #define CHUNK 2048
#define CHUNK 512
#define PORT2 2001
#define PORT1 2002
#define SERVER_THREADS 4
#define CLIENT_THREADS 3

pthread_mutex_t mlock;

void *serverThreadFunc(void *ptr);
void *clientThreadFunc(void *ptr);
void *serveRequest(void *ptr);
void *fileDownloadFunc(void *ptr);

struct threadData{
	int sockfd;
	int portNo;
	int chunkNo;
	FILE* fp;
	int fileSize;
};
typedef struct threadData struct_tData;

struct userDetail{
	int sockfd;
	char *fileNm;
	string sha;
	int fileSz;
	char *gid;
	struct sockaddr_in serv_addr;
};
typedef struct userDetail struct_userDet;


// struct upl_clientDetail{
// 	int sockfd;

// 	struct server_addr serv_addr;
// };
// typedef struct clientDetail struct_uplClientDet;


string sha256_hash_string (unsigned char hash[SHA256_DIGEST_LENGTH]){
   cout<<"Entered sha256_hash_string()"<<endl;
   stringstream ss;
   for(int i = 0; i < SHA256_DIGEST_LENGTH; i++)
    {
        ss << hex << setw(2) << setfill('0') << (int)hash[i];
    }
    return ss.str();
}

string sha256(const string str){
	cout<<"Entered sha256()"<<endl;
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, str.c_str(), str.size());
    SHA256_Final(hash, &sha256);
    stringstream ss;
    for(int i = 0; i < SHA256_DIGEST_LENGTH; i++)
    {
        ss << hex << setw(2) << setfill('0') << (int)hash[i];
    }
    return ss.str();
}

string sha256_of_chunks(FILE* f,int size){
	cout<<"Entered sha256_of_chunks()"<<endl;
	if(!f){
		cout<<"OOPS! Returning Null"<<endl;
		return NULL;
	}
	cout<<"no NULL!"<<endl;
	string finalSha="";
	unsigned char hashcal[SHA256_DIGEST_LENGTH];
	SHA256_CTX sha256;
	SHA256_Init(&sha256);
	const int buf=512;
	unsigned char* buffer=(unsigned char*)malloc(buf+1);
	int n=0;
	if(!buffer)
		return NULL;
	while((n=fread(buffer,sizeof(char),buf,f)) > 0 && size>0)
	{
		SHA256_Update(&sha256,buffer,n);
		SHA256_Final(hashcal,&sha256);
		string outBuf=sha256_hash_string(hashcal);
		string ans=outBuf.substr(0,20);
		finalSha += ans;
		memset(buffer,'\0',512);
		size -= n;
	}
	fclose(f);
	cout<<"Exiting sha256_of_chunks()"<<endl;
	return finalSha;
}

int main(){

	pthread_t serverThread, clientThread;
	struct_tData serverData, clientData;
	// char *msgServer="In server thread";
	// char *msgClient="In client thread";

	cout<<"Enter Server Port"<<endl;
	cin>>serverData.portNo;

	cout<<"Enter Client Port"<<endl;
	cin>>clientData.portNo;

	int serv_thread_status=pthread_create(&serverThread, NULL, serverThreadFunc, (void*)&serverData);
	int client_thread_status=pthread_create(&clientThread, NULL, clientThreadFunc, (void*)&clientData);
	

	pthread_join(serverThread, NULL);
	pthread_join(clientThread, NULL);

	cout<<"Server thread is back"<<endl;
	cout<<"Client thread is back"<<endl;
	exit(0);
}

void *clientThreadFunc(void *ptr){
	// cout<<(char*)ptr<<endl;

	struct_tData *clientData=(struct_tData*)ptr;
	int portNum=clientData->portNo;
	int sockfd=socket(AF_INET, SOCK_STREAM, 0);
	if(sockfd<0){
		perror("Socket creation failed : client");
		pthread_exit(NULL);
	}
	cout<<"Socket created : client"<<endl;

	struct sockaddr_in serv_addr;
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons( portNum );
	serv_addr.sin_addr.s_addr=inet_addr("127.0.0.1");

	// cout<<"Enter no."<<endl;
	// int num;
	// cin>>num;

	//In (struct sockaddr*)&serv_addr, sockaddr_in doesn't work!
	if(connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr))<0){
		perror("Connect failed : Client");
		pthread_exit(NULL);
	}
	cout<<"Connect successful : Client "<<endl;
	vector<string> cmdVec;
	cin.ignore();
	while(true){
		string cmd, cmd1;
		int ack;
		cout<<"Enter command"<<endl;
		getline(cin, cmd);
		cout<<"Command entered:Client:"<<cmd<<endl;
		cmd1=cmd;
		char *cmdName=strtok((char*)cmd1.c_str()," ");
		cout<<"Commd name : Client: "<<cmdName<<endl;
		// cout<<"Commd name1 : Client: "<<cmd1<<endl;
		cmdVec.clear();
		if(strcmp(cmdName, "create_user")==0){
			cout<<"Inside create_user:Client"<<endl;
			while((cmdName=strtok(NULL," "))!=NULL){
				// cout<<"whaaiiil"<<cmdName<<endl;
				cmdVec.push_back(cmdName);
				cout<<cmdName<<endl;
			}
			cout<<"outside vec while"<<endl;
			if(cmdVec.size()!=2){
				cout<<"Wrong command format; See: create_user​ <user_id> <passwd>"<<endl;
				// pthread_exit(NULL);
				continue;
			}
			cout<<"Entered create_user: Client: "<<endl;
			if(send(sockfd,(char*)cmd.c_str(),cmd.length(),0)<0){
				cout<<"unsuccessful cmd send:Client"<<endl;
			}
			//Receive acknowledgement
			recv(sockfd, &ack, sizeof(ack),0);
			cout<<"Ack received in create_user: Client: "<<ack<<endl;

		}
		else if(strcmp(cmdName, "upload_file")==0){
			cmdVec.push_back(cmdName);//Need to push_back "uploadd_file" too in the vector so as to make the whole command
			cout<<"Inside upload_file:CLient"<<endl;
			while((cmdName=strtok(NULL," "))!=NULL){
				// cmdName=strtok(NULL," ");
				cmdVec.push_back(cmdName);
			}
			if(cmdVec.size()!=3){
				cout<<"Wrong command format; See: upload_file​ <file_path> <group_id>"<<endl;
				// pthread_exit(NULL);
				continue;
			}
			cout<<"Entered upload_file: Client"<<endl;
			string fileName=cmdVec[1];
			string grpID=cmdVec[2];
			FILE *fp=fopen((char*)fileName.c_str(), "rb");
			/*File size:*/
			struct stat st;
			stat((char*)fileName.c_str(), &st);
			int file_size = st.st_size;
			string str_file_size=to_string(file_size);
			// cout<<"File size : ServReq : "<<file_size<<endl;
			string fileSHA=sha256_of_chunks(fp, file_size);
			cout<<"SHA as calculated:Client:"<<fileSHA<<endl;
			cout<<"Calculated SHA length:Client:"<<fileSHA.length()<<endl;

			// struct_userDet *udata=(struct_userDet *)malloc(sizeof(struct_userDet));
			// udata->fileNm=fileName;
			// udata->sha=fileSHA;
			// udata->fileSz=file_size;
			// udata->gid=grpID;
			string cmdToSend=cmdVec[0]+" "+fileName+" "+str_file_size+" "+grpID;
			//Not sending SHA here
			cout<<"Sending this cmd to tracker for upload_file: "<<cmdToSend<<endl;


			if(send(sockfd,cmdToSend.c_str(),cmdToSend.length(),0)<0){
				cout<<"Unable to send cmd(in upload_file):Client"<<endl;
			}
			//Receive acknowledgement
			recv(sockfd, &ack, sizeof(ack),0);
			cout<<"Ack received in upload_file: Client: "<<ack<<endl;

			int lenSHA=fileSHA.length();
			int cnt=0;
			cout<<"GOing to start sending SHA from Client"<<endl;
			while(cnt<lenSHA){
				string sha_20=fileSHA.substr(cnt,20);
				cnt+=20;
				cout<<"SHA sent:Client: "<<sha_20<<endl;
				send(sockfd, (char*)sha_20.c_str(), sha_20.length(), 0);
				recv(sockfd, &ack, sizeof(ack),0);

			}
			char *msg="endofSHA";
			send(sockfd, msg, sizeof(msg), 0);
		}

	}


	/*
	//File receiving code:
	FILE *fp = fopen("textp1CCCCC.txt" , "wb+");
	// int fp= fopen("afile_peer1C.txt" , "wb");
	// int file_desc=fileno(fp);

	int file_size, temp_file_size, n;
	// int chunk_no=2;

	// if(send(sockfd, &chunk_no, sizeof(chunk_no), 0)<0){
	// 	perror("Error in sending chunk no.: Client");
	// 	pthread_exit(NULL);
	// }

	// int startPosInFile;
	// startPosInFile=CHUNK*(chunk_no-1);
	// cout<<"Start pos for chunk : Client "<<startPosInFile<<endl;
	// // fseek(fp, startPosInFile, SEEK_SET);

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

	//******creating an empty file

	// int pos=lseek(file_desc, 0, SEEK_SET);
	// cout<<"fp b4 null : CLient: "<<pos<<endl;
	fseek(fp, 0, SEEK_SET);
	// cout<<"fp b4 null : CLient: "<<ftell(fp)<<endl;
	

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

	cout<<"Null file created : Client"<<endl;
	// fclose(fp);
	// close(file_desc);

	// fp = fopen("textp1C.txt" , "ab+");
	// // int fp= fopen("afile_peer1C.txt" , "wb");
	// file_desc=fileno(fp);

	//******************************************
	// lseek(file_desc, 0, SEEK_SET);
	// fseek(fp, 0, SEEK_SET);


	//****writing specific chunk to file

	pthread_t clientDownl[CLIENT_THREADS];
	int chooseChunk[CLIENT_THREADS]={1,2,3};
	int portClientThr[CLIENT_THREADS]={2051, 2052, 2053};
	int i=0;

	pthread_mutex_init(&mlock, NULL);
	while(i<CLIENT_THREADS){
		struct_tData *client_data_download=(struct_tData*)malloc(sizeof(struct_tData));
		client_data_download->fp=fp;
		client_data_download->portNo=portClientThr[i];
		client_data_download->chunkNo=chooseChunk[i];
		client_data_download->fileSize=file_size;
		pthread_create(&clientDownl[i],NULL,fileDownloadFunc, (void*)client_data_download);

		i++;

	}
	i=0;
	while(i<CLIENT_THREADS){
		pthread_join(clientDownl[i], NULL);
		i++;
	}
	pthread_mutex_destroy(&mlock);

	// cout<<"File received!"<<endl;
	
	fclose(fp);

	*/
	// close(file_desc);
	
	close(sockfd);
}

void *fileDownloadFunc(void *ptr){

	struct_tData *client_data_download=(struct_tData*)ptr;
	FILE* fp=client_data_download->fp;
	int portNum=client_data_download->portNo;

	int sockfd=socket(AF_INET, SOCK_STREAM, 0);
	if(sockfd<0){
		perror("Socket creation failed : fileDownloadFunc");
		pthread_exit(NULL);
	}
	cout<<"Socket created : fileDownloadFunc"<<endl;

	struct sockaddr_in serv_addr;
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons( portNum );
	serv_addr.sin_addr.s_addr=inet_addr("127.0.0.1");

	//In (struct sockaddr*)&serv_addr, sockaddr_in doesn't work!
	if(connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr))<0){
		perror("Connect failed : fileDownloadFunc");
		pthread_exit(NULL);
	}
	cout<<"Connect successful : fileDownloadFunc "<<endl;

	//File receiving code:
	// FILE *fp = fopen("textp1CCCCC.txt" , "wb+");
	////No need to open file in this thread, as file already opened by client
	// int fp= fopen("afile_peer1C.txt" , "wb");
	// int file_desc=fileno(fp);

	int file_size, temp_file_size, n, startPosInFile;
	int chunk_no=client_data_download->chunkNo;
	startPosInFile=CHUNK*(chunk_no-1);

	if(send(sockfd, &chunk_no, sizeof(chunk_no), 0)<0){
		perror("Error in sending chunk no.: fileDownloadFunc");
		pthread_exit(NULL);
	}

	file_size=client_data_download->fileSize;

	int my_buff_size;
	if(file_size<BUFF_SIZE){
		my_buff_size=file_size;
	}
	else{
		my_buff_size=BUFF_SIZE;
	}
	cout<<"my_buff_size : fileDownloadFunc : "<<my_buff_size<<endl;

	char buffer[my_buff_size]={'\0'};

	pthread_mutex_lock(&mlock); 
	// lseek(file_desc, startPosInFile, SEEK_SET);
	fseek(fp, startPosInFile, SEEK_SET);
	cout<<"Downloading chunk no.:fileDownloadFunc: "<<chunk_no<<endl;
	cout<<"Start pos for chunk : fileDownloadFunc : "<<startPosInFile<<endl;
	

	temp_file_size=CHUNK;//reading a CHUNK at a time
	while ((n = recv(sockfd , buffer , my_buff_size, 0) ) > 0 && file_size > 0){
		// cout<<"n : fileDownloadFunc: "<<n<<endl;
		cout<<"Buff peer1 : fileDownloadFunc: "<<buffer<<endl;
		fwrite (buffer , sizeof (char), n, fp);
		// write(file_desc, buffer, n);
		memset ( buffer , '\0', my_buff_size);
		temp_file_size = temp_file_size - n;
		// cout<<"BLAH"<<endl;
	}
	pthread_mutex_unlock(&mlock);
	pthread_exit(NULL);
}

void *serverThreadFunc(void *ptr){

	struct_tData *serverData=(struct_tData*)ptr;
	int portNum=serverData->portNo;
	// cout<<(char*)ptr<<endl;
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
	server_addr.sin_port=htons(portNum);
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
	pthread_t servethread[SERVER_THREADS];
	int i=SERVER_THREADS;
	while(1){
		if((new_server_socket_fd = accept(server_socket_fd,(struct sockaddr*)&server_addr,(socklen_t*)&addrlen))<0){
		
		// if((new_server_socket_fd = accept(server_socket_fd,(struct sockaddr*)&new_serv_addr,(socklen_t*)&addrlen))<0){
			perror("Error in accept");
			pthread_exit(NULL);
		}
		else{
			cout<<"Accept successful : Server "<<endl;
			// pthread_t servethread;
			struct_tData *tdata=(struct_tData *)malloc(sizeof(struct_tData));
			tdata->sockfd=new_server_socket_fd;

			int serv_thread_status=pthread_create(&servethread[i], NULL, serveRequest, (void*)tdata);
			if(serv_thread_status<0){
				perror("Error in creating thread to serve request : Server");
				pthread_exit(NULL);
			}

			// pthread_detach(servethread);
			pthread_detach(servethread[i]);
			i++;
			
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

	struct_tData *tstruct;
	tstruct=(struct_tData*)ptr;
	// cout<<"Have typecasted the void* "<<endl;

	int fd=tstruct->sockfd;

	// cout<<"received fd in parameter is: "<<fd<<endl;

	//receiving chunk no. from fileDownloadFunc(aka client thread)
	int chunk_no;
	if(recv(fd, &chunk_no, sizeof(chunk_no), 0)<0){
		perror("Error in receive chunk no.: ServReq");
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

	// if(send(fd, &file_size, sizeof(file_size), 0)<0){
	// 	perror("error in sending file size : ServReq");
	// 	pthread_exit(NULL);
	// }
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

	pthread_exit(NULL);
}
