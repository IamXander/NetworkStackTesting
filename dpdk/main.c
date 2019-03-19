#include <stdint.h>
#include <inttypes.h>
#include <assert.h>
#include <stdlib.h>

#include <arpa/inet.h>
#include <time.h>

#include <pthread.h>

#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_cycles.h>
#include <rte_lcore.h>
#include <rte_mbuf.h>
#include <rte_ether.h>
#include <rte_arp.h>
#include <rte_ip.h>

#define RX_RING_SIZE 1024
#define TX_RING_SIZE 1024

#define NUM_MBUFS 8191
#define MBUF_CACHE_SIZE 250
#define BURST_SIZE 32

static const struct rte_eth_conf port_conf_default = {
	.rxmode = {
		.max_rx_pkt_len = 9000, //ETHER_MAX_LEN,
	},
};

struct send_data {
	uint16_t portid;
	struct rte_mempool *mbuf_pool;
	uint32_t dest_ip;
	uint32_t src_ip;
	uint32_t packet_size;
	uint64_t data_to_send;
	int do_copy;
	int tx_rings;
};

pthread_mutex_t mutex;
int cores_done = 1;
int core_count = 0;

static inline int port_init(uint16_t port, struct rte_mempool *mbuf_pool, uint16_t tx_rings) {
	struct rte_eth_conf port_conf = port_conf_default;
	const uint16_t rx_rings = 1;
	uint16_t nb_rxd = RX_RING_SIZE;
	uint16_t nb_txd = TX_RING_SIZE;
	int retval;
	uint16_t q;
	struct rte_eth_dev_info dev_info;
	struct rte_eth_txconf txconf;

	if (!rte_eth_dev_is_valid_port(port))
		return -1;

	rte_eth_dev_info_get(port, &dev_info);
	if (dev_info.tx_offload_capa & DEV_TX_OFFLOAD_MBUF_FAST_FREE)
		port_conf.txmode.offloads |=
			DEV_TX_OFFLOAD_MBUF_FAST_FREE;

	/* Configure the Ethernet device. */
	retval = rte_eth_dev_configure(port, rx_rings, tx_rings, &port_conf);
	if (retval != 0)
		return retval;

	retval = rte_eth_dev_adjust_nb_rx_tx_desc(port, &nb_rxd, &nb_txd);
	if (retval != 0)
		return retval;

	/* Allocate and set up 1 RX queue per Ethernet port. */
	for (q = 0; q < rx_rings; q++) {
		retval = rte_eth_rx_queue_setup(port, q, nb_rxd,
				rte_eth_dev_socket_id(port), NULL, mbuf_pool);
		if (retval < 0)
			return retval;
	}

	txconf = dev_info.default_txconf;
	txconf.offloads = port_conf.txmode.offloads;
	/* Allocate and set up 1 TX queue per Ethernet port. */
	for (q = 0; q < tx_rings; q++) {
		retval = rte_eth_tx_queue_setup(port, q, nb_txd,
				rte_eth_dev_socket_id(port), &txconf);
		if (retval < 0)
			return retval;
	}

	/* Start the Ethernet port. */
	retval = rte_eth_dev_start(port);
	if (retval < 0)
		return retval;

	/* Display the port MAC address. */
	struct ether_addr addr;
	rte_eth_macaddr_get(port, &addr);
	printf("Port %u MAC: %02" PRIx8 " %02" PRIx8 " %02" PRIx8
			   " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 "\n",
			port,
			addr.addr_bytes[0], addr.addr_bytes[1],
			addr.addr_bytes[2], addr.addr_bytes[3],
			addr.addr_bytes[4], addr.addr_bytes[5]);

	/* Enable RX in promiscuous mode for the Ethernet device. */
	rte_eth_promiscuous_enable(port);

	return 0;
}

