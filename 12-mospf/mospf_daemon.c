#include "mospf_daemon.h"
#include "mospf_proto.h"
#include "mospf_nbr.h"
#include "mospf_database.h"

#include "ip.h"
#include "packet.h"

#include "list.h"
#include "log.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

extern ustack_t *instance;

pthread_mutex_t mospf_lock;

void mospf_init()
{
	pthread_mutex_init(&mospf_lock, NULL);

	instance->area_id = 0;
	// get the ip address of the first interface
	iface_info_t *iface = list_entry(instance->iface_list.next, iface_info_t, list);
	instance->router_id = iface->ip;
	instance->sequence_num = 0;
	instance->lsuint = MOSPF_DEFAULT_LSUINT;

	iface = NULL;
	list_for_each_entry(iface, &instance->iface_list, list) {
		iface->helloint = MOSPF_DEFAULT_HELLOINT;
		init_list_head(&iface->nbr_list);
	}

	init_mospf_db();
}

void *sending_mospf_hello_thread(void *param);
void *sending_mospf_lsu_thread(void *param);
void *checking_nbr_thread(void *param);
void send_mospf_lsu_once();
void *print_nbr(void *param);



void *print_nbr(void *param) {
    mospf_db_entry_t * p = NULL;
    while (1) {
        if (!list_empty(&mospf_db)) {

            list_for_each_entry(p, &mospf_db, list) {

                for (int i = 0; i < p->nadv; i++) {
                    fprintf(stdout, IP_FMT" \t"IP_FMT" \t"IP_FMT"\t"IP_FMT"\n",
                           HOST_IP_FMT_STR(p->rid),
                           HOST_IP_FMT_STR(p->array[i].subnet),
                           HOST_IP_FMT_STR(p->array[i].mask),
                           HOST_IP_FMT_STR(p->array[i].rid));
                }
            }
            printf("\n");
        } else
            printf("Database is now empty.\n");
        sleep(2);
    }
    return NULL;
}


void mospf_run()
{
	pthread_t hello, lsu, nbr, dump;
	pthread_create(&hello, NULL, sending_mospf_hello_thread, NULL);
	pthread_create(&lsu, NULL, sending_mospf_lsu_thread, NULL);
	pthread_create(&nbr, NULL, checking_nbr_thread, NULL);
	pthread_create(&dump, NULL, print_nbr, NULL);
}

void *sending_mospf_hello_thread(void *param)
{
	
	int len = ETHER_HDR_SIZE + IP_BASE_HDR_SIZE + MOSPF_HDR_SIZE + MOSPF_HELLO_SIZE; 
	while(1){
		fprintf(stdout, "TODO: send mOSPF Hello message periodically.\n");
		sleep(MOSPF_DEFAULT_HELLOINT);
		iface_info_t *iface;
		// printf("aaaaaaaaaaaaaaaaaa\n");
		list_for_each_entry(iface, &(instance->iface_list), list){
			char *packet = (char *)malloc(len);
			// memset(packet, 0, len);
			
			//ether header
			struct ether_header *ether_hdr = (struct ether_header *)packet;
			u8 dst_mac[ETH_ALEN] = {0x1, 0x0, 0x5E, 0x0, 0x0, 0x5};
			ether_hdr->ether_type = ntohs(ETH_P_IP);
			memcpy(ether_hdr->ether_shost, iface->mac, ETH_ALEN);
			memcpy(ether_hdr->ether_dhost, dst_mac, ETH_ALEN);		

			//ip header
			struct iphdr *ip = packet_to_ip_hdr(packet);
			ip_init_hdr(ip, iface->ip, 0xE0000005, (IP_BASE_HDR_SIZE + MOSPF_HDR_SIZE + MOSPF_HELLO_SIZE), 90);

			//mospf header
			struct mospf_hdr *mospf = (struct mospf_hdr *)((char *)ip + IP_BASE_HDR_SIZE);
			mospf_init_hdr(mospf, 1, MOSPF_HDR_SIZE + MOSPF_HELLO_SIZE, instance->router_id, 0x0);

			//mospf msg
			struct mospf_hello *hello = (struct mospf_hello *)((char*)mospf + MOSPF_HDR_SIZE);
			mospf_init_hello(hello, iface->mask);

			mospf->checksum = mospf_checksum(mospf);
			ip->checksum = ip_checksum(ip);

			iface_send_packet(iface, packet, len);


		}
		// printf("bbbbbbbbbbbbbbb\n");
		fprintf(stdout, "exit: send mOSPF Hello message periodically.\n");


	}
	return NULL;
}

