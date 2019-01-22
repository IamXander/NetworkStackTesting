#include "driver.h"

#include <ctime>
#include <string>
#include <boost/asio.hpp>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <iostream>
#include <string.h>
#include <chrono>

result_t recv_data(uint16_t port, uint64_t packets, uint64_t packet_size) {
    boost::asio::io_context io_context;
    boost::asio::ip::tcp::acceptor acceptor(io_context, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port));
    boost::asio::ip::tcp::socket socket(io_context);
    acceptor.accept(socket);
    boost::system::error_code ignored_error;
    std::vector<char> bu(packet_size);
    boost::asio::mutable_buffer buffer(bu.data(), bu.size());
    
    auto start = std::chrono::system_clock::now();
    for (uint64_t i = 0; i < packets; ++i) {
        boost::asio::read(socket, buffer);
    }
    auto end = std::chrono::system_clock::now();
    std::chrono::duration<uint64_t, std::nano> diff = end - start;
    result_t r;
    r.nanos = diff.count();
    std::cout << "Nanos: " << r.nanos << std::endl;
    return r;
}

int send_data(std::string ip, uint16_t port, uint64_t packets, uint64_t packet_size) {
    boost::asio::io_context io_context;
    boost::asio::ip::tcp::socket socket(io_context);
    socket.connect(boost::asio::ip::tcp::endpoint(boost::asio::ip::make_address_v4(ip), port));
    std::vector<char> bu(packet_size);
    boost::asio::mutable_buffer buffer(bu.data(), bu.size());

    std::cout << "Started sending data" << std::endl;
    auto start = std::chrono::system_clock::now();
    for (uint64_t i = 0; i < packets; ++i) {
        boost::asio::write(socket, buffer);
    }
    auto end = std::chrono::system_clock::now();
    std::cout << "Stopped sending data" << std::endl;
    std::chrono::duration<uint64_t, std::nano> diff = end - start;
    result_t r;
    r.nanos = diff.count();
    std::cout << "Nanos: " << r.nanos << std::endl;
    return 0;
}