void send_arp(uint16_t portid, struct rte_mempool *mbuf_pool, uint32_t dest_ip, uint32_t send_ip, struct ether_addr *mac_addr) {
	/* ARP */
	printf("Sending arp...\n");
	struct rte_mbuf *arp_buf = rte_pktmbuf_alloc(mbuf_pool);
	arp_buf->data_len = sizeof(struct ether_hdr) + sizeof(struct arp_hdr);
	arp_buf->pkt_len = sizeof(struct ether_hdr) + sizeof(struct arp_hdr);
	struct ether_hdr* ether = rte_pktmbuf_mtod(arp_buf, struct ether_hdr *);
	struct arp_hdr* arp = rte_pktmbuf_mtod_offset(arp_buf, struct arp_hdr *, sizeof(struct ether_hdr));

	//ether
	ether->ether_type = htons(ETHER_TYPE_ARP);
	rte_eth_macaddr_get(portid, &ether->s_addr);
	memset(&ether->d_addr, 0xff, 6); //broadcast

	//ipv4
	arp->arp_hrd = htons(ARP_HRD_ETHER);
	arp->arp_pro = htons(ETHER_TYPE_IPv4);
	arp->arp_hln = 6;
	arp->arp_pln = 4;
	arp->arp_op = htons(ARP_OP_REQUEST);
	rte_eth_macaddr_get(portid, &arp->arp_data.arp_sha);
	arp->arp_data.arp_sip = send_ip;
	memset(&arp->arp_data.arp_tha, 0x00, 6);
	arp->arp_data.arp_tip = dest_ip;

	// for (uint16_t i = 0; i < arp_buf->data_len; i++) {
	// 	uint8_t c = *(((uint8_t*) arp_buf->buf_addr) + i + RTE_PKTMBUF_HEADROOM);
	// 	printf("%x ", c);
	// }

	// Send ARP
	uint16_t nb_tx = 0;
	do {
		nb_tx = rte_eth_tx_burst(portid, 0, &arp_buf, 1);
	} while (unlikely(nb_tx < 1));
	printf("Arp dispatched...\nWaiting for response...\n");

	// Receive an ARP back
	uint16_t nb_rx = 0;
	struct rte_mbuf* arp_buf_reply = NULL;
	do {
		nb_rx = rte_eth_rx_burst(portid, 0, &arp_buf_reply, 1);
		if (nb_rx == 1) {
			uint8_t* data = rte_pktmbuf_mtod(arp_buf_reply, uint8_t*);
			if (data[0] == 0x1 && data[1] == 0x80) {
				printf("Got a stupid packet...\n");
				rte_pktmbuf_free(arp_buf_reply);
				arp_buf_reply = NULL;
				nb_rx = 0;
			}
		}
	} while (nb_rx == 0);
	printf("Got arp response!\n");

	// for (uint16_t i = 0; i < rte_pktmbuf_data_len(arp_buf_reply); i++) {
	// 	uint8_t c = *(((uint8_t*) arp_buf_reply->buf_addr) + i + RTE_PKTMBUF_HEADROOM);
	// 	printf("%x ", c);
	// }

	//Process ARP
	ether = rte_pktmbuf_mtod(arp_buf_reply, struct ether_hdr *);
	assert(ntohs(ether->ether_type) == ETHER_TYPE_ARP);
	arp = rte_pktmbuf_mtod_offset(arp_buf_reply, struct arp_hdr *, sizeof(struct ether_hdr));
	assert(ntohs(arp->arp_op) == ARP_OP_REPLY);
	memcpy(mac_addr, &ether->s_addr, sizeof(ether->s_addr)); //We got the MAC to send data to
}

/*
 * The lcore main. This is the main thread that does the work, reading from
 * an input port and writing to an output port.
 */
