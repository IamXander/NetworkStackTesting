#include "../dpdk-common/dpdk_common.h"
#include <inttypes.h>
#include <stdlib.h>

#include <time.h>

#include <pthread.h>

#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_cycles.h>
#include <rte_lcore.h>
#include <rte_ether.h>
#include <rte_arp.h>
#include <rte_ip.h>
#include <rte_sched.h>

#define MBUF_CACHE_SIZE 250

#define MAX_GENERATORS 10

struct generatorConfig_t {
	int64_t packetsToGenerate;
	int64_t packetSize;
	int64_t queueSize;
	int64_t burstSize;
	int64_t packetsToDequeue; //Max packets to try and dequeue per operation on QoS thread
	struct rte_ring * ring;
};

struct rte_mempool *mbuf_pool;
struct rte_sched_port* scheduler;

static int lcore_main_generate_packets(void * gc) {
	struct generatorConfig_t* generatorConfig = (struct generatorConfig_t*) gc;
	int64_t packetsSent = 0;

	struct timespec start;
	int rc = clock_gettime(CLOCK_REALTIME, &start);
	if (rc != 0) {
		printf("%s\n", strerror(errno));
	}

	while (packetsSent < generatorConfig->packetsToGenerate) {
		struct rte_mbuf *bufs[generatorConfig->burstSize];
		int rc = rte_pktmbuf_alloc_bulk(mbuf_pool, bufs, generatorConfig->burstSize);
		assert(rc == 0);

		for (int i = 0; i < generatorConfig->burstSize; i++) {
			struct rte_mbuf * buf = bufs[i];
			buf->data_len = generatorConfig->packetSize;
			buf->pkt_len = generatorConfig->packetSize;
		}
		packetsSent += generatorConfig->burstSize;

		uint16_t nb_tx = 0;
		do {
			nb_tx += rte_ring_sp_enqueue_burst(generatorConfig->ring, bufs + nb_tx, generatorConfig->burstSize - nb_tx, NULL);
		} while (nb_tx < generatorConfig->burstSize);
	}
	struct timespec end;
	rc = clock_gettime(CLOCK_REALTIME, &end);
	if (rc != 0) {
		printf("%s\n", strerror(errno));
	}
	time_t sec = end.tv_sec - start.tv_sec;
	long nanos = end.tv_nsec - start.tv_nsec;
	double seconds = ((double) sec) + ((double)nanos / 1000000000.0);

	printf("core: %d, seconds: %f, packet size: %ld, burst size: %ld, packets sent: %ld\n", rte_lcore_id(), seconds, generatorConfig->packetSize, generatorConfig->burstSize, packetsSent);
	return 0;
}

static void lcore_main_QOS(struct generatorConfig_t* generatorConfigs, int64_t numGenerators, int64_t numQoSDequeue) {
	int64_t packetsRecved[MAX_GENERATORS];
	memset(packetsRecved, 0, sizeof(int64_t) * MAX_GENERATORS);
	int morePackets = 1;
	while (morePackets) {
		morePackets = 0;
		for (int64_t i = 0; i < numGenerators; ++i) {
			if (packetsRecved[i] >= generatorConfigs[i].packetsToGenerate) continue;
			morePackets = 1;
			struct rte_mbuf *bufs[generatorConfigs[i].packetsToDequeue];
			uint16_t nb_tx = 0;
			do {
				nb_tx += rte_ring_sc_dequeue_burst(generatorConfigs[i].ring, bufs + nb_tx, generatorConfigs[i].packetsToDequeue - nb_tx, NULL);
			} while (nb_tx < generatorConfigs[i].packetsToDequeue);
			packetsRecved[i] += nb_tx;

			if (numQoSDequeue == -1) { //No QoS
				for (int j = 0; j < generatorConfigs[i].packetsToDequeue; ++j) {
					rte_pktmbuf_free(bufs[j]);
				}
			} else {

			}
		}
	}
}