void *checking_nbr_thread(void *param)
{
	fprintf(stdout, "TODO: neighbor list timeout operation.\n");
	while(1){
		sleep(1);
		pthread_mutex_lock(&mospf_lock);
		iface_info_t *iface;
		list_for_each_entry(iface, &(instance->iface_list), list){
			mospf_nbr_t *p = NULL;
			mospf_nbr_t *q = NULL;
			if(!list_empty(&(iface->nbr_list))){
				list_for_each_entry_safe(p, q, &(iface->nbr_list), list){
					p->alive++;
					if(p->alive > (3 * MOSPF_DEFAULT_HELLOINT)){
						list_delete_entry(&(p->list));
						iface->num_nbr--;
						printf("delete\n");
						send_mospf_lsu_once();
					}
				}
			}
		}


		pthread_mutex_unlock(&mospf_lock);
	}

	return NULL;
}

void handle_mospf_hello(iface_info_t *iface, const char *packet, int len)
{
	fprintf(stdout, "TODO: handle mOSPF Hello message.\n");
	
	struct iphdr *ip = (struct iphdr *)(packet + ETHER_HDR_SIZE);
	struct mospf_hdr *mospf = (struct mospf_hdr *)((char *)ip + IP_HDR_SIZE(ip));
	struct mospf_hello *hello = (struct mospf_hello *)((char*)mospf + MOSPF_HDR_SIZE);

	mospf_nbr_t *p = NULL;
	pthread_mutex_lock(&mospf_lock);
	if(!list_empty(&(iface->nbr_list))){
		list_for_each_entry(p, &(iface->nbr_list), list){
			if(p->nbr_id == ntohl(mospf->rid)){
				p->alive = 0;///////////////////////////////
				pthread_mutex_unlock(&mospf_lock);
				return;
			}
		}	
	}

	mospf_nbr_t *new = (mospf_nbr_t *)malloc(sizeof(mospf_nbr_t));
	memset((char *)new, 0, sizeof(mospf_nbr_t));

	new->nbr_id = ntohl(mospf->rid);
	new->nbr_ip = ntohl(ip->saddr);
	new->nbr_mask = ntohl(hello->mask);
	new->alive = 0;////////////////////////////

	list_add_tail(&(new->list), &(iface->nbr_list));
	iface->num_nbr ++;
	send_mospf_lsu_once();
	pthread_mutex_unlock(&mospf_lock);
	fprintf(stdout, "exit: handle mOSPF Hello message.\n");

	return;

}



void *sending_mospf_lsu_thread(void *param)
{
	while(1){
	fprintf(stdout, "TODO: send mOSPF LSU message periodically.\n");
		sleep(instance->lsuint);
		send_mospf_lsu_once();
	fprintf(stdout, "exit: handle mOSPF Hello message.\n");

	}
	return NULL;
}

