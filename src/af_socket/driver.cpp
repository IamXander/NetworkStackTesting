#include "driver.h"

#include <cassert>
#include <chrono>
#include <iostream>

#include <sys/socket.h>
#include <sys/sendfile.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>

result_t recv_data(uint16_t port) {
    int server_fd, new_socket, valread;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);
       
    // Creating socket file descriptor 
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    } 
       
    // Forcefully attaching socket to the port 8080
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    } 
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);
       
    // Forcefully attaching socket to the port 8080
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    } 
    if (listen(server_fd, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    } 
    if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
        perror("accept");
        exit(EXIT_FAILURE);
    }
	uint64_t packets_and_size[2];
	valread = recv(new_socket, packets_and_size, sizeof(packets_and_size), MSG_WAITALL);
	assert(valread == sizeof(packets_and_size));
	uint64_t packets = packets_and_size[0];
	uint64_t packet_size = packets_and_size[1];
	uint8_t buffer[packet_size];

    auto start = std::chrono::system_clock::now();
    for (uint64_t i = 0; i < packets; ++i) {
        valread = recv(new_socket, buffer, packet_size, MSG_WAITALL);
    }
    auto end = std::chrono::system_clock::now();
    std::chrono::duration<uint64_t, std::nano> diff = end - start;
    result_t r;
    r.nanos = diff.count();
    return r;
}

int send_data(std::string ip, uint16_t port, uint64_t packets, uint64_t packet_size) {
    struct sockaddr_in address;
    int sock = 0, valread;
    struct sockaddr_in serv_addr;
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) { 
        printf("\n Socket creation error \n");
        return -1;
    } 
   
    memset(&serv_addr, '0', sizeof(serv_addr));
   
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
       
    // Convert IPv4 and IPv6 addresses from text to binary form 
    if(inet_pton(AF_INET, ip.c_str(), &serv_addr.sin_addr) <= 0) { 
        printf("\nInvalid address/ Address not supported \n");
        return -1;
    } 
   
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) { 
        printf("\nConnection Failed \n");
        return -1;
    }
	valread = send(sock, &packets, sizeof(packets), 0);
	assert(valread == sizeof(packets));
	valread = send(sock, &packet_size, sizeof(packet_size), 0);
	assert(valread == sizeof(packet_size));

	#ifdef SEND_FILE
		char filename_template[11] = "NST.XXXXXX";
		int buffer_fd = mkstemp(filename_template);
		if (buffer_fd == -1) {
			std::cout << "ERROR 1" << std::endl;
			return -1;
		}
		if (unlink(filename_template) < 0) {
			std::cout << "ERROR 2" << std::endl;
			return -1;
		}
		if (ftruncate(buffer_fd, packet_size) < 0) {
			std::cout << "ERROR 3" << std::endl;
			return -1;
		}
	#else
		char buffer[packet_size];
		memset(buffer, 0, packet_size);
	#endif

    std::cout << "Starting data send..." << std::endl;
    for (uint64_t i = 0; i < packets; ++i) {
		#ifdef SEND_FILE
			valread = 0;
			while (valread < packet_size)
				valread += sendfile(sock, buffer_fd, NULL, packet_size);
			//TODO: this is borked
		#else
        	valread = send(sock, buffer, packet_size, 0);
		#endif
		// assert(valread == packet_size);
    }
    std::cout << "Stopped sending data" << std::endl;

	#ifdef SEND_FILE
		close(buffer_fd);
	#endif
    return 0;
}