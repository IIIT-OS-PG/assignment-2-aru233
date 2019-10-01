#include <stdio.h>
#include <unistd.h>
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
	int server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if(server_fd<0){
		perror("Socket creation failed");
		exit(EXIT_FAILURE);
	}
	cout<<"socket created!"<<endl;

	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(PORT);
	addr.sin_addr.s_addr=inet_addr("127.0.0.1");
	cout<<"inaddr_any "<<inet_addr("127.0.0.1")<<endl;

	int addrlen = sizeof(addr);

	//In (struct sockaddr*)&addr, sockaddr_in doesn't work!
	int bind_status=bind(server_fd , (struct sockaddr *)&addr , sizeof ( addr ));
	if(bind_status<0){
		perror("Binding failed");
		exit(EXIT_FAILURE);
	}
	cout<<"Binding done!"<<endl;

	int listen_status=listen (server_fd, 3);
	if(listen_status<0){
		perror("ECONNREFUSED");
		exit(EXIT_FAILURE);
	}
	cout<<"Listening!"<<endl;

	int sockfd = accept(server_fd , (struct sockaddr *)&addr , (socklen_t*)&addrlen);
	if(sockfd<0){
		perror("Accept() failed");
		exit(EXIT_FAILURE);
	}
	cout<<"Accepted!"<<endl;

	FILE *fp = fopen("song1.mp3" , "wb");

	char buffer[BUFF_SIZE]={0};

	int file_size, n;
	recv(sockfd, &file_size, sizeof(file_size), 0);
	cout<<"FIle size "<< file_size<<endl;
	while ((n = recv(sockfd , buffer , BUFF_SIZE, 0) ) > 0 && file_size > 0){
		cout<<"Buff "<<buffer<<endl;
		fwrite (buffer , sizeof (char), n, fp);
		memset ( buffer , '\0', BUFF_SIZE);
		file_size = file_size - n;
	}
	close(sockfd);
	close(server_fd);
	fclose(fp);

	return 0;

}