void send_mospf_lsu_once(){

	iface_info_t *iface = NULL;

	// find total nbr num
	int total_nbr_num = 0;
	// struct mospf_lsa temp[100];

	if (!list_empty(&(instance->iface_list))) {
        list_for_each_entry(iface, &(instance->iface_list), list){
        	if(iface->num_nbr == 0){
        		//still count the no reply net
        		total_nbr_num ++;
        	} else {
        		total_nbr_num += iface->num_nbr;
        	}
        }
    }

	mospf_nbr_t *p = NULL;
	// struct mospf_lsa *lsa_temp = (struct mospf_lsa *)malloc(total_nbr_num * sizeof(struct mospf_lsa));
	char *temp = (char *)malloc(total_nbr_num * MOSPF_LSA_SIZE);
	struct mospf_lsa *lsa_temp = (struct mospf_lsa *)temp;

	if(!list_empty(&(instance->iface_list))){
		list_for_each_entry(iface, &(instance->iface_list), list){
			if(iface->num_nbr == 0){
				lsa_temp->subnet = htonl(iface->ip & iface->mask);
				lsa_temp->mask = htonl(iface->mask);
				lsa_temp->rid = 0;
				lsa_temp++;
				continue;
			}

			list_for_each_entry(p, &(iface->nbr_list), list){
				lsa_temp->subnet = htonl(p->nbr_ip & p->nbr_mask);
				lsa_temp->mask = htonl(p->nbr_mask);
				lsa_temp->rid = htonl(p->nbr_id);
				lsa_temp++;
				
			}
		}
	}

	int len = ETHER_HDR_SIZE + IP_BASE_HDR_SIZE + MOSPF_HDR_SIZE + MOSPF_LSU_SIZE + total_nbr_num * MOSPF_LSA_SIZE;
	mospf_nbr_t *p_temp = NULL;
	// char *a = (char *)lsa_temp;
	if(!list_empty(&(instance->iface_list))){
		list_for_each_entry(iface, &(instance->iface_list), list){
			if(!list_empty(&(iface->nbr_list))){
				list_for_each_entry(p_temp, &(iface->nbr_list), list){
					(instance->sequence_num)++;
					char *packet = (char *)malloc(len);

					// struct ether_header *ether_hdr = (struct ether_header *)packet;
					
					//ip header
					struct iphdr *ip = packet_to_ip_hdr(packet);
					ip_init_hdr(ip, iface->ip, p_temp->nbr_ip, len - ETHER_HDR_SIZE, 90);

					//mospf header
					struct mospf_hdr *mospf = (struct mospf_hdr *)((char *)ip + IP_BASE_HDR_SIZE);
					mospf_init_hdr(mospf, 4, len - ETHER_HDR_SIZE - IP_BASE_HDR_SIZE, instance->router_id, 0x0);		
			
					//mospf lsu
					struct mospf_lsu *m_lsu = (struct mospf_lsu *)((char *)mospf + MOSPF_HDR_SIZE);
					mospf_init_lsu(m_lsu, total_nbr_num);

					//mospf lsa
					struct mospf_lsa *m_lsa = (struct mospf_lsa *)((char *)m_lsu + MOSPF_LSU_SIZE);
					memcpy((char *)m_lsa, (char *)temp, total_nbr_num * MOSPF_LSA_SIZE);

					mospf->checksum = mospf_checksum(mospf);
					ip->checksum = ip_checksum(ip);

					ip_send_packet(packet, len);

				}

			}
		}
	}
	free(temp);
}

