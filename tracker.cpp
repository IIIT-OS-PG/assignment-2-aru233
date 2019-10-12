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
#include <bits/stdc++.h> 

using namespace std;

#define CMD_LEN 20480 //20kb

struct threadData{
	int sockfd;
	int portNo;
	int chunkNo;
	FILE* fp;
	int fileSize;
};
typedef struct threadData struct_tData;

struct portIP{
	string portNo;
	string ip;
};
typedef struct portIP struct_portIP;

// struct clientDetail{
// 	int sockfd;
// 	struct server_addr serv_addr;
// };
// typedef struct clientDetail struct_clientDet;

struct userDetail{
	int sockfd;
	char *fileNm;
	string sha;
	int fileSz;
	char *gid;
	struct sockaddr_in serv_addr;
};
typedef struct userDetail struct_userDet;

void *serveReqAtTracker(void *ptr){
	cout<<"Inside serveReqAtTracker()"<<endl;
	struct_userDet *userDetail=(struct_userDet*)ptr;
	int sockfd=userDetail->sockfd;

	vector<string> cmdVec;
	while(1){
		char cmd[CMD_LEN]={'\0'};
		int cmdLngth=recv(sockfd, cmd, sizeof(cmd),0);
		cout<<"Command length:Tracker:"<<cmdLngth<<endl;
		char rcmd[CMD_LEN];
		strcpy(rcmd,cmd);
		cout<<"Command received from client: "<<rcmd<<endl;
		char *cmdName=strtok(rcmd," ");
		cmdVec.clear();
		int ack=1;
		if(strcmp(cmdName, "create_user")==0){
			cout<<"create_user case:Tracker"<<endl;	
			while((cmdName=strtok(NULL," "))!=NULL){
				// cmdName=strtok(NULL," ");
				cmdVec.push_back(cmdName);
			}
			// if(cmdVec.size()!=2){
			// 	cout<<"Wrong command format; See: create_user​ <user_id> <passwd>"<<endl;
			// 	pthread_exit(NULL);
			// }
			string userNm=cmdVec[0];
			string passw=cmdVec[1];
			cout<<"Recieved username and password: "<<userNm<<" "<<passw<<endl;

			send(sockfd, &ack, ack, 0);
			cout<<"tracker sent ack to client"<<endl;
		}
		else if(strcmp(cmdName, "upload_file")==0){
			cout<<"upload_file case:Tracker"<<endl;
			while((cmdName=strtok(NULL," "))!=NULL){
				// cmdName=strtok(NULL," ");
				cmdVec.push_back(cmdName);
			}
			string fileNm=cmdVec[0];
			string str_fileSz=cmdVec[1];
			string grpID=cmdVec[2];

			cout<<"Recieved fileNm, fileSz, grpID: "<<
			fileNm<<" "<< str_fileSz<<" " <<grpID<<endl;

			send(sockfd, &ack, ack, 0);
			cout<<"tracker sent ack to client"<<endl;

			cout<<"Starting to recieve SHA"<<endl;
			int n;
			char bufSHA[21]={'\0'};
			string finalSHA="";
			while(n=recv(sockfd, bufSHA, sizeof(bufSHA), 0)>0){
				if(strcmp(bufSHA,"endofSHA")==0){
					break;
				}
				cout<<"Recieved SHA: "<<bufSHA<<endl;
				finalSHA+=bufSHA;
				send(sockfd, &ack, sizeof(ack), 0);
				memset(bufSHA, '\0', sizeof(bufSHA));
			}
			cout<<"Final SHA recvd:Tracker: "<<finalSHA<<endl;
			cout<<"Length of Final SHA recvd:Tracker: "<<finalSHA.length()<<endl;

		}
	}//End of infinite while
	close(sockfd);

}

void *trackerThreadFunc(void *ptr){
	struct_portIP *port_ip=(struct_portIP*)ptr;
	string portNum=port_ip->portNo;
	string ip_tracker1=port_ip->ip;

	struct sockaddr_in server_addr;
	struct sockaddr_in new_serv_addr;
	int newsocket;

	int server_socket_fd, new_server_socket_fd;
	if((server_socket_fd=socket(AF_INET,SOCK_STREAM,0))<0){
		cout<<"Error in socket creation:Tracker1:"<<endl;
		pthread_exit(NULL);
	}
	cout<<"Socket created : Tracker1:"<<endl;

	memset(&server_addr,'\0',sizeof(server_addr));

	server_addr.sin_family=AF_INET;
	server_addr.sin_port=htons(stoi(portNum));
	// inet_pton(AF_INET, ip_tracker1.c_str(), &server_addr.sin_addr);
	server_addr.sin_addr.s_addr=inet_addr(ip_tracker1.c_str());
	//TODO : comp with inet_ntoa, inet_pton ???
	

	int bindStatus = bind(server_socket_fd,(struct sockaddr*)&server_addr,sizeof(server_addr));
	
	if(bindStatus < 0){
		cout<<"Error in binding"<<endl;
		pthread_exit(NULL);
	}
	cout<<"Bind successful : Tracker1"<<endl;

	int listencount=0;

	if(listen(server_socket_fd, 5)!=0){
		cout<<"Error in Listening:Tracker1:"<<stoi(portNum)<<endl;
		pthread_exit(NULL);
	}
	cout<<"Listening now : Tracker1: "<<portNum<<endl;

	int addrlen=sizeof(server_addr);
	pthread_t trackerReqThr[50];
	int i=0;
	while(1){
		if((new_server_socket_fd = accept(server_socket_fd,(struct sockaddr*)&new_serv_addr,(socklen_t*)&addrlen))<0){
		
		// if((new_server_socket_fd = accept(server_socket_fd,(struct sockaddr*)&new_serv_addr,(socklen_t*)&addrlen))<0){
			perror("Error in accept");
			pthread_exit(NULL);
		}
		else{
			cout<<"Accept successful : Tracker1 "<<endl;
			// pthread_t servetrackerReqthread;
			struct_userDet *cdata=(struct_userDet *)malloc(sizeof(struct_userDet));
			cdata->sockfd=new_server_socket_fd;
			cdata->serv_addr=new_serv_addr;
			//TODO : Is this correct?


			int serv_thread_status=pthread_create(&trackerReqThr[i], NULL, serveReqAtTracker, (void*)cdata);
			if(serv_thread_status<0){
				perror("Error in creating thread to serve request : Server");
				pthread_exit(NULL);
			}

			// pthread_detach(servethread);
			pthread_detach(trackerReqThr[i]);
			i++;
			
		}
	}
}


int main(int argc, char *argv[]){
	// int portNum;
	// cout<<"Enter Tracker Port No."<<endl;
	// cin>>portNum;


	if(argc!=3){
		cout<<"insufficient info"<<endl;
		//./tracker Tracker1IP Tracker1PORT
		exit(0);
	}

	struct_portIP port_ip;
	port_ip.ip=argv[1];
	port_ip.portNo=argv[2];

	pthread_t trackerThread;
	pthread_create(&trackerThread, NULL, trackerThreadFunc, (void*)&port_ip);
	// cout<<"tracker returned to main()"<<endl;

	string quitCmd;
	cin>>quitCmd;
	if(quitCmd=="quit"){
		pthread_cancel(trackerThread);
		return 0;
	}

	pthread_join(trackerThread, NULL);
	// int j=0;
	// while(j<i){
	// 	pthread_detach(clientDownl[j]);
	// 	j++;
	// }

	return 0;
}

