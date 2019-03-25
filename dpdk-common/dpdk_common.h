#ifndef DPDK_COMMON_H
#define DPDK_COMMON_H

#include <stdint.h>
#include <assert.h>

#include <arpa/inet.h>

#include <rte_mbuf.h>
#include <rte_ether.h>
#include <rte_arp.h>
#include <rte_ethdev.h>

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

#endif