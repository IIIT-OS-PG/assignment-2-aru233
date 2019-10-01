#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <iostream>
#include <string.h>
#include <bits/stdc++.h>
#include <netinet/in.h>
#include <arpa/inet.h>

using namespace std;

#define BUFF_SIZE 2048
#define PORT 2000

int main(){
	int sockfd=socket(AF_INET, SOCK_STREAM, 0);
	if(sockfd<0){
		perror("Socket creation failed");
		exit(EXIT_FAILURE);
	}

	struct sockaddr_in serv_addr;
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons( PORT );
	serv_addr.sin_addr.s_addr=inet_addr("10.1.100.250");

	//In (struct sockaddr*)&serv_addr, sockaddr_in doesn't work!
	if(connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr))<0){
		perror("Connect failed");
		exit(EXIT_FAILURE);
	}

	FILE *fp=fopen("song.mp3", "rb");


	// SEEK_END : It denotes end of the file.
	// SEEK_SET : It denotes starting of the file.
	// SEEK_CUR : It denotes file pointer’s current position.


	fseek(fp, 0, SEEK_END);
	//above means that we move the pointer by 0 distance with respect to end of file i.e pointer now points to end of the file. 
	int file_size=ftell(fp);
	cout<<"File size "<<file_size<<endl;
	rewind(fp);

	send(sockfd, &file_size, sizeof(file_size), 0);

	char Buffer[BUFF_SIZE];
	int n;
	while((n=fread(Buffer, sizeof(char), BUFF_SIZE, fp)) > 0 && file_size>0){
		send(sockfd, Buffer, n, 0);
		// cout<<"in buff"<<Buffer<<endl;
		memset(Buffer, '\0', BUFF_SIZE);
		file_size=file_size-n;
	}

	fclose(fp);
	close(sockfd);
}