static void lcore_main_flyback(struct send_data* sd) {
	uint16_t portid = sd->portid;
	uint32_t ip = sd->src_ip;
	int do_copy = sd->do_copy;
	char temp_copy[10000];
	free(sd);
	/*
	 * Check that the port is on the same NUMA node as the polling thread
	 * for best performance.
	 */
	if (rte_eth_dev_socket_id(portid) > 0 &&
			rte_eth_dev_socket_id(portid) !=
					(int)rte_socket_id())
		printf("WARNING, port %u is on remote NUMA node to "
				"polling thread.\n\tPerformance will "
				"not be optimal.\n", portid);

	printf("\nCore %u forwarding packets. [Ctrl+C to quit]\n",
			rte_lcore_id());

	/* Run until the application is quit or killed. */
	while (1) {
		struct rte_mbuf *buf;
		const uint16_t nb_rx = rte_eth_rx_burst(portid, 0, &buf, 1);
		if (unlikely(nb_rx == 0))
			continue;
		uint8_t* data = rte_pktmbuf_mtod(buf, uint8_t*);
		if (data[0] == 0x1 && data[1] == 0x80) {
			printf("Got a stupid packet...\n");
			rte_pktmbuf_free(buf);
			continue;
		}
		struct ether_hdr* ether = rte_pktmbuf_mtod(buf, struct ether_hdr *);
		
		if (ntohs(ether->ether_type) == ETHER_TYPE_ARP) { //ARP - we gotta arp back
			printf("We got an arp!\n");
			struct arp_hdr* arp = rte_pktmbuf_mtod_offset(buf, struct arp_hdr *, sizeof(struct ether_hdr));

			//Ethernet good to send out
			memcpy(&ether->d_addr, &ether->s_addr, sizeof(ether->s_addr));
			rte_eth_macaddr_get(portid, &ether->s_addr);

			arp->arp_op = htons(ARP_OP_REPLY);
			memcpy(&arp->arp_data.arp_tha, &arp->arp_data.arp_sha, sizeof(arp->arp_data.arp_tha));
			memcpy(&arp->arp_data.arp_tip, &arp->arp_data.arp_tip, sizeof(arp->arp_data.arp_tip));
			rte_eth_macaddr_get(portid, &arp->arp_data.arp_sha);
			arp->arp_data.arp_sip = ip;
		} else if (ntohs(ether->ether_type) == ETHER_TYPE_IPv4) {
			struct ipv4_hdr* ipv4 = rte_pktmbuf_mtod_offset(buf, struct ipv4_hdr *, sizeof(struct ether_hdr));

			// Switch etherpacket
			rte_memcpy(&ether->d_addr, &ether->s_addr, sizeof(ether->s_addr));
			rte_eth_macaddr_get(portid, &ether->s_addr);

			//Switch IP packet
			ipv4->dst_addr = ipv4->src_addr;
			ipv4->src_addr = ip;
			ipv4->hdr_checksum = 0;
			ipv4->hdr_checksum = rte_ipv4_cksum(ipv4);

			if (do_copy) {
				rte_memcpy(temp_copy, rte_pktmbuf_mtod(buf, uint8_t*), buf->data_len);
			}
		}
		// Send that sucker back!
		uint16_t nb_tx = 0;
		do {
			nb_tx += rte_eth_tx_burst(portid, 0, &buf, 1);
		} while (unlikely(nb_tx < 1));
	}
}

void print_diff(const char* title, struct timespec* start, struct timespec* end) {
	time_t sec = end->tv_sec - start->tv_sec;
	long nanos = end->tv_nsec - start->tv_nsec;
	double seconds = ((double) sec) + ((double)nanos / 1000000000.0);
	printf("%s: seconds: %f, nanos: %ld\n", title, seconds, nanos);
}

/*
 * The lcore main. This is the main thread that does the work, reading from
 * an input port and writing to an output port.
 */