int main(int argc, char *argv[]) {
	/* Initialize the Environment Abstraction Layer (EAL). */
	int ret = rte_eal_init(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Error with EAL initialization\n");

	argc -= ret;
	argv += ret;

	int64_t numGenerators = 0;
	int64_t totalQueueSize = 0;
	int64_t totalDequeuePackets = 0;
	int64_t numQoSDequeue = -1;
	struct generatorConfig_t generatorConfigs[MAX_GENERATORS];
	char ringName[20];
	strcpy(ringName, "ring");
	for (int i = 0; i < argc; i++) {
		if (!strcmp(argv[i], "-g")) {
			assert(i + 5 < argc);
			assert(numGenerators + 1 < MAX_GENERATORS);
			generatorConfigs[numGenerators].packetsToGenerate = atol(argv[++i]);
			generatorConfigs[numGenerators].packetSize = atol(argv[++i]);
			generatorConfigs[numGenerators].queueSize = atol(argv[++i]);
			generatorConfigs[numGenerators].burstSize = atol(argv[++i]);
			generatorConfigs[numGenerators].packetsToDequeue = atol(argv[++i]);
			sprintf(ringName + 4, "%d", i);
			generatorConfigs[numGenerators].ring =
				rte_ring_create(ringName, generatorConfigs[numGenerators].queueSize, rte_socket_id(), RING_F_SP_ENQ | RING_F_SC_DEQ); //Single producer, single consumer
			if (generatorConfigs[numGenerators].ring == NULL)
				rte_exit(EXIT_FAILURE, "Could not create ring %ld\n", numGenerators);
			totalQueueSize += generatorConfigs[numGenerators].queueSize;
			totalDequeuePackets += generatorConfigs[numGenerators].packetsToDequeue;
			++numGenerators;
		} else if (!strcmp(argv[i], "-d")) {
			++i;
			assert(i < argc);
			numQoSDequeue = atol(argv[i]);
		}
	}

	if (numGenerators == 0) {
		rte_exit(EXIT_FAILURE, "Must use at least one generator\n");
	}

	if (rte_lcore_count() != numGenerators + 1) {
		rte_exit(EXIT_FAILURE, "Must uses %ld cores\n", numGenerators);
	}

	if (numQoSDequeue == -1) printf("No QoS\n");
	else if (numQoSDequeue == 0) numQoSDequeue = totalDequeuePackets;
	else printf("Packets to QoS dequeue: %ld\n", numQoSDequeue);

	/* Creates a new mempool in memory to hold the mbufs. */
	// int64_t powOf2 = 8048;
	// while (powOf2 - 1 < totalQueueSize) powOf2 *= powOf2;
	mbuf_pool = rte_pktmbuf_pool_create("MBUF_POOL", 8191,
		MBUF_CACHE_SIZE, 0, RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());

	if (mbuf_pool == NULL) rte_exit(EXIT_FAILURE, "Cannot create mbuf pool\nError: %s\n", rte_strerror(rte_errno));

	// static struct rte_sched_pipe_params pipe_profiles[RTE_SCHED_PIPE_PROFILES_PER_PORT] = {
	// 	{ /* Profile #0 */
	// 		.tb_rate = 305175,
	// 		.tb_size = 1000000,
	// 		.tc_rate = {305175, 305175, 305175, 305175},
	// 		.tc_period = 40,
	// 		.wrr_weights = {1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1},
	// 	},
	// };
	// struct rte_sched_port_params port_params = {
	// 	.name = "port_scheduler_0",
	// 	.socket = rte_socket_id(),
	// 	.rate = 0, /* computed */
	// 	.mtu = 6 + 6 + 4 + 4 + 2 + 1500,
	// 	.frame_overhead = RTE_SCHED_FRAME_OVERHEAD_DEFAULT,
	// 	.n_subports_per_port = 1,
	// 	.n_pipes_per_subport = 4096,
	// 	.qsize = {64, 64, 64, 64},
	// 	.pipe_profiles = pipe_profiles,
	// 	.n_pipe_profiles = sizeof(pipe_profiles) / sizeof(struct rte_sched_pipe_params),
	// };

	// scheduler = rte_sched_port_config(&port_params);
	// if (scheduler == NULL){
	// 	rte_exit(EXIT_FAILURE, "Unable to config sched port\n");
	// }

	int coreNum = 0;
	int lcore_id;
	RTE_LCORE_FOREACH_SLAVE(lcore_id) {
		rte_eal_remote_launch(lcore_main_generate_packets, &generatorConfigs[coreNum++], lcore_id);
	}

	lcore_main_QOS(generatorConfigs, numGenerators, numQoSDequeue);

	// RTE_LCORE_FOREACH_SLAVE(lcore_id) {
	// 	rte_eal_wait_lcore(lcore_id);
	// }

	return 0;
}
