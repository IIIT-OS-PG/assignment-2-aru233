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
#define SERVER_THREADS 100
#define CLIENT_THREADS 3
#define IN_GROUP 4
#define IN_LIST 2
#define LOGGED_IN 3
#define NOT_LOGGED_IN -2
#define NOT_IN_GROUP -3
#define IS_OWNER -4
#define NOT_OWNER -44
#define NOT_PENDING_JOIN -5
#define NO_GROUPS -6
#define NO_USER -7
#define REQ_FOR_VEC "Request for Vector"
#define REQ_FOR_DATA "Request for Data"

pthread_mutex_t mlock;
int canStartUpload=0;


void *serverThreadFunc(void *ptr);
void *clientThreadFunc(void *ptr);
void *serveRequest(void *ptr);
void *fileDownloadFunc(void *ptr);

struct portNums{
	int serverPortNo;
	int trackerPortNo;
};
typedef struct portNums struct_portNums;

struct threadData{
	int sockfd;
	int portNo;
	int chunkNo;
	FILE* fp;
	int fileSize;
};
typedef struct threadData struct_tData;

struct threadDownlData{
	string ip;
	int portNo;
	FILE *fp;	
	string filenm;
	vector<int> chunksToDownload;
	string sha;
};
typedef struct threadDownlData struct_tDownlData;

struct userDetail{
	int sockfd;
	char *fileNm;
	string sha;
	int fileSz;
	char *gid;
	struct sockaddr_in serv_addr;
};
typedef struct userDetail struct_userDet;

map<string, vector<int> > seederFileChunkMap;

string sha256_hash_string (unsigned char hash[SHA256_DIGEST_LENGTH]){
   // cout<<"Entered sha256_hash_string()"<<endl;
   stringstream ss;
   for(int i = 0; i < SHA256_DIGEST_LENGTH; i++)
    {
        ss << hex << setw(2) << setfill('0') << (int)hash[i];
    }
    return ss.str();
}

string sha256(const string str){
	// cout<<"Entered sha256()"<<endl;
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
	// cout<<"no NULL!"<<endl;
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
		cout<<"sha piece in sha256_of_chunks "<<ans<<endl;
		memset(buffer,'\0',512);
		size -= n;
	}
	fclose(f);
	cout<<"Exiting sha256_of_chunks()"<<endl;
	return finalSha;
}

void createEmptyFile(string filename, int file_size){
	FILE *fp = fopen((char*)filename.c_str() , "wb+");

	int temp_file_size, n;
	temp_file_size=file_size;

	int my_buff_size;
	if(file_size<BUFF_SIZE){
		my_buff_size=file_size;
	}
	else{
		my_buff_size=BUFF_SIZE;
	}
	// cout<<"my_buff_size : Client : "<<my_buff_size<<endl;

	char buffer[my_buff_size]={'\0'};
	fseek(fp, 0, SEEK_SET);
	// cout<<"fp b4 null : CLient: "<<ftell(fp)<<endl;	

	memset ( buffer , '\0', BUFF_SIZE);
	n=my_buff_size;
	while (temp_file_size > 0){
		// cout<<"Buff peer1 client: "<<buffer<<endl;
		fwrite (buffer , sizeof (char), n, fp);
		memset ( buffer , '\0', BUFF_SIZE);
		temp_file_size = temp_file_size - n;
		// cout<<"BLAH"<<endl;
	}
	fclose(fp);	
	cout<<"Null file created : Client"<<endl;
}

void connectToSeedersForVec(vector<pair<string,int> >& seederVec, vector<vector<int> >& seederChunkVec, string filenm){
	int seederVecSize=seederVec.size();
	int ack=1;
	for(int i=0;i<seederVecSize;i++){
		int opt=1;
		int portNum=seederVec[i].second;
		string ip=seederVec[i].first;
		cout<<"Trying to connect to server with:"<<endl;
		cout<<"portNum in connectToSeeders:"<<portNum<<endl;
		cout<<"ip in connectToSeeders:"<<ip<<endl;

		int sockfd=socket(AF_INET, SOCK_STREAM, 0);
		if(sockfd<0){
			perror("Socket creation failed :client's connectToSeeders");
			pthread_exit(NULL);
		}
		cout<<"Socket created : client's connectToSeeders"<<endl;

		struct sockaddr_in serv_addr;

		memset(&serv_addr,'\0',sizeof(serv_addr));
		
		serv_addr.sin_family = AF_INET;
		serv_addr.sin_port = htons(portNum);
		serv_addr.sin_addr.s_addr=inet_addr(ip.c_str());

		if(setsockopt(sockfd, SOL_SOCKET,SO_REUSEADDR|SO_REUSEPORT,&opt,sizeof(opt))){
			cout<<"setsockopt"<<endl;
			exit(EXIT_FAILURE);
		}

		//In (struct sockaddr*)&serv_addr, sockaddr_in doesn't work!
		if(connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr))<0){
			perror("Connect failed:Client");
			pthread_exit(NULL);
		}
		cout<<"Connect successful:Client "<<endl;

		recv(sockfd,&ack, sizeof(ack),0);//to sync

		char *msg=REQ_FOR_VEC;
		send(sockfd,msg,1028,0);//using sizeof(msg) instead of 1028 didn't send proper msg
		cout<<"sent the msg:Client:"<<msg<<endl;

		recv(sockfd,&ack, sizeof(ack),0);//to sync
		cout<<"sync"<<endl;

		//sending filename to server
		send(sockfd, (char*)filenm.c_str(), filenm.length(),0);
		cout<<"filename sent to server:"<<filenm<<endl;

		//receiving chunkVec size from server
		int chunkVecSize;
		recv(sockfd,&chunkVecSize,sizeof(chunkVecSize),0);
		cout<<"Size of chunkVec that client has recvd:"<<chunkVecSize<<endl;

		send(sockfd,&ack, sizeof(ack),0);//to sync
		cout<<"sync1"<<endl;

		//Receiving the chunkNos in seederChunkVec from tracker
		cout<<"Recvd chunkNo:"<<endl;
		for(int j=0;j<chunkVecSize;j++){
			int chunkNo;
			recv(sockfd,&chunkNo,sizeof(chunkNo),0);
			cout<<chunkNo<<" ";

			seederChunkVec[i].push_back(chunkNo);

			send(sockfd,&ack, sizeof(ack),0);//to sync
			cout<<"sync2"<<endl;
		}
		close(sockfd);

	}
}