static int lcore_main_send_latency(void * SD) {
	pthread_mutex_lock(&mutex);
	struct send_data* sd = (struct send_data*) SD;
	uint16_t portid = sd->portid;
	struct rte_mempool *mbuf_pool = sd->mbuf_pool;
	uint32_t ip = sd->src_ip;
	uint32_t dest_ip = sd->dest_ip;
	uint32_t packet_size = sd->packet_size;
	if (packet_size < sizeof(struct ether_hdr) + sizeof(struct ipv4_hdr)) packet_size = sizeof(struct ether_hdr) + sizeof(struct ipv4_hdr);
	uint32_t data_size = packet_size - (sizeof(struct ether_hdr) + sizeof(struct ipv4_hdr));
	uint64_t data_to_send = sd->data_to_send;

	double avg_time = 0;
	uint64_t packets_sent = 0;
	free(sd);
	/*
	 * Check that the port is on the same NUMA node as the polling thread
	 * for best performance.
	 */
	if (rte_eth_dev_socket_id(portid) > 0 &&
			rte_eth_dev_socket_id(portid) !=
					(int)rte_socket_id())
		printf("WARNING, port %u is on remote NUMA node to "
				"polling thread.\n\tPerformance will "
				"not be optimal.\n", portid);

	printf("\nCore %u forwarding packets. [Ctrl+C to quit]\n",
			rte_lcore_id());

	struct ether_hdr ether_with_mac;
	ether_with_mac.ether_type = htons(ETHER_TYPE_IPv4);
	rte_eth_macaddr_get(portid, &ether_with_mac.s_addr);
	send_arp(portid, mbuf_pool, dest_ip, ip, &ether_with_mac.d_addr);

	// Set up IPv4 Req
	struct ipv4_hdr ipv4_default;
	ipv4_default.version_ihl = 0x45;
	ipv4_default.type_of_service = 0;
	ipv4_default.total_length = htons(20 + data_size);
	ipv4_default.packet_id = 0;
	ipv4_default.fragment_offset = 0x4000;
	ipv4_default.time_to_live = 64;
	ipv4_default.next_proto_id = 0x11; //UDP
	ipv4_default.hdr_checksum = 0;
	ipv4_default.src_addr = ip;
	ipv4_default.dst_addr = dest_ip;
	ipv4_default.hdr_checksum = rte_ipv4_cksum(&ipv4_default);

	cores_done++;
	pthread_mutex_unlock(&mutex);
	while(1) { //Busy wait barrier for all arps to get sent
		pthread_mutex_lock(&mutex);
		if (cores_done == rte_lcore_count()) break;
		pthread_mutex_unlock(&mutex);
	}
	pthread_mutex_unlock(&mutex);

	/* Run until the application is quit or killed. */
	uint64_t bytesSent = 0;
	while (bytesSent < data_to_send) {
		struct rte_mbuf *buf;
		int rc = rte_pktmbuf_alloc_bulk(mbuf_pool, &buf, 1);
		assert(rc == 0);
		buf->data_len = packet_size;
		buf->pkt_len = packet_size;
		bytesSent += packet_size;
		struct ether_hdr* ether = rte_pktmbuf_mtod(buf, struct ether_hdr *);
		struct ipv4_hdr* ipv4 = rte_pktmbuf_mtod_offset(buf, struct ipv4_hdr *, sizeof(struct ether_hdr));

		//ether
		rte_memcpy(ether, &ether_with_mac, sizeof(ether_with_mac));

		//ipv4
		rte_memcpy(ipv4, &ipv4_default, sizeof(ipv4_default));

		struct timespec start;
		rc = clock_gettime(CLOCK_REALTIME, &start);
		if (rc != 0) {
			printf("%s\n", strerror(errno));
		}
		uint16_t nb_tx = 0;
		do {
			nb_tx = rte_eth_tx_burst(portid, rte_lcore_id(), &buf, 1);
		} while (unlikely(nb_tx < 1));

		struct rte_mbuf* reply_buf = NULL;
		uint16_t nb_rx = 0;
		int stupidPacket = 0;
		do {
			nb_rx = rte_eth_rx_burst(portid, 0, &reply_buf, 1);
			if (nb_rx == 1) {
				uint8_t* data = rte_pktmbuf_mtod(reply_buf, uint8_t*);
				if (data[0] == 0x1 && data[1] == 0x80) {
					printf("Got a stupid packet...\n");
					rte_pktmbuf_free(reply_buf);
					reply_buf = NULL;
					stupidPacket = 1;
					nb_rx = 0;
					bytesSent -= packet_size;
					continue;
				}
			}
		} while (nb_rx == 0);

		rte_pktmbuf_free(reply_buf);
		struct timespec end;
		rc = clock_gettime(CLOCK_REALTIME, &end);
		if (rc != 0) {
			printf("%s\n", strerror(errno));
		}
		if (!stupidPacket) {
			double nanos = ((end.tv_nsec - start.tv_nsec) + ((end.tv_sec - start.tv_sec) * 1000000000.0));
			avg_time = ((avg_time * packets_sent) + nanos) / (++packets_sent);
		}
		// print_diff("RT TIME:", &start, &end);
	}
	printf("Average time: %f\n", avg_time);
	printf("Packet size: %d\n", packet_size);
	printf("Packets sent: %d\n", packets_sent);
	return 0;
}

