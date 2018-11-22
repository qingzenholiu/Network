#ifndef __NAT_H__
#define __NAT_H__

#include "types.h"
#include "base.h"
#include "list.h"

#include "hash.h"

#include <time.h>
#include <pthread.h>

#define NAT_PORT_MIN	12345			// the lower bound of port range used for translation
#define NAT_PORT_MAX	23456			// the upper bound of port range used for translation

#define TCP_ESTABLISHED_TIMEOUT	60		// if the tcp connection does not transmit any packet 
                                        // in 60 seconds, it is regarded as finished

// DIR_IN is direction that packet from public network to private network, 
// DIR_OUT is direction that packet from private network to public network
enum packet_dir { DIR_IN = 1, DIR_OUT, DIR_INVALID };

struct nat_connection {
	u8 internal_fin;		// indicates whether received fin packet from inner node
	u8 external_fin;		// indicates whether received fin packet from outer node

	u32	internal_seq_end;	// the highest sequence number transmitted by inner node
	u32	external_seq_end;	// the highest sequence number transmitted by outer node

	u32 internal_ack;		// the highest sequence number acked by inner node
	u32 external_ack;		// the highest sequence number acked by outer node
};

// the mapping entry used for address translation
struct nat_mapping {
	struct list_head list;

	u32 internal_ip;		// ip address seen in private network
	u32 external_ip;		// ip address seen in public network (the ip address of external interface)
	u16 internal_port;		// port seen in private network
	u16 external_port;		// port seen in public network (assigned by nat)

	time_t update_time;		// when receiving the latest packet
	struct nat_connection conn;	// statistics of the tcp connection
};

struct nat_table {
	struct list_head nat_mapping_list[HASH_8BITS];	// nat hash table

	iface_info_t *internal_iface;		// pointer to internal interface
	iface_info_t *external_iface;		// pointer to external interface

	u8 assigned_ports[65536];			// port pool, indicates whether a port is assigned

	pthread_mutex_t lock;				// each nat operation should apply the lock first
	pthread_t thread;					// thread id of nat timeout
};

void nat_table_init();
void nat_table_destroy();
void nat_translate_packet(iface_info_t *iface, char *packet, int len);
void *nat_timeout();

// static int get_packet_direction(char *packet);
// int assign_external_port();
// u8 my_hash(u32 ip, u16 port);
// void update_ip_tcp(char *packet, u32 ip_addr, u16 port, int dir);
// void update_conn(struct nat_mapping *p, struct tcphdr *tcp);
// void do_translation(iface_info_t *iface, char *packet, int len, int dir);


#endif