void pieceSelection(vector<vector<int> >& seederChunkVec, vector<vector<int> >& pieceSelectVec, int noOfChunksInFile){
	cout<<"Entered pieceSelection()"<<endl;
	vector<int> vis(noOfChunksInFile+1, 0);//chunkNos are 1-indexed
	int chunkCount=1;
	cout<<"noOfChunksInFile: "<<noOfChunksInFile<<endl;
	while(chunkCount<=noOfChunksInFile){
		for(int i=0;i<seederChunkVec.size();i++){
			// vector<int> chunkRow=seederChunkVec[i];
			cout<<"ChunkRowSize for a client:"<<seederChunkVec[i].size()<<endl;
			cout<<"Curr chunk: "<<seederChunkVec[i][0]<<endl;
			if(seederChunkVec[i].size()>0 && vis[seederChunkVec[i][0] ]==0){
				pieceSelectVec[i].push_back(seederChunkVec[i][0]);
				vis[seederChunkVec[i][0]]=1;
				seederChunkVec[i].erase(seederChunkVec[i].begin());
				// cout<<"New row length:"<<
				chunkCount++;
				cout<<"chunkCount:"<<chunkCount<<endl;
				
			}
			else if(seederChunkVec[i].size()>0 && vis[seederChunkVec[i][0] ]==1){
				while(seederChunkVec[i].size()>0 && vis[seederChunkVec[i][0] ]==1){
					seederChunkVec[i].erase(seederChunkVec[i].begin());			
				}
				if(seederChunkVec[i].size()>0 && vis[seederChunkVec[i][0] ]==0){
					pieceSelectVec[i].push_back(seederChunkVec[i][0]);
					vis[seederChunkVec[i][0]]=1;
					seederChunkVec[i].erase(seederChunkVec[i].begin());
					// cout<<"New row length:"<<
					chunkCount++;
					cout<<"chunkCount:"<<chunkCount<<endl;
				}
			}
			if(chunkCount>noOfChunksInFile){
				break;
			}

		}
	}
	
	cout<<"Exiting pieceSelection()"<<endl;
}

void *chunkDownloadFunc(void *ptr){	
	cout<<"in chunkDownloadFunc "<<endl;

	struct_tDownlData *client_data_download=(struct_tDownlData*)ptr;
	FILE* fp=client_data_download->fp;
	string sourcefilenm=client_data_download->filenm;
	int portNum=client_data_download->portNo;
	string ip=client_data_download->ip;
	vector<int> chunksToTake = client_data_download->chunksToDownload;
	string sha=client_data_download->sha;

	int sockfd=socket(AF_INET, SOCK_STREAM, 0), opt=1, ack=1;
	if(sockfd<0){
		perror("Socket creation failed :chunkDownloadFunc");
		pthread_exit(NULL);
	}
	cout<<"Socket created:chunkDownloadFunc"<<endl;

	struct sockaddr_in serv_addr;
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons( portNum );
	serv_addr.sin_addr.s_addr=inet_addr(ip.c_str());

	if(setsockopt(sockfd, SOL_SOCKET,SO_REUSEADDR|SO_REUSEPORT,&opt,sizeof(opt))){
		cout<<"setsockopt"<<endl;
		exit(EXIT_FAILURE);
	}

	//In (struct sockaddr*)&serv_addr, sockaddr_in doesn't work!
	if(connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr))<0){
		perror("Connect failed:chunkDownloadFunc");
		pthread_exit(NULL);
	}
	cout<<"Connect successful:chunkDownloadFunc "<<endl;

	recv(sockfd,&ack, sizeof(ack),0);//to sync

	// cout<<"sync!"<<endl;

	char *msg=REQ_FOR_DATA;
	send(sockfd,msg,1028,0);//using sizeof(msg) instead of 1028 didn't send proper msg
	cout<<"sent the msg:Client:"<<msg<<endl;

	recv(sockfd,&ack, sizeof(ack),0);//to sync
	// cout<<"sync"<<endl;


	int file_size, temp_file_size, n, startPosInFile;

	//sending to server the no. of chunks needed from it
	int chunkCount=chunksToTake.size();
	send(sockfd, &chunkCount, sizeof(chunkCount),0);
	cout<<"chunkCount sent to server is:"<<chunkCount<<endl;

	recv(sockfd,&ack,sizeof(ack),0);//to sync
	// cout<<"sync!"<<endl;

	//Send filename to server(the server will read from this file and send data to client)
	send(sockfd, (char*)sourcefilenm.c_str(), sourcefilenm.length(),0);
	cout<<"sourcefilenm sent to server:"<<sourcefilenm<<endl;

	
	// cout<<"Sync!!"<<endl;

	// pthread_mutex_lock(&mlock);

	for(int cn=0; cn<chunksToTake.size();cn++){
		cout<<"entered chunk ka for loop"<<endl;
		int chunk_no=chunksToTake[cn];
		startPosInFile=CHUNK*(chunk_no-1);
		recv(sockfd,&ack,sizeof(ack),0);//to sync
		if(send(sockfd, &chunk_no, sizeof(chunk_no), 0)<0){
			perror("Error in sending chunk no.: fileDownloadFunc");
			pthread_exit(NULL);
		}
		cout<<"Sent chunkNo. to server:"<<chunk_no<<endl;

		int my_buff_size=BUFF_SIZE;
		char buffer[BUFF_SIZE]={'\0'};

		fseek(fp, startPosInFile, SEEK_SET);
		cout<<"Downloading chunk no.:chunkDownloadFunc: "<<chunk_no<<endl;
		cout<<"Start pos for chunk : chunkDownloadFunc : "<<startPosInFile<<endl;
		

		temp_file_size=CHUNK;//reading a CHUNK at a time
		while (temp_file_size > 0){
			n = recv(sockfd , buffer , my_buff_size, 0);
			if(n==0){
				cout<<"Received 0 byte count; So am breaking"<<endl;
				break;
			}
			cout <<"recieved byte count: " << n << endl;
			if( n < 0){
				perror("error in receiving chunk:chunkDownloadFunc");
				pthread_exit(NULL);
			}
			// cout<<"n : fileDownloadFunc: "<<n<<endl;
			cout<<"Buff peer1:fileDownloadFunc: "<<buffer<<endl;
			fwrite (buffer , sizeof(char), n, fp);
			// write(file_desc, buffer, n);
			memset ( buffer , '\0', my_buff_size);
			cout<<"tempfilesize"<<temp_file_size<<" "<<"n "<<n<<endl;
			temp_file_size = temp_file_size - n;
			cout<<"tempfilesize"<<temp_file_size<<endl;
			send(sockfd,&ack,ack,0);//to sync
			
			cout<<"BLAH"<<endl;
		}
		cout<<"1 chunk bheja!"<<endl;
		// fseek(fp, 0, SEEK_SET);
		// send(sockfd,&ack,ack,0);
		// cout<<"sync!!"<<endl;

		seederFileChunkMap[sha].push_back(chunk_no);
		if(seederFileChunkMap.find(sha)==seederFileChunkMap.end()){
			canStartUpload=1;
		}
	}
	fclose(fp);
	close(sockfd);
	cout<<"Out of for(); Done with all chunks"<<endl;
	
	// pthread_mutex_unlock(&mlock);

	cout<<"Exiting chunk download funct"<<endl;
	pthread_exit(NULL);
	cout<<"Will this even print?"<<endl;
	
}