/*
 * The lcore main. This is the main thread that does the work, reading from
 * an input port and writing to an output port.
 */
static int lcore_main_send_speed(void * SD) {
	pthread_mutex_lock(&mutex);
	struct send_data* sd = (struct send_data*) SD;
	uint16_t portid = sd->portid;
	struct rte_mempool *mbuf_pool = sd->mbuf_pool;
	uint32_t ip = sd->src_ip;
	uint32_t dest_ip = sd->dest_ip;
	uint32_t packet_size = sd->packet_size;
	if (packet_size < sizeof(struct ether_hdr) + sizeof(struct ipv4_hdr)) packet_size = sizeof(struct ether_hdr) + sizeof(struct ipv4_hdr);
	uint32_t data_size = packet_size - (sizeof(struct ether_hdr) + sizeof(struct ipv4_hdr));
	uint64_t data_to_send = sd->data_to_send;
	int tx_rings = sd->tx_rings;
	int core = core_count++;
	int core_tx_ring_offset = core * tx_rings;
	int current_queue = 0;
	free(sd);
	/*
	 * Check that the port is on the same NUMA node as the polling thread
	 * for best performance.
	 */
	if (rte_eth_dev_socket_id(portid) > 0 &&
			rte_eth_dev_socket_id(portid) !=
					(int)rte_socket_id())
		printf("WARNING, port %u is on remote NUMA node to "
				"polling thread.\n\tPerformance will "
				"not be optimal.\n", portid);

	printf("\nCore %u forwarding packets. [Ctrl+C to quit]\n", rte_lcore_id());
	printf("Core %u = Core_count %d\n", rte_lcore_id(), core);

	struct ether_hdr ether_with_mac;
	ether_with_mac.ether_type = htons(ETHER_TYPE_IPv4);
	rte_eth_macaddr_get(portid, &ether_with_mac.s_addr);
	send_arp(portid, mbuf_pool, dest_ip, ip, &ether_with_mac.d_addr);

	// Set up IPv4 Req
	struct ipv4_hdr ipv4_default;
	ipv4_default.version_ihl = 0x45;
	ipv4_default.type_of_service = 0;
	ipv4_default.total_length = htons(20 + data_size);
	ipv4_default.packet_id = 0;
	ipv4_default.fragment_offset = 0x4000;
	ipv4_default.time_to_live = 64;
	ipv4_default.next_proto_id = 0x11; //UDP
	ipv4_default.hdr_checksum = 0;
	ipv4_default.src_addr = ip;
	ipv4_default.dst_addr = dest_ip;
	ipv4_default.hdr_checksum = rte_ipv4_cksum(&ipv4_default);

	cores_done++;
	pthread_mutex_unlock(&mutex);
	while(1) { //Busy wait barrier for all arps to get sent
		pthread_mutex_lock(&mutex);
		if (cores_done == rte_lcore_count()) break;
		pthread_mutex_unlock(&mutex);
	}
	pthread_mutex_unlock(&mutex);

	/* Run until the application is quit or killed. */
	uint64_t bytesSent = 0;
	uint64_t packets_sent = 0;
	struct timespec start;
	int rc = clock_gettime(CLOCK_REALTIME, &start);
	if (rc != 0) {
		printf("%s\n", strerror(errno));
	}
	unsigned long long cyclesOnSend = 0;
	unsigned long long cyclesStart = __rdtsc();
	while (bytesSent < data_to_send) {
		// printf("trying to send...\n");
		// struct timespec begin;
		// clock_gettime(CLOCK_REALTIME, &begin);
		struct rte_mbuf *bufs[BURST_SIZE];
		rc = rte_pktmbuf_alloc_bulk(mbuf_pool, bufs, BURST_SIZE);
		assert(rc == 0);
		// struct timespec post_alloc;
		// clock_gettime(CLOCK_REALTIME, &post_alloc);
		// print_diff("Alloc Time", &begin, &post_alloc);
		// clock_gettime(CLOCK_REALTIME, &post_alloc);
		for (int i = 0; i < BURST_SIZE; i++) {
			struct rte_mbuf * buf = bufs[i];
			buf->data_len = packet_size;
			buf->pkt_len = packet_size;
			bytesSent += packet_size;
			struct ether_hdr* ether = rte_pktmbuf_mtod(buf, struct ether_hdr *);
			struct ipv4_hdr* ipv4 = rte_pktmbuf_mtod_offset(buf, struct ipv4_hdr *, sizeof(struct ether_hdr));

			//ether
			rte_memcpy(ether, &ether_with_mac, sizeof(ether_with_mac));

			//ipv4
			rte_memcpy(ipv4, &ipv4_default, sizeof(ipv4_default));
		}
		current_queue = (current_queue + 1) % tx_rings;
		// struct timespec post_memcpy;
		// clock_gettime(CLOCK_REALTIME, &post_memcpy);
		// print_diff("Memcpy Time", &post_alloc, &post_memcpy);
		// clock_gettime(CLOCK_REALTIME, &post_memcpy);

		// pthread_mutex_lock(&mutex);
		unsigned long long cyclesStartBurst = __rdtsc();
		uint16_t nb_tx = 0;
		do {
			nb_tx += rte_eth_tx_burst(portid, core_tx_ring_offset + current_queue, bufs + nb_tx, BURST_SIZE - nb_tx);
		} while (unlikely(nb_tx < BURST_SIZE));
		unsigned long long cyclesEndBurst = __rdtsc();
		cyclesOnSend += cyclesEndBurst - cyclesStartBurst;

		// struct timespec end;
		// clock_gettime(CLOCK_REALTIME, &end);
		// print_diff("TX Burst Time", &post_memcpy, &end);
		// pthread_mutex_unlock(&mutex);
		packets_sent += BURST_SIZE;
	}
	unsigned long long cyclesEnd = __rdtsc();
	struct timespec end;
	rc = clock_gettime(CLOCK_REALTIME, &end);
	if (rc != 0) {
		printf("%s\n", strerror(errno));
	}
	time_t sec = end.tv_sec - start.tv_sec;
	long nanos = end.tv_nsec - start.tv_nsec;
	double seconds = ((double) sec) + ((double)nanos / 1000000000.0);
	double bytesSentGbps = bytesSent / 1000000000.0;
	double bps = (bytesSent * 8.0) / seconds;
	double gbps = bps / 1000000000.0;

	unsigned long long totalCycles = cyclesEnd - cyclesStart;
	double cpuUsage = (double)cyclesOnSend / (double)totalCycles;
	//cyclesOnSend
	pthread_mutex_lock(&mutex);
	printf("seconds: %f, data sent (GBs): %f\nbps: %f, gbps: %f\n", seconds, bytesSentGbps, bps, gbps);
	printf("packets sent: %d\n", packets_sent);
	printf("packets size: %d\n", packet_size);
	printf("core: %d queues: %d\n", rte_lcore_id(), tx_rings);
	printf("total cycles: %llu, cycles sending: %llu, sending/total: %f\n", totalCycles, cyclesOnSend, cpuUsage);
	pthread_mutex_unlock(&mutex);
	return 0;
}

