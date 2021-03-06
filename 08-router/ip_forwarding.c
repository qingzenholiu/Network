#include "ip.h"
#include "icmp.h"
#include "rtable.h"
#include "arp.h"
#include "arpcache.h"

#include <stdio.h>
#include <stdlib.h>

// forward the IP packet from the interface specified by longest_prefix_match, 
// when forwarding the packet, you should check the TTL, update the checksum,
// determine the next hop to forward the packet, then send the packet by 
// iface_send_packet_by_arp
void ip_forward_packet(u32 ip_dst, char *packet, int len)
{
    fprintf(stderr, "TODO: forward ip packet.\n");
    rt_entry_t *p = NULL;
    p = longest_prefix_match(ip_dst);
    if(!p){
        icmp_send_packet(packet, len, ICMP_DEST_UNREACH, ICMP_NET_UNREACH);
        return;
    }
    struct iphdr *ip = packet_to_ip_hdr(packet);
    
    //check ttl
    // ip->ttl = htonl(ntohl(ip->ttl) - 1);
    u8 ttl = htonl(ntohl(ip->ttl) - 1);
    if(ttl <= 0){
        //drop packet & reply icmp
        icmp_send_packet(packet, len, ICMP_TIME_EXCEEDED, ICMP_EXC_TTL);
        return;
    }

    ip->ttl = ttl;

    //update checknum
    ip->checksum = ip_checksum(ip);

    //determin next hop
    u32 next_hop = p->gw;
    if(!next_hop)
        next_hop = ip_dst;

    iface_send_packet_by_arp(p->iface, next_hop, packet, len);

}

// // handle ip packet
// //
// // If the packet is ICMP echo request and the destination IP address is equal to
// // the IP address of the iface, send ICMP echo reply; otherwise, forward the
// // packet.
void handle_ip_packet(iface_info_t *iface, char *packet, int len)
{
    struct iphdr *ip = packet_to_ip_hdr(packet);
    struct icmphdr *icmp_hr = (struct icmphdr*)((char *)ip + IP_HDR_SIZE(ip));
    u32 daddr = ntohl(ip->daddr);
    if (daddr == iface->ip && icmp_hr->type == ICMP_ECHOREQUEST) {
        fprintf(stderr, "TODO: reply to the sender if it is ping packet.\n");
        icmp_send_packet(packet, len, ICMP_ECHOREPLY, ICMP_NET_UNREACH);
        free(packet);
    }
    else {
        ip_forward_packet(daddr, packet, len);
    }
}