void connectToSeedersForData(vector<pair<string,int> >& seederVec, vector<vector<int> >& pieceSelectVec, string dest, string sourceFile, string finalSHA){
	int seederVecSize=seederVec.size();
	pthread_t clientDownl[seederVecSize];
	int ack=1;
	FILE *fp = fopen((char*)dest.c_str() , "r+");

	// pthread_mutex_init(&mlock, NULL);
	for(int i=0;i<seederVecSize;i++){
		struct_tDownlData *dData=new struct_tDownlData;
		dData->ip=seederVec[i].first;
		dData->portNo=seederVec[i].second;
		dData->fp=fp;
		dData->filenm=sourceFile;
		dData->chunksToDownload=pieceSelectVec[i];
		dData->sha=finalSHA;

		cout<<"Thread for server "<<i+1<<" created"<<endl;
		pthread_create(&clientDownl[i],NULL,chunkDownloadFunc, (void*)dData);

	}
	cout<<"Have created all threads for chunk downl"<<endl;

	int i=0;
	while(i<seederVecSize){
		pthread_join(clientDownl[i], NULL);
		cout<<"Thread joining..."<<endl;
		i++;
	}
	// pthread_mutex_destroy(&mlock);

	cout<<"All threads have come back"<<endl;

	// cout<<"File received!"<<endl;
	
	fclose(fp);

}



int main(){

	pthread_t serverThread, clientThread;
	struct_portNums portDet;
	// char *msgServer="In server thread";
	// char *msgClient="In client thread";

	cout<<"Enter Server Port"<<endl;
	cin>>portDet.serverPortNo;

	cout<<"Enter Tracker Port"<<endl;
	cin>>portDet.trackerPortNo;

	int serv_thread_status=pthread_create(&serverThread, NULL, serverThreadFunc, (void*)&portDet);
	int client_thread_status=pthread_create(&clientThread, NULL, clientThreadFunc, (void*)&portDet);
	

	pthread_join(serverThread, NULL);
	pthread_join(clientThread, NULL);

	cout<<"Server thread is back"<<endl;
	cout<<"Client thread is back"<<endl;
	exit(0);
}

