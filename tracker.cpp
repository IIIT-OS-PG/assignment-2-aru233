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
#define LOGGED_IN 3
#define IN_LIST 2
#define IN_GROUP 4
#define NOT_LOGGED_IN -2
#define NOT_IN_GROUP -3
#define IS_OWNER -4
#define NOT_PENDING_JOIN -5
#define NO_GROUPS -6
#define NO_USER -7

// struct threadData{
// 	int sockfd;
// 	int portNo;
// 	int chunkNo;
// 	FILE* fp;
// 	int fileSize;
// };
// typedef struct threadData struct_tData;

struct portIP{
	string portNo;
	string ip;
};
typedef struct portIP struct_portIP;

struct threadData{
	int sockfd;
	char *fileNm;
	string sha;
	int fileSz;
	char *gid;
	struct sockaddr_in client_addr;
};
typedef struct threadData struct_threadData;

struct filedetails{	
	int fileSz;
	string SHAvar;
	vector<string> listOfSeeders;//list of userIDs of users who're participating in the upload process
};
typedef struct filedetails fileDetail;

struct userdetails{
	string password;
	int portNo;
	string ip;
	int loggedIn;//1-> if user logged in/active; else 0
};
typedef struct userdetails userDetail;

struct groupdetails{
	vector<pair<string,int> > fileNm;//pair is of filename,shareable; shareable=1
	vector<string> listOfSeeders;
	string grpOwner;
	vector<string> listOfPendingReq;//list of userIDs of users with pending group join requests
};
typedef struct groupdetails groupDetail;

map<string,fileDetail*> fileMap;//key-> gid+filename
map<string,userDetail*> userMap;//key-> userId
map<string, groupDetail*> groupMap;//key-> gid

//TODO: are locks needed for global DS and this function???

int userLoggedIn(string userId){
	if(userId==""){
		return 0;
	}
	for(auto i : userMap){
		if(i.first==userId && (i.second)->loggedIn==1){
			return 1;
		}
	}
	return 0;
}

int inList(vector<string>& mlist, string uid){
	for(int i=0;i<mlist.size();i++){
		if(mlist[i]==uid){
			return i;
		}
	}
	return -1;
}

//for fileNm list of groupDetail struct, where each elem is a pair<string,int>
// int inFileList(vector<string>& filelist, string filenm){
// 	for(int i=0;i<filelist.size();i++){
// 		if(filelist[i].first==filenm){
// 			return i;
// 		}
// 	}
// 	return -1;
// }

int userInGroup(string userid, string gid){
	if(inList((groupMap[gid]->listOfSeeders),userid)!=-1){
		return 1;
	}
	return -1;
}