/*
 * The main function, which does initialization and calls the per-lcore
 * functions.
 */
int main(int argc, char *argv[]) {
	struct rte_mempool *mbuf_pool;
	unsigned nb_ports;
	uint16_t portid = -1;
	uint32_t ip = 0;
	uint32_t dest_ip = 0;
	uint32_t packet_size = 0;
	uint64_t data_to_send = 0;
	uint16_t tx_rings = 1;
	int do_copy = 0;
	int speed = 0;
	int latency = 0;
	int client = 0;
	int server = 0;
	for (int i = 0; i < argc; i++) {
		if (!strcmp(argv[i], "--ip")) {
			++i;
			assert(i < argc);
			ip = inet_addr(argv[i]);
		} else if (!strcmp(argv[i], "--client")) {
			client = 1;
			++i;
			assert(i < argc);
			assert(!server);
			dest_ip = inet_addr(argv[i]);
		} else if (!strcmp(argv[i], "--server")) {
			server = 1;
			assert(!client);
		} else if (!strcmp(argv[i], "--port")) {
			++i;
			assert(i < argc);
			portid = atoi(argv[i]);
		} else if (!strcmp(argv[i], "--sz")) {
			++i;
			assert(i < argc);
			packet_size = atoi(argv[i]);
		} else if (!strcmp(argv[i], "--MB-to-send")) {
			++i;
			assert(i < argc);
			data_to_send = atoll(argv[i]) * 1000000;
		} else if (!strcmp(argv[i], "--B-to-send")) {
			++i;
			assert(i < argc);
			data_to_send = atoll(argv[i]);
		}  else if (!strcmp(argv[i], "--speed")) {
			speed = 1;
			assert(!latency);
		} else if (!strcmp(argv[i], "--latency")) {
			latency = 1;
			assert(!speed);
		} else if (!strcmp(argv[i], "--do-copy")) {
			do_copy = 1;
			assert(latency);
		} else if (!strcmp(argv[i], "--tx-rings")) {
			++i;
			assert(i < argc);
			tx_rings = atoi(argv[i]);
			assert(speed);
		}
	}
	assert(speed || latency);

	/* Initialize the Environment Abstraction Layer (EAL). */
	int ret = rte_eal_init(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Error with EAL initialization\n");

	argc -= ret;
	argv += ret;

	/* Check that there is an even number of ports to send/receive on. */
	nb_ports = rte_eth_dev_count_avail();
	// if (nb_ports < 2 || (nb_ports & 1))
	// 	rte_exit(EXIT_FAILURE, "Error: number of ports must be even\n");

	/* Creates a new mempool in memory to hold the mbufs. */
	mbuf_pool = rte_pktmbuf_pool_create("MBUF_POOL", NUM_MBUFS * nb_ports,
		MBUF_CACHE_SIZE, 0, 10000 /*RTE_MBUF_DEFAULT_BUF_SIZE*/, rte_socket_id());

	if (mbuf_pool == NULL)
		rte_exit(EXIT_FAILURE, "Cannot create mbuf pool\n");

	/* Initialize all ports. */
	// RTE_ETH_FOREACH_DEV(portid)
		if (port_init(portid, mbuf_pool, tx_rings * (rte_lcore_count() - 1)) != 0)
			rte_exit(EXIT_FAILURE, "Cannot init port %"PRIu16 "\n",
					portid);

	pthread_mutex_init(&mutex, NULL);

	uint16_t lcore_id;
	if (client) {
		if (speed) {
			RTE_LCORE_FOREACH_SLAVE(lcore_id) {
				struct send_data* sd = malloc(sizeof(struct send_data));
				sd->portid = portid;
				sd->mbuf_pool = mbuf_pool;
				sd->dest_ip = dest_ip;
				sd->src_ip = ip;
				sd->packet_size = packet_size;
				sd->data_to_send = data_to_send;
				sd->do_copy = do_copy;
				sd->tx_rings = tx_rings;
				rte_eal_remote_launch(lcore_main_send_speed, sd, lcore_id);
			}
			RTE_LCORE_FOREACH_SLAVE(lcore_id) {
				rte_eal_wait_lcore(lcore_id);
			}
		} else {
			struct send_data* sd = malloc(sizeof(struct send_data));
			sd->portid = portid;
			sd->mbuf_pool = mbuf_pool;
			sd->dest_ip = dest_ip;
			sd->src_ip = ip;
			sd->packet_size = packet_size;
			sd->data_to_send = data_to_send;
			sd->do_copy = do_copy;
			sd->tx_rings = tx_rings;
			lcore_main_send_latency(sd);
		}
	} else {
		struct send_data* sd = malloc(sizeof(struct send_data));
		sd->portid = portid;
		sd->mbuf_pool = mbuf_pool;
		sd->dest_ip = dest_ip;
		sd->src_ip = ip;
		sd->packet_size = packet_size;
		sd->data_to_send = data_to_send;
		sd->do_copy = do_copy;
		sd->tx_rings = tx_rings;
		lcore_main_flyback(sd);
	}

	pthread_mutex_destroy(&mutex);

	return 0;
}