void *clientThreadFunc(void *ptr){
	// cout<<(char*)ptr<<endl;
	int opt=1;
	struct_portNums *clientData=(struct_portNums*)ptr;
	int portNumofTracker=clientData->trackerPortNo;
	int portNumofServer=clientData->serverPortNo;
	int sockfd=socket(AF_INET, SOCK_STREAM, 0);
	if(sockfd<0){
		perror("Socket creation failed : client");
		pthread_exit(NULL);
	}
	cout<<"Socket created : client"<<endl;

	struct sockaddr_in serv_addr_forTrackerConnect, serv_addr_forClient;

	memset(&serv_addr_forTrackerConnect,'\0',sizeof(serv_addr_forTrackerConnect));
	memset(&serv_addr_forClient,'\0',sizeof(serv_addr_forClient));

	serv_addr_forTrackerConnect.sin_family = AF_INET;
	serv_addr_forTrackerConnect.sin_port = htons(portNumofTracker);
	serv_addr_forTrackerConnect.sin_addr.s_addr=inet_addr("127.0.0.1");

	serv_addr_forClient.sin_family = AF_INET;
	serv_addr_forClient.sin_port = htons(portNumofServer);
	serv_addr_forClient.sin_addr.s_addr=inet_addr("127.0.0.1");

	if(setsockopt(sockfd, SOL_SOCKET,SO_REUSEADDR|SO_REUSEPORT,&opt,sizeof(opt))){
		cout<<"setsockopt"<<endl;
		exit(EXIT_FAILURE);
	}

	//Binding the peer's Client too....with the same portNo. and IP as the peer's Server
	int bindStatus = bind(sockfd,(struct sockaddr*)&serv_addr_forClient,sizeof(serv_addr_forClient));
	
	if(bindStatus < 0){
		cout<<"Error in binding:Client"<<endl;
		pthread_exit(NULL);
	}
	cout<<"Bind successful:Client"<<endl;
	// cout<<"Enter no."<<endl;
	// int num;
	// cin>>num;

	//In (struct sockaddr*)&serv_addr, sockaddr_in doesn't work!
	if(connect(sockfd, (struct sockaddr*)&serv_addr_forTrackerConnect, sizeof(serv_addr_forTrackerConnect))<0){
		perror("Connect failed : Client");
		pthread_exit(NULL);
	}
	cout<<"Connect successful : Client "<<endl;
	vector<string> cmdVec;
	cin.ignore();
	string USERID="";
	// int ackk=3;
	int ack, ack1;
	while(true){
		string cmd, cmd1;
		
		cout<<"Enter command"<<endl;
		getline(cin, cmd);
		
		cmd1=cmd;
		char *cmdName=strtok((char*)cmd1.c_str()," ");

		cout<<"Command entered:Client:"<<cmd<<endl;
		
		// cout<<"Commd name : Client: "<<cmdName<<endl;
		// cout<<"Commd name1 : Client: "<<cmd1<<endl;
		cmdVec.clear();
		cout<<endl;

		//**********************************************************************************//
		// if(cmdName==NULL){
		// 	pthread_exit(NULL);
		// }
		if(strcmp(cmdName, "create_user")==0){
			cout<<"Inside create_user:Client"<<endl;
			while((cmdName=strtok(NULL," "))!=NULL){
				// cout<<"whaaiiil"<<cmdName<<endl;
				cmdVec.push_back(cmdName);
				cout<<cmdName<<endl;
			}
			// cout<<"outside vec while"<<endl;
			if(cmdVec.size()!=2){
				cout<<"Wrong command format; See: create_user​ <user_id> <passwd>"<<endl;
				// pthread_exit(NULL);
				continue;
			}
			cout<<"Entered create_user:Client: Cmd being sent="<<cmd<<endl;
			if(send(sockfd,(char*)cmd.c_str(),cmd.length(),0)<0){
				cout<<"unsuccessful create_user cmd send:Client"<<endl;
			}
			//Receive acknowledgement
			recv(sockfd, &ack, sizeof(ack),0);
			if(ack==-1){
				cout<<"Err: User already exists!! Try again"<<endl;
			}
			else{
				cout<<"Created user!: Client: "<<ack<<endl;
			}

		}
		//**********************************************************************************//
		else if(strcmp(cmdName, "login")==0){
			cout<<"Inside login:Client"<<endl;
			if(USERID!=""){//a user is already logged in
				cout<<"SORRY! Another user already logged in"<<endl;
				continue;
				//TODO: set USERID as NULL at time of logout
			}
			while((cmdName=strtok(NULL," "))!=NULL){
				// cout<<"whaaiiil"<<cmdName<<endl;
				cmdVec.push_back(cmdName);
				cout<<cmdName<<endl;
			}
			// cout<<"outside vec while"<<endl;
			if(cmdVec.size()!=2){
				cout<<"Wrong command format; See: login <user_id> <passwd>"<<endl;
				// pthread_exit(NULL);
				continue;
			}
			cout<<"Entered login:Client: Cmd being sent="<<cmd<<endl;
			if(send(sockfd,(char*)cmd.c_str(),cmd.length(),0)<0){
				cout<<"unsuccessful login cmd send:Client"<<endl;
			}
			//Receive acknowledgement
			recv(sockfd, &ack, sizeof(ack),0);
			if(ack==-1){
				cout<<"Either user doesnt exist or wrong password sent!!Try again"<<endl;
				USERID="";
				// continue;
			}
			else{
				cout<<"Logged in!: Client: "<<ack<<endl;
				USERID=cmdVec[0];
			}
			
		}
		//**********************************************************************************//
		else if(strcmp(cmdName, "create_group")==0){
			cmdVec.push_back(cmdName);//Need to push_back "create_group" too in the vector so as to make the whole command
			cout<<"Inside create_group:CLient"<<endl;
			while((cmdName=strtok(NULL," "))!=NULL){
				// cmdName=strtok(NULL," ");
				cmdVec.push_back(cmdName);
			}
			if(cmdVec.size()!=2){
				cout<<"Wrong command format; See: create_group <group_id>"<<endl;
				// pthread_exit(NULL);
				continue;
			}
			cout<<"Entered create_group: Client"<<endl;
			string grpID=cmdVec[1];
			
			// if(USERID==""){
			// 	cout<<"User not logged in correctly"<<endl;
			// 	continue;
			// }
			if(USERID==""){
				cout<<"No User logged in yet! Users need to login before they can create groups!!"<<endl;
				continue;
			}
			string cmdToSend=cmdVec[0]+" "+grpID+" "+USERID;
			cout<<"Sending this cmd to tracker for create_group: "<<cmdToSend<<endl;

			if(send(sockfd,cmdToSend.c_str(),cmdToSend.length(),0)<0){
				cout<<"Unable to send cmd(in create_group):Client"<<endl;
			}
			//Receive acknowledgement
			recv(sockfd, &ack, sizeof(ack),0);
			if(ack==-1){
				cout<<"Unsuccessful create_group- Already exists!!Try again"<<endl;
			}
			else if(ack==NO_USER){
				cout<<"No User yet! Users need to login before they can create groups!!"<<endl;
			}
			else if(ack==NOT_LOGGED_IN){
				cout<<"User not logged in properly!!"<<endl;
			}
			else{
				cout<<"Group Created!:Client: "<<ack<<endl;
			}
			
		}
		//**********************************************************************************//
		else if(strcmp(cmdName, "join_group")==0){
			cmdVec.push_back(cmdName);//Need to push_back "join_group" too in the vector so as to make the whole command
			cout<<"Inside join_group:CLient"<<endl;
			while((cmdName=strtok(NULL," "))!=NULL){
				// cmdName=strtok(NULL," ");
				cmdVec.push_back(cmdName);
			}
			if(cmdVec.size()!=2){
				cout<<"Wrong command format; See: join_group <group_id>"<<endl;
				// pthread_exit(NULL);
				continue;
			}
			cout<<"Entered join_group: Client"<<endl;
			string grpID=cmdVec[1];

			if(USERID==""){
				cout<<"No User logged in yet! Users need to login before they can join/create groups!!"<<endl;
				continue;
			}
			
			string cmdToSend=cmdVec[0]+" "+grpID+" "+USERID;
			cout<<"Sending this cmd to tracker for join_group: "<<cmdToSend<<endl;

			if(send(sockfd,cmdToSend.c_str(),cmdToSend.length(),0)<0){
				cout<<"Unable to send cmd(in join_group):Client"<<endl;
			}
			//Receive acknowledgement
			recv(sockfd, &ack, sizeof(ack),0);
			if(ack==-1){
				cout<<"Unsuccessful join_group!!Try again"<<endl;
			}
			else if(ack==NOT_LOGGED_IN){
				cout<<"User not logged in properly!!"<<endl;
			}
			else if(ack==IN_LIST){
				cout<<"User already in Pending Requests' List!!"<<endl;
			}
			else{
				cout<<"Group Joined!:Client: "<<ack<<endl;
			}
			
		}
		//**********************************************************************************//
		else if(strcmp(cmdName, "leave_group")==0){
			cmdVec.push_back(cmdName);//Need to push_back "leave_group" too in the vector so as to make the whole command
			cout<<"Inside leave_group:CLient"<<endl;
			while((cmdName=strtok(NULL," "))!=NULL){
				// cmdName=strtok(NULL," ");
				cmdVec.push_back(cmdName);
			}
			if(cmdVec.size()!=2){
				cout<<"Wrong command format; See: leave_group <group_id>"<<endl;
				// pthread_exit(NULL);
				continue;
			}
			cout<<"Entered leave_group: Client"<<endl;
			string grpID=cmdVec[1];

			if(USERID==""){
				cout<<"No User logged in yet! Invalid leave_group request!!"<<endl;
				continue;
			}

			string cmdToSend=cmdVec[0]+" "+grpID+" "+USERID;
			cout<<"Sending this cmd to tracker for leave_group: "<<cmdToSend<<endl;

			if(send(sockfd,cmdToSend.c_str(),cmdToSend.length(),0)<0){
				cout<<"Unable to send cmd(in leave_group):Client"<<endl;
			}
			//Receive acknowledgement
			recv(sockfd, &ack, sizeof(ack),0);
			if(ack==-1){
				cout<<"No such group!!Try again"<<endl;
			}
			else if(ack==NOT_LOGGED_IN){
				cout<<"User not logged in properly!!"<<endl;
			}
			else if(ack==NOT_IN_GROUP){
				cout<<"User not a member of the group!!"<<endl;
			}
			else if(ack==IS_OWNER){
				cout<<"User is the owner of the group; He can't leave it!!"<<endl;
			}
			else{
				cout<<"Left group!:Client:"<<ack<<endl;
			}
			
		}
		//**********************************************************************************//
		else if(strcmp(cmdName, "list_requests")==0){
			cmdVec.push_back(cmdName);//Need to push_back "list_requests" too in the vector so as to make the whole command
			cout<<"Inside list_requests:CLient"<<endl;
			while((cmdName=strtok(NULL," "))!=NULL){
				// cmdName=strtok(NULL," ");
				cmdVec.push_back(cmdName);
			}
			if(cmdVec.size()!=2){
				cout<<"Wrong command format; See: list_requests <group_id>"<<endl;
				// pthread_exit(NULL);
				continue;
			}
			cout<<"Entered list_requests: Client"<<endl;
			string grpID=cmdVec[1];

			if(USERID==""){
				cout<<"No User logged in yet! Can't list pending join requests!!"<<endl;
				continue;
			}

			string cmdToSend=cmdVec[0]+" "+grpID+" "+USERID;
			cout<<"Sending this cmd to tracker for list_requests: "<<cmdToSend<<endl;

			if(send(sockfd,cmdToSend.c_str(),cmdToSend.length(),0)<0){
				cout<<"Unable to send cmd(in list_requests):Client"<<endl;
			}
			// cout<<"...OK..."<<endl;
			int szeOfList;
			//Receive size of listOfPendingReq from tracker
			recv(sockfd, &szeOfList, sizeof(szeOfList),0);
			// cout<<"Rcvd1"<<endl;
			cout<<"Rcvd list size:Client:"<<szeOfList<<endl;

			//List size rcvd acknowledgement
			send(sockfd, &ack, sizeof(ack), 0);
			// cout<<"Send ack1"<<endl;

			char listele[2048];
			cout<<"The pending requests are:"<<endl;
			//If recvd szeOfList==0, won't enter for()
			for(int i=0;i<szeOfList;i++){
				// if(i==0) cout<<"..rcving list elems..."<<endl;
				recv(sockfd, listele, sizeof(listele),0);
				cout<<"Rcvd2"<<endl;
				send(sockfd, &ack, sizeof(ack), 0);
				cout<<"Send2"<<endl;
				cout<<listele<<" ";
			}
			cout<<endl;

			//Receive acknowledgement from tracker
			recv(sockfd, &ack, sizeof(ack),0);
			cout<<"Rcvd ack3"<<endl;
			if(ack==-1){
				cout<<"No such group!!Try again"<<endl;
			}
			else if(ack==NOT_LOGGED_IN){
				cout<<"User not logged in properly!!"<<endl;
			}
			else if(ack==NOT_OWNER){
				cout<<"User not the OWNER! only owner can list requests!!"<<endl;
			}
			else{
				cout<<"Successfully listed pending requests!:Client:"<<ack<<endl;
			}
			
		}
		//**********************************************************************************//
		else if(strcmp(cmdName, "accept_request")==0){
			cmdVec.push_back(cmdName);//Need to push_back "accept_request" too in the vector so as to make the whole command
			cout<<"Inside accept_request:Client"<<endl;
			while((cmdName=strtok(NULL," "))!=NULL){
				// cmdName=strtok(NULL," ");
				cmdVec.push_back(cmdName);
			}
			if(cmdVec.size()!=3){
				cout<<"Wrong command format; See: accept_request <group_id> <userid>"<<endl;
				// pthread_exit(NULL);
				continue;
			}
			cout<<"Entered accept_request: Client"<<endl;
			string grpID=cmdVec[1];
			string userId=cmdVec[2];
			if(USERID==""){
				cout<<"User not logged in yet! Needs to log in before accept_request!!"<<endl;
				continue;
			}
			string cmdToSend=cmdVec[0]+" "+grpID+" "+userId;
			cout<<"Sending this cmd to tracker for accept_request: "<<cmdToSend<<endl;

			if(send(sockfd,cmdToSend.c_str(),cmdToSend.length(),0)<0){
				cout<<"Unable to send cmd(in accept_request):Client"<<endl;
			}
			//Receive acknowledgement from tracker
			recv(sockfd, &ack, sizeof(ack),0);
			// cout<<"Rcvd ack3"<<endl;
			if(ack==-1){
				cout<<"No such group!!Try again"<<endl;
			}
			else if(ack==NOT_PENDING_JOIN){
				cout<<"User not in pending join request"<<endl;
			}
			// else if(ack==NOT_LOGGED_IN){
			// 	cout<<"User not logged in properly!!"<<endl;
			// }
			else{
				cout<<"Successfully accepted pending join request!:Client:"<<ack<<endl;
			}
			
		}
		//**********************************************************************************//
		else if(strcmp(cmdName, "list_groups")==0){
			// cmdVec.push_back(cmdName);//Need to push_back "list_groups" too in the vector so as to make the whole command
			cout<<"Inside list_groups:Client"<<endl;
			cout<<"Entered list_groups: Client"<<endl;

			if(USERID==""){
				cout<<"No Users logged in yet! Can't list groups!!"<<endl;
				continue;
			}
			string cmdToSend=cmdName;
			cout<<"Sending this cmd to tracker for list_groups: "<<cmdToSend<<endl;
			if(send(sockfd,cmdToSend.c_str(),cmdToSend.length(),0)<0){
				cout<<"Unable to send cmd(in list_groups):Client"<<endl;
			}
			int szeOfMap;
			//Receive size of map from tracker
			recv(sockfd, &szeOfMap, sizeof(szeOfMap),0);
			// cout<<"Rcvd1"<<endl;
			cout<<"Rcvd map size:Client:"<<szeOfMap<<endl;

			//Map size rcvd acknowledgement
			send(sockfd, &ack, sizeof(ack), 0);
			// cout<<"Send ack1"<<endl;

			char listele[2048];
			cout<<"The groups are:"<<endl;
			for(int i=0;i<szeOfMap;i++){
				// if(i==0) cout<<"..rcving list elems..."<<endl;
				recv(sockfd, listele, sizeof(listele),0);
				// cout<<"Rcvd2"<<endl;
				send(sockfd, &ack, sizeof(ack), 0);
				// cout<<"Send2"<<endl;
				cout<<listele<<" ";
			}
			cout<<endl;

			//Receive acknowledgement from tracker
			recv(sockfd, &ack, sizeof(ack),0);
			// cout<<"Rcvd ack3"<<endl;
			cout<<"Successfully listed the groups!:Client:"<<ack<<endl;	
		}

		//**********************************************************************************//
		else if(strcmp(cmdName, "list_files")==0){
			cmdVec.push_back(cmdName);//Need to push_back "list_files" too in the vector so as to make the whole command
			cout<<"Inside list_files:Client"<<endl;
			while((cmdName=strtok(NULL," "))!=NULL){
				// cmdName=strtok(NULL," ");
				cmdVec.push_back(cmdName);
			}
			if(cmdVec.size()!=2){
				cout<<"Wrong command format; See: list_files​ <group_id>"<<endl;
				continue;
			}
			cout<<"Entered list_files:Client"<<endl;
			string grpID=cmdVec[1];

			if(USERID==""){
				cout<<"No Users logged in yet! Can't list files!!"<<endl;
				continue;
			}
			string cmdToSend=cmdVec[0]+" "+grpID+" "+USERID;
			cout<<"Sending this cmd to tracker for list_files: "<<cmdToSend<<endl;
			if(send(sockfd,cmdToSend.c_str(),cmdToSend.length(),0)<0){
				cout<<"Unable to send cmd(in list_files):Client"<<endl;
			}
			int szeOfList;
			//Receive size of map from tracker
			recv(sockfd, &szeOfList, sizeof(szeOfList),0);
			// cout<<"Rcvd1"<<endl;
			cout<<"Rcvd map size:Client:"<<szeOfList<<endl;

			//Map size rcvd acknowledgement
			send(sockfd, &ack, sizeof(ack), 0);
			// cout<<"Send ack1"<<endl;

			char listele[2048];
			string strr;
			cout<<"The shareable files in group "<<grpID<<" are:"<<endl;
			for(int i=0;i<szeOfList;i++){
				// if(i==0) cout<<"..rcving list elems..."<<endl;
				recv(sockfd, listele, sizeof(listele),0);
				// cout<<"Rcvd2"<<endl;
				send(sockfd, &ack, sizeof(ack), 0);
				// cout<<"Send2"<<endl;
				strr=listele;
				if(strr!="Do0Not1Include2"){
					cout<<strr<<" ";
				}
				
			}
			cout<<endl;

			//Receive acknowledgement from tracker
			recv(sockfd, &ack, sizeof(ack),0);
			// cout<<"Rcvd ack3"<<endl;
			if(ack==-1){
				cout<<"The group doesn't exist!:Client:"<<ack<<endl;
			}
			else if(ack==NOT_IN_GROUP){
				cout<<"The user is not a part of the group! Can't list files!!"<<ack<<endl;
			}
			else{
				cout<<"Successfully listed the shareable files!:Client:"<<ack<<endl;	
			}			
		}

		//**********************************************************************************//
		//Upload file
		else if(strcmp(cmdName, "upload_file")==0){
			cmdVec.push_back(cmdName);//Need to push_back "upload_file" too in the vector so as to make the whole command
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
			if(USERID==""){
				cout<<"No Users logged in yet! Need to login before upload_files!!"<<endl;
				continue;
			}
			string fileName=cmdVec[1];
			string grpID=cmdVec[2];
			FILE *fp=fopen((char*)fileName.c_str(), "rb");

			/*File size:*/
			struct stat st;
			stat((char*)fileName.c_str(), &st);
			int file_size = st.st_size;

			//updating seederFileChunkMap for the file
			//(assuming the client has complete file when he explicitly runs the upload_file req)
			int noOfChunksInFile=(int)file_size/CHUNK;
			if(file_size%CHUNK!=0){
				noOfChunksInFile++;
			}
			for(int i=1;i<=noOfChunksInFile;i++){
				seederFileChunkMap[fileName].push_back(i);
			}
			////////////////////////////////////////////////////

			string str_file_size=to_string(file_size);
			// cout<<"File size : ServReq : "<<file_size<<endl;
			string fileSHA=sha256_of_chunks(fp, file_size);
			cout<<"SHA as calculated:Client:"<<fileSHA<<endl;
			cout<<"Calculated SHA length:Client:"<<fileSHA.length()<<endl;
			if(USERID==""){
				cout<<"No Users logged in yet! Need to login before upload_files!!"<<endl;
				continue;
			}
			string cmdToSend=cmdVec[0]+" "+fileName+" "+str_file_size+" "+grpID+" "+USERID;
			//Not sending SHA here
			cout<<"Sending this cmd to tracker for upload_file: "<<cmdToSend<<endl;


			if(send(sockfd,(char*)cmdToSend.c_str(),cmdToSend.length(),0)<0){
				cout<<"Unable to send cmd(in upload_file):Client"<<endl;
			}
			//Receive acknowledgement
			recv(sockfd, &ack, sizeof(ack),0);
			cout<<"sync1"<<endl;
			cout<<"Ack received for sending cmd in upload_file: Client: "<<ack<<endl;

			send(sockfd, &ack, sizeof(ack),0);//to sync
			cout<<"sync2"<<endl;
			recv(sockfd, &ack, sizeof(ack),0);//check if user in group or not
			cout<<"sync3"<<endl;
			if(ack==NOT_IN_GROUP){
				cout<<"Sorry isnt in the group! Join the group to upload_file!!"<<endl;
				continue;
			}
			else if(ack==IN_GROUP){
				int lenSHA=fileSHA.length();
				int cnt=0;
				cout<<"Going to start sending SHA from Client"<<endl;
				while(cnt<lenSHA){
					string sha_20=fileSHA.substr(cnt,20);
					cnt+=20;
					cout<<"SHA sent:Client: "<<sha_20<<endl;
					send(sockfd, (char*)sha_20.c_str(), sha_20.length(), 0);
					cout<<"sync4"<<endl;
					recv(sockfd, &ack, sizeof(ack),0);
					cout<<"sync5"<<endl;

				}
				char *msg="endofSHA";
				send(sockfd, msg, sizeof(msg), 0);
				cout<<"sync4"<<endl;

				int nu=recv(sockfd, &ack, sizeof(ack),0);//work done
				cout<<"sync6; ack: "<<ack<<endl;
				cout<<"upload_file last rec bytes: "<<nu<<endl;
			}

		}
		//**********************************************************************************//
		else if(strcmp(cmdName, "logout")==0){
			if(USERID==""){
				cout<<"No user Logged in! Can't logout!!"<<endl;
				continue;
			}			
			string cmdToSend=cmdName;
			cmdToSend+=" "+USERID;
			USERID="";
			if(send(sockfd,(char*)cmdToSend.c_str(),cmdToSend.length(),0)<0){
				cout<<"Unable to send cmd(in logout):Client"<<endl;
			}
			//Receive acknowledgement
			recv(sockfd, &ack, sizeof(ack),0);
			cout<<"sync1"<<endl;
			cout<<"Logged out successfully!"<<endl;

		}
		//**********************************************************************************//
		else if(strcmp(cmdName, "download_file")==0){
			cmdVec.push_back(cmdName);//Need to push_back "upload_file" too in the vector so as to make the whole command
			// cout<<"Inside download_file:Client"<<endl;
			while((cmdName=strtok(NULL," "))!=NULL){
				// cmdName=strtok(NULL," ");
				cmdVec.push_back(cmdName);
			}
			if(cmdVec.size()!=4){
				cout<<"Wrong command format; See: download_file​ <group_id> <file_name> <destination_path>"<<endl;
				// pthread_exit(NULL);
				continue;
			}
			// cout<<"Entered download_file: Client"<<endl;
			if(USERID==""){
				cout<<"No Users logged in yet! Need to login before download_file!!"<<endl;
				continue;
			}
			string grpID=cmdVec[1];
			string fileNm=cmdVec[2];
			string dest=cmdVec[3];

			string cmdToSend=cmdVec[0]+" "+grpID+" "+fileNm+" "+dest+" "+USERID;
			cout<<"Sending this cmd to tracker for download_file: "<<cmdToSend<<endl;

			if(send(sockfd,cmdToSend.c_str(),cmdToSend.length(),0)<0){
				cout<<"Unable to send cmd(in download_file):Client"<<endl;
			}

			recv(sockfd,&ack,sizeof(ack),0);//user in group or not
			if(ack==NOT_IN_GROUP){
				cout<<"User not in group! Can't download file!!"<<endl;
				continue;
			}

			//Receive size of org file from tracker
			int fileSzSource, noOfChunksInFile;
			recv(sockfd, &fileSzSource, sizeof(fileSzSource),0);
			cout<<"file size source "<<fileSzSource<<endl;

			send(sockfd,&ack,sizeof(ack),0);//to sync

			createEmptyFile(dest, fileSzSource);
			cout<<"Returned after creating empty file"<<endl;
			// exit(0);

			noOfChunksInFile=(int)fileSzSource/CHUNK;
			if(fileSzSource%CHUNK!=0){
				noOfChunksInFile++;
			}

			// cout<<"Starting to recieve SHA from tracker"<<endl;
			int n;
			char bufSHA[21]={'\0'};
			string finalSHA="";
			while(n=recv(sockfd, &bufSHA, sizeof(bufSHA), 0)>0){
				// cout<<"sync4"<<endl;
				if(strcmp(bufSHA,"endofSHA")==0){
					break;
				}
				cout<<"Recieved SHA: "<<bufSHA<<endl;
				finalSHA+=bufSHA;
				send(sockfd, &ack, sizeof(ack), 0);
				// cout<<"sync5"<<endl;
				memset(bufSHA, '\0', sizeof(bufSHA));
			}
			// cout<<"Final SHA recvd:Tracker: "<<finalSHA<<endl;
			cout<<"Length of Final SHA recvd:Tracker: "<<finalSHA.length()<<endl;

			send(sockfd, &ack, sizeof(ack), 0);//to sync

			//Receiving seederList size
			int noOfSeeder;
			vector<pair<string,int> > seederVec;
			recv(sockfd, &noOfSeeder, sizeof(noOfSeeder), 0);
			send(sockfd, &ack, sizeof(ack), 0);
			char pIp[2048];
			string strr;
			int port; string ip;
			memset(pIp,'\0',2048);
			for(int i=0;i<noOfSeeder;i++){
				memset(pIp,'\0',2048);
				recv(sockfd,&pIp,sizeof(pIp),0);
				strr=pIp;
				cout<<"recvd pip: "<<pIp<<endl;
				send(sockfd, &ack, sizeof(ack), 0);//to sync
				if(strr!="Do0Not1Include2"){
					ip=strtok(pIp," ");
					// cout<<"strtok edits1:"<<pIp<<endl;
					string ppp=strtok(NULL," ");
					// cout<<"strtok edits2:"<<pIp<<endl;
					// cout<<"ppp: "<<ppp<<endl;
					port=stoi(ppp);
					cout<<"ip "<<ip<<" port "<<port<<endl;
					seederVec.push_back(make_pair(ip,port));
				}
				
			}

			//connecting to the respective servers
			int seederVecSize=seederVec.size();//=no. of seeders available to upload that file
			vector<vector<int> > seederChunkVec(seederVecSize);

			// cout<<"seederVecSize: "<<seederVecSize<<endl;
			// cout<<"Calling connectToSeedersForVec()"<<endl;

			connectToSeedersForVec(seederVec, seederChunkVec, fileNm);
			// cout<<"Have returned from connectToSeedersForVec()"<<endl;

			cout<<"Printing seederChunkVec so obtained:"<<endl;
			for(int i=0;i<seederVecSize;i++){
				cout<<"seeder no.:"<<i+1<<"with portNo.:"<<seederVec[i].second<<endl;
				cout<<"chunk nos with this seeder:"<<endl;
				for(auto &x : seederChunkVec[i]){
					cout<<x<<" ";
				}
				cout<<endl;
			}
			///////////////////////////////////////////

			//piece selection, now that we have info abt all the pieces and their location(Ip+port)
			// cout<<"Calling pieceSelection()"<<endl;

			vector<vector<int> > pieceSelectVec(seederVecSize);
			pieceSelection(seederChunkVec, pieceSelectVec, noOfChunksInFile);

			// cout<<"Have returned from pieceSelection()"<<endl;

			cout<<"Printing pieceSelectVec so obtained:"<<endl;
			for(int i=0;i<seederVecSize;i++){
				cout<<"seeder no.:"<<i+1<<"with portNo.:"<<seederVec[i].second<<endl;
				cout<<"chunk nos being selected from this seeder:"<<endl;
				for(auto &x : pieceSelectVec[i]){
					cout<<x<<" ";
				}
				cout<<endl;
			}
			//////////////////////////////////////////////

			//connecting to servers to download data
			// cout<<"Calling connectToSeedersForData()"<<endl;

			connectToSeedersForData(seederVec, pieceSelectVec, dest, fileNm, finalSHA);

			cout<<"Have returned from connectToSeedersForData()"<<endl;

			///////////////////////////////////////////////



		}

		else{//No command matches
			cout<<"ERR! Check the command you've entered"<<endl;
		}

	}
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

	struct_portNums *serverData=(struct_portNums*)ptr;
	int portNum=serverData->serverPortNo;
	int opt=1;
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

	if(setsockopt(server_socket_fd, SOL_SOCKET,SO_REUSEADDR|SO_REUSEPORT,&opt,sizeof(opt))){
		cout<<"setsockopt"<<endl;
		exit(EXIT_FAILURE);
	}	

	int bindStatus = bind(server_socket_fd,(struct sockaddr*)&server_addr,sizeof(server_addr));
	
	if(bindStatus < 0){
		cout<<"Error in binding:Server"<<endl;
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
			cout<<"Accept successful:Server "<<endl;
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
	close(server_socket_fd);
	// close(new_server_socket_fd);
}

void *serveRequest(void *ptr){
	cout<<"Inside serveRequest"<<endl;

	struct_tData *tstruct;
	tstruct=(struct_tData*)ptr;
	cout<<"Have typecasted the void* "<<endl;

	int ack=1;

	int sockfd=tstruct->sockfd;
	// cout<<"received fd in parameter is: "<<fd<<endl;

	send(sockfd,&ack, sizeof(ack),0);//to sync

	char msg[1028]={'\0'};

	recv(sockfd,&msg,sizeof(msg),0);
	string msg_str=msg;
	// cout<<"Recvd the msg:Server:"<<msg<<endl;
	// cout<<"Recvd the msg_str:Server:"<<msg_str<<endl;

	// cout<<"msg_str:"<<msg_str<<endl;
	// cout<<"REQ_FOR_VEC:"<<REQ_FOR_VEC<<endl;
	// cout<<"check if's halat: "<<(msg_str=="Request for Vector")<<endl;

	if(msg_str=="Request for Vector"){
		cout<<"It's a request for vector:Server"<<endl;
		send(sockfd,&ack, sizeof(ack),0);//to sync
		// cout<<"sync1"<<endl;

		//Recv filename from client
		char filename[1028]={'\0'};
		recv(sockfd,&filename, sizeof(filename),0);
		string filenm=filename;
		cout<<"filename recvd from client"<<filenm<<endl;

		//Sending vec size to client
		int chunkVecSize=seederFileChunkMap[filenm].size();
		send(sockfd, &chunkVecSize, sizeof(chunkVecSize),0);
		cout<<"Size of chunkVec that server is sending:"<<chunkVecSize<<endl;

		recv(sockfd,&ack, sizeof(ack),0);//to sync
		// cout<<"sync2"<<endl;

		//Start sending the chunkNos in chunkVector to clientcout<<"Sending chunkNo:"<<endl;
		cout<<"Sending chunkNo:"<<endl;
		for(int i=0;i<chunkVecSize;i++){
			int chunkNo=seederFileChunkMap[filenm][i];
			send(sockfd,&chunkNo,sizeof(chunkNo),0);
			cout<<chunkNo<<" ";
			recv(sockfd,&ack, sizeof(ack),0);//to sync
			// cout<<"sync3"<<endl;
		}
		cout<<endl;

	}
	else if(msg_str==REQ_FOR_DATA){
		cout<<"It's a request for data:Server"<<endl;
		send(sockfd,&ack, sizeof(ack),0);//to sync
		// cout<<"sync1"<<endl;

		//receive chunkNo from client
		int chunkCount;
		recv(sockfd,&chunkCount,sizeof(chunkCount),0);
		cout<<"server recvd chunkCOunt:"<<chunkCount<<endl;

		send(sockfd,&ack,sizeof(ack),0);//to sync
		// cout<<"ack!"<<endl;

		//Recv filename from client
		char filename[1028]={'\0'};
		recv(sockfd,&filename, 1028,0);
		string filenm=filename;
		cout<<"source filename recvd from client"<<filenm<<endl;

		
		// cout<<"ack!!"<<endl;
		cout<<"Going to open file in server"<<endl;
		FILE *fp=fopen(filenm.c_str(), "rb");
		cout<<"Sble to open file in server"<<endl;

		for(int cn=0; cn<chunkCount;cn++){
			int chunk_no;
			send(sockfd,&ack,sizeof(ack),0);//to sync
			if(recv(sockfd, &chunk_no, sizeof(chunk_no), 0)<0){
				perror("Error in receive chunk no.: ServReq");
				pthread_exit(NULL);
			}
			cout<<"Chunk no. recvd in ServReq "<<chunk_no<<endl;
			
			/*File size:*/
			struct stat st;
			stat(filenm.c_str(), &st);
			int file_size = st.st_size;
			cout<<"File size : ServReq : "<<file_size<<endl;

			int my_buff_size=BUFF_SIZE;
			if(file_size<BUFF_SIZE){
				my_buff_size=file_size;
			}
			else{
				my_buff_size=BUFF_SIZE;
			}
			char Buffer[my_buff_size];
			int n;

			fseek(fp,0,SEEK_SET);

			// if(CHUNK*(chunk_no-1)<=file_size){
				int startPosInFile=CHUNK*(chunk_no-1);
				fseek(fp, startPosInFile, SEEK_SET);
				cout<<"File ptr pos before : ServReq : "<<ftell(fp)<<endl;

				cout<<"startPosInFile:ServReq : "<<startPosInFile<<endl;
				// cout<<"SEEK_SET : ServReq : "<<SEEK_SET<<endl;


				int temp_file_size=CHUNK;

				// cout<<"my_buff_size : ServReq : "<<my_buff_size<<endl;
				// cout<<"temp_file_size : ServReq :"<<temp_file_size<<endl;
				n=0;
				while(temp_file_size>0){
					n=fread(Buffer, sizeof(char), my_buff_size, fp);
					cout<<"In buff:ServReq: "<<Buffer<<endl;
					if(n < 0) {
						cout << "extra read !!" << endl;
						break;
					}
					if(n==0){
						cout<<"Read 0 bytes; so am breaking outta the loop"<<endl;
						break;
					}
					
					if(send(sockfd, Buffer, n, 0)<0){
						perror("error in sending file : ServReq");
						pthread_exit(NULL);
					}
					
					memset(Buffer, '\0', my_buff_size);
					cout<<"n "<<n<<" tempfilesize "<<temp_file_size<<endl;
					temp_file_size=temp_file_size-n;
					cout<<"n "<<n<<" tempfilesize "<<temp_file_size<<endl;

					recv(sockfd,&ack,ack,0);//to sync
					cout<<"1 chunk mila!"<<endl;
				}


				cout<<"File ptr pos now : ServReq : "<<ftell(fp)<<endl;
			// }			

		}
		close(sockfd);	
		fclose(fp);
		cout<<"Out of for(); Done with all chunks"<<endl;
		
		pthread_exit(NULL);
		cout<<"Will this even print??"<<endl;


	}
}