void handle_mospf_lsu(iface_info_t *iface, char *packet, int len)
{
	fprintf(stdout, "TODO: handle mOSPF LSU message.\n");

	struct iphdr *ip = (struct iphdr *)packet_to_ip_hdr(packet);
	struct mospf_hdr *mospf = (struct mospf_hdr *)((char *)ip + IP_BASE_HDR_SIZE);
	struct mospf_lsu *m_lsu = (struct mospf_lsu *)((char *)mospf + MOSPF_HDR_SIZE);
	struct mospf_lsa *m_lsa = (struct mospf_lsa *)((char *)m_lsu + MOSPF_LSU_SIZE);

	mospf_db_entry_t *p = NULL;
	int find = 0;

    pthread_mutex_lock(&mospf_lock);
	if(!list_empty(&(mospf_db))){
		list_for_each_entry(p, &mospf_db, list){
			if(p->rid == ntohl(mospf->rid)){
				find = 1;
				//update sequence number if larger
				if(p->seq < ntohs(m_lsu->seq)){
					p->seq = ntohs(m_lsu->seq);
					p->rid = ntohl(mospf->rid);
					p->nadv = ntohl(m_lsu->nadv);
					for(int i = 0; i < p->nadv; i++){
						p->array[i].subnet = ntohl(m_lsa->subnet);
						p->array[i].mask = ntohl(m_lsa->mask);
						p->array[i].rid = ntohl(m_lsa->rid);
						m_lsa = (struct mospf_lsa *)((char *)m_lsa + sizeof(struct mospf_lsa));
					}
				}
				break;
			}
		}
	}

	if(find == 0){
		//create new 
		mospf_db_entry_t *new = (mospf_db_entry_t *)malloc(sizeof(mospf_db_entry_t));
		new->seq = ntohs(m_lsu->seq);
		new->rid = ntohl(mospf->rid);
		new->nadv = ntohl(m_lsu->nadv);

		new->array = (struct mospf_lsa *)malloc(sizeof(struct mospf_lsa) * (new->nadv));
		for(int i = 0; i < new->nadv; i++){
			new->array[i].subnet = ntohl(m_lsa->subnet);
			new->array[i].mask = ntohl(m_lsa->mask);
			new->array[i].rid = ntohl(m_lsa->rid);
			m_lsa = (struct mospf_lsa *)((char *)m_lsa + sizeof(struct mospf_lsa));
		}

		list_add_tail(&(new->list), &mospf_db);
	}

	pthread_mutex_unlock(&mospf_lock);


	m_lsu->ttl--;
	//ttl > 0, forward
	if(m_lsu->ttl > 0){
		iface_info_t *iface = NULL;
		list_for_each_entry(iface, &(instance->iface_list), list){
			mospf_nbr_t *p = NULL;
			if(!list_empty(&(iface->nbr_list))){
				list_for_each_entry(p, &(iface->nbr_list), list){
					if((p->nbr_ip != ntohl(ip->saddr)) && (p->nbr_id != ntohl(mospf->rid))){
						char *packet_send = (char *)malloc(len);
						memcpy((char *)packet_send, (char *)packet, len);
						
						struct iphdr *ip_send = packet_to_ip_hdr(packet_send);
						ip_send->saddr = htonl(iface->ip);
						ip_send->daddr = htonl(p->nbr_ip);

						struct mospf_hdr *mospf_send = (struct mospf_hdr *)((char *)ip_send + IP_BASE_HDR_SIZE);
						
						mospf_send->checksum = mospf_checksum(mospf_send);
						ip_send->checksum = ip_checksum(ip_send);

						ip_send_packet(packet_send, len);
					}
				}
			}
		}
	}
	fprintf(stdout, "exit: handle mOSPF LSU message.\n");


}



void handle_mospf_packet(iface_info_t *iface, char *packet, int len)
{
	struct iphdr *ip = (struct iphdr *)(packet + ETHER_HDR_SIZE);
	struct mospf_hdr *mospf = (struct mospf_hdr *)((char *)ip + IP_HDR_SIZE(ip));

	if (mospf->version != MOSPF_VERSION) {
		log(ERROR, "received mospf packet with incorrect version (%d)", mospf->version);
		return ;
	}
	if (mospf->checksum != mospf_checksum(mospf)) {
		log(ERROR, "received mospf packet with incorrect checksum");
		return ;
	}
	if (ntohl(mospf->aid) != instance->area_id) {
		log(ERROR, "received mospf packet with incorrect area id");
		return ;
	}

	// log(DEBUG, "received mospf packet, type: %d", mospf->type);

	switch (mospf->type) {
		case MOSPF_TYPE_HELLO:
			handle_mospf_hello(iface, packet, len);
			break;
		case MOSPF_TYPE_LSU:
			handle_mospf_lsu(iface, packet, len);
			break;
		default:
			log(ERROR, "received mospf packet with unknown type (%d).", mospf->type);
			break;
	}
}