void *serveReqAtTracker(void *ptr){
	cout<<"Inside serveReqAtTracker()"<<endl;
	struct_threadData *thrdData=(struct_threadData*)ptr;
	int sockfd=thrdData->sockfd;
	// userDetail->client_addr.sin_port
	// struct sockaddr_in client_serv_addr;
	// memset(&client_serv_addr,'\0',sizeof(client_serv_addr));

	vector<string> cmdVec;
	int ack;
	while(1){
		char cmd[CMD_LEN]={'\0'};
		int cmdLngth=recv(sockfd, cmd, sizeof(cmd),0);		
		// char rcmd[CMD_LEN];
		// strcpy(rcmd,cmd);
		// char *cmdName=strtok(rcmd," ");
		char *cmdName=strtok(cmd," ");
		if(cmdName==NULL){
			pthread_exit(NULL);
		}
		// cout<<"Command received from client: "<<cmd<<endl;
		// cout<<"Command length:Tracker:"<<cmdLngth<<endl;
		cmdVec.clear();
		cout<<endl;
		//**********************************************************************************//
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
			string userId=cmdVec[0];
			string passw=cmdVec[1];
			cout<<"Recieved userID and password: "<<userId<<" "<<passw<<endl;

			if(userMap.find(userId)!=userMap.end()){
				cout<<"Err: User already exists!!"<<endl;
				ack=-1;
				send(sockfd, &ack, sizeof(ack), 0);
				cout<<"tracker sent -ve ack to client"<<endl;
				continue;
			}
			// userDetail* userDet=(userDetail*)malloc(sizeof(userDetail));
			userDetail* userDet=new userDetail;
			userDet->password=passw;
			userDet->loggedIn=0;
			userDet->portNo=ntohs((thrdData->client_addr).sin_port);
			char *ipp=new char[1000];
			inet_ntop(AF_INET,&((thrdData->client_addr).sin_addr),ipp,1000);
			userDet->ip=ipp;
			userMap[userId]=userDet;

			cout<<"Details entered in userMap:"<<endl;
			cout<<userId<<" "<<userDet->password<<" "<<userDet->portNo<<" "<<userDet->ip<<" "<<userDet->loggedIn<<endl;

			ack=1;
			send(sockfd, &ack, sizeof(ack), 0);
			cout<<"tracker sent ack to client"<<endl;
		}

		//**********************************************************************************//
		else if(strcmp(cmdName, "login")==0){
			cout<<"login case:Tracker"<<endl;	
			while((cmdName=strtok(NULL," "))!=NULL){
				// cmdName=strtok(NULL," ");
				cmdVec.push_back(cmdName);
			}
			string userId=cmdVec[0];
			string passw=cmdVec[1];
			cout<<"Recieved userID and password: "<<userId<<" "<<passw<<endl;

			//TODO: if an already logged in user tries to log in agains --- check if loggedin flag already set?
			if(userMap.find(userId)!=userMap.end()){
				if(userMap[userId]->password==passw){
					userMap[userId]->loggedIn=1;//User is active now
					userMap[userId]->portNo=ntohs((thrdData->client_addr).sin_port);//Updating portNo(client may log back in with a new ip+port)
					char *ipp=new char[1000];
					inet_ntop(AF_INET,&((thrdData->client_addr).sin_addr),ipp,1000);
					userMap[userId]->ip=ipp;
					cout<<"Logged in successfully!"<<endl;
					cout<<"Details after updation in userMap:"<<endl;
					cout<<userId<<" "<<userMap[userId]->password<<" "<<userMap[userId]->portNo<<" "<<userMap[userId]->ip<<" "<<userMap[userId]->loggedIn<<endl;

				}
				else{//password not authenticated
					cout<<"Err: Wrong password!!"<<endl;
					ack=-1;
					send(sockfd, &ack, sizeof(ack), 0);
					cout<<"tracker sent -ve ack to client"<<endl;
					continue;
				}				
			}
			else{//user doesnt exist
				cout<<"Err: User doesn't exit!!"<<endl;
				ack=-1;
				send(sockfd, &ack, sizeof(ack), 0);
				cout<<"tracker sent -ve ack to client"<<endl;
				continue;
			}	
			ack=1;		
			send(sockfd, &ack, sizeof(ack), 0);
			cout<<"tracker sent ack to client"<<endl;
		}

		//**********************************************************************************//
		//rcvd cmd-> create_group gid userid
		else if(strcmp(cmdName, "create_group")==0){
			int flag=0;
			cout<<"create_group case:Tracker"<<endl;
			while((cmdName=strtok(NULL," "))!=NULL){
				// cmdName=strtok(NULL," ");
				cmdVec.push_back(cmdName);
			}
			string grpId=cmdVec[0];
			string userId=cmdVec[1];
			if(userId==""){
				ack=NO_USER;
				send(sockfd, &ack, sizeof(ack), 0);
				continue;
			}
			if(!userLoggedIn(userId)){//User not logged in
				cout<<"Err:User not logged in properly!!"<<endl;
				ack=NOT_LOGGED_IN;
				send(sockfd, &ack, sizeof(ack), 0);
				cout<<"tracker sent -ve ack to client"<<endl;
				continue;
			}
			for(auto i : groupMap){
				if(i.first==grpId){//group already exists
					cout<<"Err:Group already exists!!"<<endl;
					ack=-1;
					send(sockfd, &ack, sizeof(ack), 0);
					cout<<"tracker sent -ve ack to client"<<endl;
					flag=1;
					break;					
				}
			}
			if(flag==1){
				continue;
			}
			// groupDetail* gpDet=(groupDetail*)malloc(sizeof(groupDetail));
			groupDetail* gpDet=new groupDetail;
			gpDet->grpOwner=userId;
			gpDet->listOfSeeders.push_back(userId);
			groupMap[grpId]=gpDet;//inserting new groupId as key and an empty struct as value
			cout<<"Group created successfully!"<<endl;
			// cout<<"Details in groupMap:"<<endl;
			// cout<<grpId<<" "<<fileMap[userId]->password<<" "<<fileMap[userId]->port<<" "<<fileMap[userId]->ip<<" "<<fileMap[userId]->loggedIn<<endl;

			ack=1;
			send(sockfd, &ack, sizeof(ack), 0);
			cout<<"tracker sent ack to client; ack:"<<ack<<endl;
		}

		//**********************************************************************************//
		//rcvd cmd-> join_group gid userid
		else if(strcmp(cmdName, "join_group")==0){
			int flag=0;
			cout<<"join_group case:Tracker"<<endl;
			while((cmdName=strtok(NULL," "))!=NULL){
				// cmdName=strtok(NULL," ");
				cmdVec.push_back(cmdName);
			}
			string grpId=cmdVec[0];
			string userId=cmdVec[1];
			if(!userLoggedIn(userId)){//User not logged in
				cout<<"Err:User not logged in properly!!"<<endl;
				ack=NOT_LOGGED_IN;
				send(sockfd, &ack, sizeof(ack), 0);
				cout<<"tracker sent ack to client"<<endl;
				continue;
			}
			for(auto i : groupMap){
				if(i.first==grpId){//group exists
					if(inList(groupMap[grpId]->listOfPendingReq, userId)==-1){//checking if user's not already in pending requests' list
						(groupMap[grpId]->listOfPendingReq).push_back(userId);
						cout<<"Join req processed..added to pending request queue"<<endl;
						ack=1;
					}
					else{
						cout<<"Already in pending request queue"<<endl;
						ack=IN_LIST;
					}
					
					send(sockfd, &ack, sizeof(ack), 0);
					cout<<"tracker sent ack to client"<<endl;
					flag=1;
					break;				
				}
			}
			if(flag==1){
				continue;
			}
			cout<<"Err:No such group exists!!"<<endl;//Group with given groupId doesn't exist
			ack=-1;
			send(sockfd, &ack, sizeof(ack), 0);
			cout<<"tracker sent ack to client"<<endl;
		}

		//**********************************************************************************//
		//rcvd cmd-> leave_group gid userid
		else if(strcmp(cmdName, "leave_group")==0){
			int flag=0;
			cout<<"leave_group case:Tracker"<<endl;
			while((cmdName=strtok(NULL," "))!=NULL){
				// cmdName=strtok(NULL," ");
				cmdVec.push_back(cmdName);
			}
			string grpId=cmdVec[0];
			string userId=cmdVec[1];
			if(!userLoggedIn(userId)){//User not logged in
				cout<<"Err:User not logged in properly!!"<<endl;
				ack=NOT_LOGGED_IN;
				send(sockfd, &ack, sizeof(ack), 0);
				cout<<"tracker sent ack to client"<<endl;
				continue;
			}
			int pos;
			for(auto i : groupMap){
				if(i.first==grpId){//group exists
					if((pos=inList(groupMap[grpId]->listOfSeeders, userId))!=-1){//checking if user is in the group (before removing it)
						if(groupMap[grpId]->grpOwner==userId){
							cout<<"User is an owner; can't leave group!!"<<endl;
							ack=IS_OWNER;
						}
						else{
							(groupMap[grpId]->listOfSeeders).erase((groupMap[grpId]->listOfSeeders).begin()+pos);
							cout<<"Removed userId from ListOfSeeders!"<<endl;
							//TODO: handle case when a user who's the sole owner of a file leaves the group, do we delete the filename from tracker?
							//TODO :removing user from listOfSeeders in fileMap
							ack=1;
						}						
					}
					else{
						cout<<"Err:User not in group yet"<<endl;
						ack=NOT_IN_GROUP;
					}
					
					send(sockfd, &ack, sizeof(ack), 0);
					cout<<"tracker sent ack to client"<<endl;
					flag=1;
					break;				
				}
			}
			if(flag==1){
				continue;
			}
			cout<<"Err:No such group exists!!"<<endl;//Group with given groupId doesn't exist
			ack=-1;
			send(sockfd, &ack, sizeof(ack), 0);
			cout<<"tracker sent ack to client"<<endl;
		}

		//**********************************************************************************//
		//rcvd cmd-> list_requests gid uid
		else if(strcmp(cmdName, "list_requests")==0){
			int flag=0;
			cout<<"list_requests case:Tracker"<<endl;
			while((cmdName=strtok(NULL," "))!=NULL){
				// cmdName=strtok(NULL," ");
				cmdVec.push_back(cmdName);
			}
			string grpId=cmdVec[0];
			string userId=cmdVec[1];
			if(!userLoggedIn(userId)){//User not logged in
				cout<<"Err:User not logged in properly!!"<<endl;
				ack=NOT_LOGGED_IN;
				send(sockfd, &ack, sizeof(ack), 0);
				cout<<"tracker sent ack to client"<<endl;
				continue;
			}
			int pos;
			for(auto i : groupMap){
				if(i.first==grpId){//group exists					
					int sze=(groupMap[grpId]->listOfPendingReq).size();
					cout<<"Sending the size of listOfPendingReq to client; size:"<<sze<<endl;
					send(sockfd, &sze, sizeof(sze), 0);
					cout<<"Send1"<<endl;

					//Receiving ack for listSize reception from client
					recv(sockfd, &ack, sizeof(ack),0);
					cout<<"Rcvd ack1"<<endl;

					cout<<"Sending the list of pending req to client"<<endl;
					for(int i=0;i<sze;i++){
						string listele=(groupMap[grpId]->listOfPendingReq)[i];
						cout<<listele<<" ";
						send(sockfd, (char*)listele.c_str(), listele.length(), 0);
						// cout<<"Send2"<<endl;
						recv(sockfd, &ack, sizeof(ack),0);
						// cout<<"Rcvd ack2"<<endl;
					}
					cout<<endl;
					cout<<"Sent the list to client"<<endl;
					ack=1;
					send(sockfd, &ack, sizeof(ack), 0);
					// cout<<"Send3"<<endl;
					cout<<"tracker sent ack to client; ack:"<<ack<<endl;
					flag=1;
					break;				
				}
			}
			if(flag==1){
				continue;
			}
			//If reqd group doesnt exist, then the tracker won't enter the block where it send size and recv ack.
			//But the client is waiting to recv size and will send ack. Hence we need to do this:
			int sze=0;
			send(sockfd, &sze, sizeof(sze), 0);
			cout<<"Sending the size of listOfPendingReq to client; size:"<<sze<<endl;
			
			// cout<<"Send1"<<endl;

			//Receiving ack for listSize reception from client
			recv(sockfd, &ack, sizeof(ack),0);

			cout<<"Err:No such group exists!!"<<endl;//Group with given groupId doesn't exist
			ack=-1;
			send(sockfd, &ack, sizeof(ack), 0);
			cout<<"tracker sent ack to client"<<endl;
		}

		//**********************************************************************************//
		//rcvd cmd-> accept_request gid uid
		else if(strcmp(cmdName, "accept_request")==0){
			int flag=0;
			cout<<"accept_request case:Tracker"<<endl;
			while((cmdName=strtok(NULL," "))!=NULL){
				// cmdName=strtok(NULL," ");
				cmdVec.push_back(cmdName);
			}
			string grpId=cmdVec[0];
			string userId=cmdVec[1];
			// if(!userLoggedIn(userId)){//User not logged in
			// 	cout<<"Err:User not logged in properly!!"<<endl;
			// 	ack=NOT_LOGGED_IN;
			// 	send(sockfd, &ack, sizeof(ack), 0);
			// 	cout<<"tracker sent ack to client"<<endl;
			// 	continue;
			// }
			int pos;
			for(auto i : groupMap){
				if(i.first==grpId){//group exists
					if((pos=inList(groupMap[grpId]->listOfPendingReq, userId))!=-1){//checking if user is in the listOfPendingReq (before removing it)
						(groupMap[grpId]->listOfPendingReq).erase((groupMap[grpId]->listOfPendingReq).begin()+pos);
						cout<<"Removed userId from listOfPendingReq!"<<endl;
						(groupMap[grpId]->listOfSeeders).push_back(userId);
						cout<<"Inserted userId into listOfSeeders!"<<endl;
						ack=1;											
					}
					else{//Userid not a 'pending request'; Can't 'accept' it
						cout<<"Err:User not in pending join request"<<endl;
						ack=NOT_PENDING_JOIN;
						//TODO : check if that userid already accepted..i.e present in listOfSeeders already and display msg accordingly
					}
					send(sockfd, &ack, sizeof(ack), 0);
					// cout<<"Send3"<<endl;
					cout<<"tracker sent ack to client; ack:"<<ack<<endl;
					flag=1;
					break;				
				}
			}
			if(flag==1){
				continue;
			}
			cout<<"Err:No such group exists!!"<<endl;//Group with given groupId doesn't exist
			ack=-1;
			send(sockfd, &ack, sizeof(ack), 0);
			cout<<"tracker sent ack to client"<<endl;
		}

		//**********************************************************************************//
		//rcvd cmd-> list_groups gid uid
		else if(strcmp(cmdName, "list_groups")==0){
			// int flag=0;
			cout<<"list_groups case:Tracker"<<endl;
			int sze=groupMap.size();
			// if(sze==0){
			// 	cout<<"No groups yet!!"<<endl;
			// 	ack=NO_GROUPS;
			// 	send(sockfd, &ack, sizeof(ack), 0);
			// 	continue;
			// }
			cout<<"Sending the size of groupMap to client; size:"<<sze<<endl;
			send(sockfd, &sze, sizeof(sze), 0);
			// cout<<"Send1"<<endl;

			//Receiving ack for groupMapSize reception from client
			recv(sockfd, &ack, sizeof(ack),0);
			// cout<<"Rcvd ack1"<<endl;
			cout<<"Sending the groups to client"<<endl;
			for(auto i : groupMap){			
				string listele=i.first;
				send(sockfd, (char*)listele.c_str(), listele.length(), 0);
				cout<<listele<<" ";
				// cout<<"Send2"<<endl;
				recv(sockfd, &ack, sizeof(ack),0);
			}
			cout<<endl;
			cout<<"Sent the list to client"<<endl;
			ack=1;
			send(sockfd, &ack, sizeof(ack), 0);
			// cout<<"Send3"<<endl;
			cout<<"tracker sent ack to client; ack:"<<ack<<endl;
		}

		//**********************************************************************************//
		//rcvd cmd-> list_files gid uid
		else if(strcmp(cmdName, "list_files")==0){
			int flag=0;
			cout<<"list_files case:Tracker"<<endl;
			while((cmdName=strtok(NULL," "))!=NULL){
				cmdVec.push_back(cmdName);
			}
			string grpId=cmdVec[0];
			string userId=cmdVec[1];
			int pos;
			for(auto i : groupMap){
				if(i.first==grpId){//group exists
					//check if user part of the group
					// if(inList((groupMap[i]->ListOfSeeders),userId)!=-1){//user is in group
						int sze=(groupMap[grpId]->fileNm).size();
						cout<<"Sending the size of list of fileNames to client; size:"<<sze<<endl;
						send(sockfd, &sze, sizeof(sze), 0);
						// cout<<"Send1"<<endl;

						//Receiving ack for listSize reception from client
						recv(sockfd, &ack, sizeof(ack),0);
						// cout<<"Rcvd ack1"<<endl;

						cout<<"Sending the fileNames to client"<<endl;
						for(int i=0;i<sze;i++){
							pair<string, int> listelm;
							listelm=(groupMap[grpId]->fileNm)[i];
							if(listelm.second==1){//file is shareable
								string listele=listelm.first;
								cout<<listele<<" ";
								send(sockfd, (char*)listele.c_str(), listele.length(), 0);
								// cout<<"Send2"<<endl;
								recv(sockfd, &ack, sizeof(ack),0);
								// cout<<"Rcvd ack2"<<endl;
							}
							else{
								string listele="Do0Not1Include2";
								send(sockfd, (char*)listele.c_str(), listele.length(), 0);
								// cout<<"Send2"<<endl;
								recv(sockfd, &ack, sizeof(ack),0);
							}						
						}
						cout<<endl;
						cout<<"Sent the list to client"<<endl;
						ack=1;
						send(sockfd, &ack, sizeof(ack), 0);
						// cout<<"Send3"<<endl;
						cout<<"tracker sent ack to client; ack:"<<ack<<endl;
						flag=1;
						break;			
					// }
					// else{//group exists but user not in group
						
					// }
						
				}
					
			}
			if(flag==1){
				continue;
			}
			//If reqd group doesnt exist, then the tracker won't enter the block where it send size and recv ack.
			//But the client is waiting to recv size and will send ack. Hence we need to do this:
			int sze=0;
			send(sockfd, &sze, sizeof(sze), 0);
			cout<<"Sending the size of listOf fileNames to client; size:"<<sze<<endl;
			
			// cout<<"Send1"<<endl;

			//Receiving ack for listSize reception from client
			recv(sockfd, &ack, sizeof(ack),0);

			cout<<"Err:No such group exists!!"<<endl;//Group with given groupId doesn't exist
			ack=-1;
			send(sockfd, &ack, sizeof(ack), 0);
			cout<<"tracker sent ack to client"<<endl;
		}


		//**********************************************************************************//
		else if(strcmp(cmdName, "upload_file")==0){
			cout<<"upload_file case:Tracker"<<endl;
			while((cmdName=strtok(NULL," "))!=NULL){
				// cmdName=strtok(NULL," ");
				cmdVec.push_back(cmdName);
			}

			string fileNm=cmdVec[0];
			string str_fileSz=cmdVec[1];
			string grpID=cmdVec[2];
			string userId=cmdVec[3];
			int filesz=stoi(str_fileSz);

			cout<<"Recieved fileNm, fileSz, grpID, userId: "<<
			fileNm<<" "<< str_fileSz<<"(string) "<<filesz<<"(int)"<<" " <<grpID<<" "<<userId<<endl;

			send(sockfd, &ack, sizeof(ack), 0);
			cout<<"sync1"<<endl;

			recv(sockfd, &ack, sizeof(ack), 0);
			cout<<"sync2"<<endl;

			if(!userInGroup(userId, grpID)){
				ack=NOT_IN_GROUP;
				send(sockfd, &ack, sizeof(ack), 0);
				cout<<"sync3"<<endl;
				cout<<"user not in group"<<endl;
				continue;
			}
			else{
				ack=IN_GROUP;
				send(sockfd, &ack, sizeof(ack), 0);
				cout<<"sync3"<<endl;
				cout<<"user in group"<<endl;
			}

			cout<<"Starting to recieve SHA"<<endl;
			int n;
			char bufSHA[21]={'\0'};
			string finalSHA="";
			ack=666;
			while(n=recv(sockfd, &bufSHA, sizeof(bufSHA), 0)>0){
				cout<<"sync4"<<endl;
				if(strcmp(bufSHA,"endofSHA")==0){
					break;
				}
				cout<<"Recieved SHA: "<<bufSHA<<endl;
				finalSHA+=bufSHA;
				ack=98765;
				// cout<<"UMM"<<endl;
				send(sockfd, &ack, sizeof(ack), 0);
				cout<<"sync5"<<endl;
				memset(bufSHA, '\0', sizeof(bufSHA));
			}
			cout<<"Final SHA recvd:Tracker: "<<finalSHA<<endl;
			cout<<"Length of Final SHA recvd:Tracker: "<<finalSHA.length()<<endl;
			ack=777;
			//Updating  fileMap
			string key=grpID+fileNm;
			if(fileMap.find(key)!=fileMap.end()){//key laready exists
				(fileMap[key]->listOfSeeders).push_back(userId);
			}
			else{
				// struct filedetails *fdet=(struct filedetails*)malloc(sizeof(struct filedetails));
				fileDetail *fdet=new fileDetail;
				cout<<"ping"<<endl;
				fdet->fileSz=filesz;
				cout<<"pingfilesz"<<endl;

				fdet->SHAvar=finalSHA;
				cout<<"pingsha"<<endl;

				(fdet->listOfSeeders).push_back(userId);
				cout<<"pinglistofseeders"<<endl;
				fileMap[key]=fdet;
				cout<<"pingmap"<<endl;
				cout<<"size "<<fileMap[key]->fileSz<<endl;
				cout<<"sha "<<fileMap[key]->SHAvar<<endl;
			}	
			ack=888;		

			//Adding file to groupMap's listOfFileName, if not already present
			int filePresent=0;
			vector<pair<string,int> > vect=groupMap[grpID]->fileNm;
			for(int i=0;i<vect.size();i++){
				if(vect[i].first==fileNm){
					vect[i].second=1;//making file shareable again
					filePresent=1;//a flag
					break;
				}
			}
			if(filePresent==0){
				(groupMap[grpID]->fileNm).push_back(make_pair(fileNm,1));
			}
			cout<<"ping3"<<endl;
			ack=999;
			send(sockfd, &ack, sizeof(ack), 0);//work done
			cout<<"sync6"<<endl;
			cout<<"Exiting upload_file"<<endl;
		}

		//**********************************************************************************//
		//rcvd cmd-> logout uid
		else if(strcmp(cmdName, "logout")==0){
			cout<<"logout case:Tracker"<<endl;
			while((cmdName=strtok(NULL," "))!=NULL){
				// cmdName=strtok(NULL," ");
				cmdVec.push_back(cmdName);
			}
			string userId=cmdVec[0];
			cout<<"received user id for logout:"<<userId<<endl;
			userMap[userId]->loggedIn=0;
			ack=1;
			send(sockfd, &ack, sizeof(ack), 0);
			cout<<"Logged out:Tracker!"<<endl;


		}

		//**********************************************************************************//
		//rcvd cmd-> logdownload_file grpid filenm destn uid
		else if(strcmp(cmdName, "download_file")==0){
			cout<<"download_file case:Tracker"<<endl;
			while((cmdName=strtok(NULL," "))!=NULL){
				cout<<"cmd recvd:"<<cmdName<<endl;
				cmdVec.push_back(cmdName);
			}
			string grpID=cmdVec[0];
			string fileNm=cmdVec[1];
			string dest=cmdVec[2];
			string userId=cmdVec[3];

			string key=grpID+fileNm;

			//Sending file size to client
			int fileSz=fileMap[key]->fileSz;
			cout<<"file size i see:"<<fileSz<<endl;
			send(sockfd,&fileSz,sizeof(fileSz),0);

			recv(sockfd,&ack,sizeof(ack),0);//to sync

			//Sending sha to client
			string sHa=fileMap[key]->SHAvar;
			int lenSHA=sHa.length();
			cout<<"Length of sha to be sent: "<<lenSHA<<endl;
			int cnt=0; 
			cout<<"Going to start sending SHA from Client"<<endl;
			while(cnt<lenSHA){
				string sha_20=sHa.substr(cnt,20);
				cnt+=20;
				cout<<"SHA sent:Tracker: "<<sha_20<<endl;
				send(sockfd, (char*)sha_20.c_str(), sha_20.length(), 0);
				cout<<"sync4"<<endl;
				recv(sockfd, &ack, sizeof(ack),0);
				cout<<"sync5"<<endl;

			}
			char *msg="endofSHA";
			send(sockfd, msg, sizeof(msg), 0);
			cout<<"sync4"<<endl;

			recv(sockfd, &ack, sizeof(ack), 0);//to sync

			//Sending concerned IP+Ports to Client
			vector<string> seederList=fileMap[key]->listOfSeeders;
			//sending no of seeders to client
			int seederSize=seederList.size();
			send(sockfd, &seederSize, sizeof(seederSize), 0);
			recv(sockfd, &ack, sizeof(ack), 0);

			for(auto &userId : seederList){
				if(userMap[userId]->loggedIn==1){
					string pIp=userMap[userId]->ip+" "+to_string(userMap[userId]->portNo);
					send(sockfd,(char*)pIp.c_str(), pIp.length(),0);
					cout<<"Ip+Port sent to client: "<<pIp<<endl;
					recv(sockfd, &ack, sizeof(ack), 0);//to sync
			
				}
				else{
					string pIp="Do0Not1Include2";
					send(sockfd,(char*)pIp.c_str(), pIp.length(),0);
					recv(sockfd, &ack, sizeof(ack), 0);//to sync
				}
			}

			



			// cout<<"received user id for logout:"<<userId<<endl;
			// userMap[userId]->loggedIn=0;
			// ack=1;
			// send(sockfd, &ack, sizeof(ack), 0);
			// cout<<"Logged out:Tracker!"<<endl;


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
			// struct_threadData *cdata=(struct_threadData *)malloc(sizeof(struct_threadData));

			/* 
			TIPS: malloc vs new: 
			malloc only allocates memory, it has no idea what a class is or how to instantiate one. 
			You should generally not use it in C++.(malloc is a C thing)
			"new" allocates memory and constructs an instance of the class..it calls the default constructor.
			Was getting an error while assigning value to a string inside the struct "fileDetail"
			*/
			struct_threadData *cdata=new struct_threadData;
			cdata->sockfd=new_server_socket_fd;
			cdata->client_addr=new_serv_addr;
			//TODO : Is this correct?

			// cout<<"[Tracker]Printing the struct sockaddr_in as updated in accept()"<<endl;
			// cout<<"Port..unmodified "<<new_serv_addr.sin_port<<endl;
			// cout<<"Port..modified "<<to_string(ntohs(new_serv_addr.sin_port))<<endl;
			// cout<<"IP..unmodified "<<new_serv_addr.sin_addr.s_addr<<endl;
			// char *ipp=new char[1000];
			// inet_ntop(AF_INET,&(new_serv_addr.sin_addr),ipp,1000);
			// cout<<"IP..modified "<<ipp<<endl; //--> This gives back the IP in the format entered at sender's(peer's client's) end.
			//->Like 127.0.0.1


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

