#include "driver.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <iostream>
#include <string.h>
#include <chrono>

result_t recv_data(uint16_t port, uint64_t packets, uint64_t packet_size) {
    int server_fd, new_socket, valread;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);
    uint8_t buffer[packet_size];
       
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
    auto start = std::chrono::system_clock::now();
    for (uint64_t i = 0; i < packets; ++i) {
        valread = recv(new_socket, buffer, packet_size, MSG_WAITALL);
    }
    auto end = std::chrono::system_clock::now();
    std::chrono::duration<uint64_t, std::nano> diff = end - start;
    result_t r;
    r.nanos = diff.count();
    std::cout << "Nanos: " << r.nanos << std::endl;
    return r;
}

int send_data(std::string ip, uint16_t port, uint64_t packets, uint64_t packet_size) {
    struct sockaddr_in address;
    int sock = 0, valread;
    struct sockaddr_in serv_addr;
    char buffer[packet_size] = {0};
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
    std::cout << "Starting data send..." << std::endl;
    for (uint64_t i = 0; i < packets; ++i) {
        valread = send(sock, buffer, packet_size, 0);
    }
    std::cout << "Sent all data..." << std::endl;
    std::cout << valread << std::endl;
    return 0;
}