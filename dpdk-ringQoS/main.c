#include "../dpdk-common/dpdk_common.h"
#include "../dpdk-common/ll.h"

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
#include <rte_meter.h>

#define MBUF_CACHE_SIZE 250

#define MAX_GENERATORS 10

struct generatorConfig_t {
	int64_t packetsToGenerate;
	int64_t packetSize;
	int64_t queueSize;
	int64_t burstSize;
	int64_t packetsToDequeue; //Max packets to try and dequeue per operation on QoS thread
	struct rte_ring * ring;

	uint32_t subport;
	uint32_t pipe;
	uint32_t traffic_class;
	uint32_t queue;
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
			rte_sched_port_pkt_write(scheduler, buf, generatorConfig->subport, generatorConfig->pipe, generatorConfig->traffic_class, generatorConfig->queue, e_RTE_METER_GREEN);
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

static void lcore_main_QOS(struct generatorConfig_t* generatorConfigs, int64_t numGenerators, int64_t numQoSDequeue, uint64_t sleeptime) {
	int64_t packetsRecved[MAX_GENERATORS];
	int64_t packetsDropped[MAX_GENERATORS];
	memset(packetsDropped, 0, sizeof(packetsDropped));
	memset(packetsRecved, 0, sizeof(int64_t) * MAX_GENERATORS);
	int morePackets = 1;
	int64_t totalDeq = 0;
	int64_t totalPackets = 0;
	for (int64_t i = 0; i < numGenerators; ++i) {
		totalPackets += generatorConfigs[i].packetsToGenerate;
	}
	struct timespec start;
	int rc = clock_gettime(CLOCK_REALTIME, &start);
	if (rc != 0) {
		printf("%s\n", strerror(errno));
	}
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
				int numEnqueue = 0;
				// do {
				numEnqueue += rte_sched_port_enqueue(scheduler, bufs, generatorConfigs[i].packetsToDequeue);
				packetsDropped[i] += generatorConfigs[i].packetsToDequeue - numEnqueue;
				if (sleeptime > 0) nanosleep((const struct timespec[]){{0, sleeptime}}, NULL);
				// } while (numEnqueue < generatorConfigs[i].packetsToDequeue);
			}
		}

		if (numQoSDequeue != -1) {
			struct rte_mbuf *bufs[numQoSDequeue];
			int numDequeue = rte_sched_port_dequeue(scheduler, bufs, numQoSDequeue);
			for (int j = 0; j < numDequeue; ++j) {
				rte_pktmbuf_free(bufs[j]);
			}
			totalDeq += numDequeue;
		}
	}
	struct timespec end;
	rc = clock_gettime(CLOCK_REALTIME, &end);
	if (rc != 0) {
		printf("%s\n", strerror(errno));
	}
	time_t sec = end.tv_sec - start.tv_sec;
	long nanos = end.tv_nsec - start.tv_nsec;
	double seconds = ((double) sec) + ((double)nanos / 1000000000.0);

	printf("@@@DATA@@@\n"); //Marker for python!
	int64_t totalPacketsDropped = 0;
	for (int i = 0; i < numGenerators; i++) {
		totalPacketsDropped += packetsDropped[i];
		printf("Core %d: dropped/survived/total - %ld/%ld/%ld\n", i, packetsDropped[i], generatorConfigs[i].packetsToGenerate - packetsDropped[i], generatorConfigs[i].packetsToGenerate);
	}
	printf("Total packets dequeued: %ld\n", totalDeq);
	printf("Total packets dropped on enqueue: %ld\n", totalPacketsDropped);
	printf("Total packets left on queue: %ld\n", totalPackets - (totalDeq + totalPacketsDropped));
	printf("Duration (seconds): %f\n", seconds);
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

	uint32_t rate;
	uint32_t mtu;
	uint32_t n_subports_per_port;
	uint32_t n_pipes_per_subport;
	uint16_t qsize[RTE_SCHED_TRAFFIC_CLASSES_PER_PIPE];
	int pipeProfiles = 0;
	uint64_t sleeptime;

	struct ll* subports = new_ll();
	struct ll* pipeProfs = new_ll();
	for (int i = 0; i < argc; i++) {
		if (!strcmp(argv[i], "-g")) {
			assert(i + 10 < argc);
			assert(numGenerators + 1 < MAX_GENERATORS);
			generatorConfigs[numGenerators].packetsToGenerate = atol(argv[++i]);
			generatorConfigs[numGenerators].packetSize = atol(argv[++i]);
			generatorConfigs[numGenerators].queueSize = atol(argv[++i]);
			generatorConfigs[numGenerators].burstSize = atol(argv[++i]);
			generatorConfigs[numGenerators].packetsToDequeue = atol(argv[++i]);

			generatorConfigs[numGenerators].subport = atol(argv[++i]);
			generatorConfigs[numGenerators].pipe = atol(argv[++i]);
			generatorConfigs[numGenerators].traffic_class = atol(argv[++i]);
			generatorConfigs[numGenerators].queue = atol(argv[++i]);

			sprintf(ringName + 4, "%d", numGenerators);
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
		} else if (!strcmp(argv[i], "-st")) {
			++i;
			assert(i < argc);
			sleeptime = atol(argv[i]);
		} else if (!strcmp(argv[i], "-c")) {
			assert(i + 4 + RTE_SCHED_TRAFFIC_CLASSES_PER_PIPE < argc);
			rate = atol(argv[++i]);
			mtu = atol(argv[++i]);
			n_subports_per_port = atol(argv[++i]);
			n_pipes_per_subport = atol(argv[++i]);
			for (int j = 0; j < RTE_SCHED_TRAFFIC_CLASSES_PER_PIPE; j++) {
				qsize[j] = atol(argv[++i]);
			}
		} else if (!strcmp(argv[i], "-s")) {
			struct rte_sched_subport_params* subport_params = malloc(sizeof(struct rte_sched_subport_params));
			assert(i + 3 + 4 < argc);
			subport_params->tb_rate = atol(argv[++i]);
			subport_params->tb_size = atol(argv[++i]);
			for (int j = 0; j < 4; j++) {
				subport_params->tc_rate[j] = atol(argv[++i]);
			}
			subport_params->tc_period = atol(argv[++i]);
			add_ll(subports, subport_params);
		} else if (!strcmp(argv[i], "-p")) {
			struct rte_sched_pipe_params* pipe_params = malloc(sizeof(struct rte_sched_pipe_params));
			assert(i + 3 + 4 + 16 < argc);
			pipe_params->tb_rate = atol(argv[++i]);
			pipe_params->tb_size = atol(argv[++i]);
			for (int j = 0; j < 4; j++) {
				pipe_params->tc_rate[j] = atol(argv[++i]);
			}
			pipe_params->tc_period = atol(argv[++i]);
			for (int j = 0; j < 16; j++) {
				pipe_params->wrr_weights[j] = atol(argv[++i]);
			}
			pipeProfiles++;
			add_ll(pipeProfs, pipe_params);
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


	if (numQoSDequeue != -1) {
		struct rte_sched_pipe_params pipe_profiles[pipeProfiles];
		for (int i = 0; i < pipeProfiles; i++) {
			struct rte_sched_pipe_params* pipe_params = pop_ll(&pipeProfs);
			assert(pipe_params);
			memcpy(&pipe_profiles[i], pipe_params, sizeof(struct rte_sched_pipe_params));
			free(pipe_params);
		}
		struct rte_sched_port_params port_params = {
			.name = "port_scheduler_0",
			.socket = rte_socket_id(),
			.rate = rate, //1Gbps
			.mtu = mtu,
			.frame_overhead = RTE_SCHED_FRAME_OVERHEAD_DEFAULT,
			.n_subports_per_port = n_subports_per_port,
			.n_pipes_per_subport = n_pipes_per_subport,
			.pipe_profiles = pipe_profiles,
			.n_pipe_profiles = pipeProfiles,
		};
		memcpy(port_params.qsize, qsize, sizeof(uint16_t) * RTE_SCHED_TRAFFIC_CLASSES_PER_PIPE);

		scheduler = rte_sched_port_config(&port_params);
		if (scheduler == NULL){
			rte_exit(EXIT_FAILURE, "Unable to config sched port\nError: %s\n", rte_strerror(rte_errno));
		}

		while (1) {
			struct rte_sched_subport_params* subport_params = pop_ll(&subports);
			// fprintf(stderr, "HERE\n");
			if (subport_params == NULL) break;
			int err = rte_sched_subport_config(scheduler, 0, subport_params);
			if (err != 0) {
				rte_exit(EXIT_FAILURE, "Unable to config sched subport\nError: %d\n", err);
			}
			free(subport_params);
		}
	}

	int coreNum = 0;
	int lcore_id;
	RTE_LCORE_FOREACH_SLAVE(lcore_id) {
		rte_eal_remote_launch(lcore_main_generate_packets, &generatorConfigs[coreNum++], lcore_id);
	}

	lcore_main_QOS(generatorConfigs, numGenerators, numQoSDequeue, sleeptime);

	// RTE_LCORE_FOREACH_SLAVE(lcore_id) {
	// 	rte_eal_wait_lcore(lcore_id);
	// }

	return 0;
}
