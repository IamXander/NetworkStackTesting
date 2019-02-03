#include "driver.h"

#include <ctime>
#include <string>
#include <vector>
#include <boost/asio.hpp>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <iostream>
#include <string.h>
#include <chrono>

result_t recv_data(uint16_t port) {
    boost::asio::io_context io_context;
    boost::asio::ip::tcp::acceptor acceptor(io_context, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port));
    boost::asio::ip::tcp::socket socket(io_context);
    acceptor.accept(socket);

	// Both sides must have the same endianness
	uint64_t packets_and_size[2];
	auto boost_buffer = boost::asio::buffer(packets_and_size, sizeof(packets_and_size));
	size_t bytesRead = boost::asio::read(socket, boost_buffer);
	
	assert(bytesRead == sizeof(packets_and_size));
	uint64_t packets = *reinterpret_cast<uint64_t*>(boost_buffer.data());
	uint64_t packet_size = *(reinterpret_cast<uint64_t*>(boost_buffer.data()) + 1);

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
    return r;
}

int send_data(std::string ip, uint16_t port, uint64_t packets, uint64_t packet_size) {
    boost::asio::io_context io_context;
    boost::asio::ip::tcp::socket socket(io_context);
    socket.connect(boost::asio::ip::tcp::endpoint(boost::asio::ip::make_address_v4(ip), port));
    std::vector<char> bu(packet_size);
	bu.assign(bu.size(), 0);
    boost::asio::mutable_buffer buffer(bu.data(), bu.size());

	// Both sides must have the same endianness
	// Have to do this madness because boost will convert to network byte order :(
	std::vector<char> packets_and_size(2 * sizeof(uint64_t));
	memcpy(packets_and_size.data(), &packets, sizeof(packets));
	memcpy(packets_and_size.data() + sizeof(packets), &packet_size, sizeof(packet_size));
	boost::asio::write(socket, boost::asio::buffer(packets_and_size, packets_and_size.size()));

	std::cout << "Starting data send..." << std::endl;
	auto start = std::chrono::system_clock::now();
	for (uint64_t i = 0; i < packets; ++i) {
		boost::asio::write(socket, buffer);
	}
    auto end = std::chrono::system_clock::now();
    std::cout << "Stopped sending data" << std::endl;
    std::chrono::duration<uint64_t, std::nano> diff = end - start;
    result_t r;
    r.nanos = diff.count();
    return 0;
}