/**********************************************************************
 * file:  sr_router.c
 * date:  Mon Feb 18 12:50:42 PST 2002
 * Contact: casado@stanford.edu
 *
 * Description:
 *
 * This file contains all the functions that interact directly
 * with the routing table, as well as the main entry method
 * for routing. 11
 * 90904102
 **********************************************************************/

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>


#include "sr_if.h"
#include "sr_rt.h"
#include "sr_router.h"
#include "sr_protocol.h"
#include "sr_arp_cache.h"

struct ip* get_ip_header(uint8_t* packet){
    return (struct ip*)(packet+sizeof(struct sr_ethernet_hdr));
}
struct sr_arphdr* get_arp_header(uint8_t* packet){
    return (struct sr_arphdr *)(packet+sizeof(struct sr_ethernet_hdr));
}
/*---------------------------------------------------------------------
 * Method: sr_init(void)
 * Scope:  Global
 *
 * Initialize the routing subsystem
 *
 *---------------------------------------------------------------------*/

void sr_init(struct sr_instance* sr)
{
    /* REQUIRES */
    assert(sr);
    sr -> ac = malloc(sizeof(arp_cache));
    sr -> ac -> requests = NULL;
    sr -> ac -> mappings = NULL;

    /* Add initialization code here! */

} /* -- sr_init -- */


/*
 Create arp reply packet header
 */
void arp_back_header_create(struct sr_arphdr * arp_header,
                            struct sr_arphdr * new_arp_header,
                            struct sr_if *rc){
    
    new_arp_header->ar_hrd = arp_header->ar_hrd;
    new_arp_header->ar_pro = arp_header->ar_pro;
    new_arp_header->ar_hln = arp_header->ar_hln;
    new_arp_header->ar_pln = arp_header->ar_pln;
    
    new_arp_header->ar_op = htons(ARP_REPLY);
    new_arp_header->ar_tip = arp_header->ar_sip;
    new_arp_header->ar_sip = arp_header->ar_tip;
    memcpy(new_arp_header->ar_tha, arp_header->ar_sha, ETHER_ADDR_LEN);
    memcpy(new_arp_header->ar_sha, rc->addr, ETHER_ADDR_LEN);
    return;
}

/*---------------------------------------------------------------------
 * Method: search new router (gateway) in list of router
 * Using bitwise & to calculate the result
 *
 * ---------------------------------------------------------------------*/
struct sr_rt* look_router(struct sr_rt* rt, uint32_t ip){
    struct sr_rt* rc = NULL;
    struct sr_rt* copy = rt;
    while(copy!=NULL){
        uint32_t destination_ip = copy-> dest.s_addr;
        uint32_t mask = copy->mask.s_addr;
        // defualt router or other routers
        if ((destination_ip & mask) == (ip & mask)){
            if(rc ==NULL){
                
                rc = copy;
                printf("!!!!!!!!!!!!!!!!!112th %s\n",rc->interface);
            }
            else if(copy->mask.s_addr > rc->mask.s_addr ){
               // printf("114th\n");
                printf("%d %d %d\n",mask,ip,destination_ip);
                rc = copy;
                printf("!!!!!!!!!!!!!!!!!117th %s\n",rc->interface);
            }
        }
        copy = copy -> next;
        
    }
    if (rc ==NULL){
        printf("122th\n");
    }
    return rc;
}

/*---------------------------------------------------------------------
 * Method: copy_ip_packet
 * return a new ip_packet
 *
 * ---------------------------------------------------------------------*/
 uint8_t* copy_ip_packet(int ip_len,struct ip* ip_head,uint8_t* packet,unsigned int total_length){
     int h_l = ip_head->ip_hl * 4;
     uint8_t* rc = malloc(sizeof(uint8_t)*ip_len);
     // copy length
     memcpy(rc,ip_head,h_l);
     // copy data
     memcpy(rc+h_l,packet+sizeof(struct sr_ethernet_hdr ) + h_l,total_length-sizeof(struct sr_ethernet_hdr )- h_l);
     return rc;
 }
/*---------------------------------------------------------------------
 * Method: ethernet_packet_create
 *  return a new ethernet frame packet
 *
 * ---------------------------------------------------------------------*/
uint8_t *ethernet_packet_create(uint8_t *host_source,
                                uint16_t type,
                                uint8_t *packet,
                                size_t length,
                                uint8_t *host_destination){
    struct sr_ethernet_hdr ethernet_header;
    memcpy(ethernet_header.ether_dhost, host_destination, ETHER_ADDR_LEN);
    memcpy(ethernet_header.ether_shost, host_source, ETHER_ADDR_LEN);
    ethernet_header.ether_type = htons(type);
    uint8_t *buffer = malloc(sizeof(struct sr_ethernet_hdr) + length);
    memcpy(buffer, &ethernet_header, sizeof(struct sr_ethernet_hdr));
    memcpy(buffer+sizeof(struct sr_ethernet_hdr), packet, length);
    return buffer;
}
/*---------------------------------------------------------------------
* Method: bordercast()
* Broadcasts an ARP request and send it as 0xFFFFFF
* ---------------------------------------------------------------------*/
void bordercast(struct sr_instance *sr, struct sr_if *rc, struct in_addr destination){
    struct sr_arphdr new_arp_header;
    
    
    new_arp_header.ar_hrd = htons(ARPHDR_ETHER);
    new_arp_header.ar_pro = htons(ETHERTYPE_IP);
    new_arp_header.ar_hln = ETHER_ADDR_LEN;
    new_arp_header.ar_pln = 4;
    new_arp_header.ar_op = htons(ARP_REQUEST);
    
    memcpy(new_arp_header.ar_sha, rc->addr, ETHER_ADDR_LEN);
    bzero(new_arp_header.ar_tha, ETHER_ADDR_LEN);
    new_arp_header.ar_sip = rc->ip;
    new_arp_header.ar_tip = destination.s_addr;

    uint8_t bc[ETHER_ADDR_LEN];
    memset(bc, 0xFF,ETHER_ADDR_LEN);
    uint8_t *buffer  = ethernet_packet_create(rc->addr, ETHERTYPE_ARP, (uint8_t *)&new_arp_header, sizeof(struct sr_arphdr), bc);
   
    sr_send_packet(sr, buffer, sizeof(struct sr_ethernet_hdr)+ sizeof (struct sr_arphdr), rc->name);
    
}

/*---------------------------------------------------------------------
 * Method: deal_with_IP(1,2,3,4)
 * get ip header and then follow the IP logic
 * firstly we need to get ip header
 * and we reconstruct paocket with ethernet frame
 * lastly we send packet to next hop(routering table search)
 * ---------------------------------------------------------------------*/

void deal_with_IP (uint8_t * packet,struct sr_if* rc ,unsigned int len, struct sr_instance* sr){
     struct ip* ip_head =  get_ip_header(packet);
     uint32_t destination_ip = ip_head ->ip_dst.s_addr;
     //struct sr_if* face = sr_get_interface(sr,interface);
     if (rc->ip == destination_ip){
         return;
     }
     struct sr_rt* router =  look_router(sr->routing_table,destination_ip);
     
     if(router !=NULL){
         printf("253th\n");
         printf("296th------------------------%s\n",router-> interface);
         struct sr_if* gateway_face = sr_get_interface(sr,router->interface);
         printf("255th\n");
         struct in_addr next_hop;
         if( router->gw.s_addr != 0){
              printf("258th\n");
             next_hop = router->gw;
         }else{
            
              printf("262th\n");
             next_hop = ip_head->ip_dst;
         }
     
        // todo construct a packet in here and pass it as second argument in add_req;
        uint8_t* new_packet = copy_ip_packet(len-sizeof(struct sr_ethernet_hdr),ip_head,packet,len);
        printf("268th\n");
        sr_m* rv = look_up(sr->ac, next_hop.s_addr);
        printf("270th\n");
        if(rv!=NULL){
            uint8_t *b = ethernet_packet_create(gateway_face->addr, ETHERTYPE_IP, new_packet, len - sizeof(struct sr_ethernet_hdr), rv->mac_addr);
            printf("274th\n");
            sr_send_packet(sr, b, len, gateway_face->name);
            printf("276th\n");
        }else{
             // we need to add arp request and send it as a arp_request
            printf("279th\n");
            // if we don't have the mac, we need to add req
            add_req (next_hop.s_addr,new_packet,sr->ac,len-sizeof(struct sr_ethernet_hdr), router->interface);
            printf("281th\n");
            bordercast(sr, gateway_face, next_hop);
            printf("284th\n");
        }
     }

}

/*---------------------------------------------------------------------
 * Method: deal_with_ARP(1,2,3,4)
 * get ip header and then follow the ARP logic
 * firstly we need to get ARP header
 * ---------------------------------------------------------------------*/
void deal_with_ARP (uint8_t * packet,struct sr_if* rc ,unsigned int len, struct sr_instance* sr){
    assert(sr);
    assert(packet);
    
    //get ethernet header
    struct sr_ethernet_hdr* eth_header = (struct sr_ethernet_hdr*)packet;
    if(eth_header == NULL){
        printf("ethernet header is null\n");
        return;
    }
    
    // get arp header
    struct sr_arphdr* arp_header = (struct sr_arphdr *)get_arp_header(packet);
    if(arp_header  == NULL){
        printf("arp header is null\n");
        return;
    }
    
    // check length
    if(len < arp_packet_length){
        fprintf(stderr, "length of arp packet is short than normal");
        return;
    }
    
    // request or reply
    unsigned short ar_op = ntohs(arp_header->ar_op);
    printf("307th\n");
    if(rc){
        // arp request
        // need to construct arp reply and send it back
        if(ar_op == ARP_REQUEST){
            printf("We got a arp request in here\n");
            // set length of back packet
            // makesure we are the rightone to send reply
            if (arp_header->ar_tip != rc->ip){
                printf("we are not destination\n");
                return;
            }
            int LengthOfPacket = arp_packet_length;
            uint8_t *arp_reply_header = (uint8_t *)malloc(LengthOfPacket);
            printf("320th\n");

            // create arp header
            arp_back_header_create(arp_header, (struct sr_arphdr*) arp_reply_header,rc
                                   );
            uint8_t* new_packet = ethernet_packet_create(rc->addr,
                                ETHERTYPE_ARP,
                                arp_reply_header,
                                sizeof(struct sr_arphdr),
                                eth_header->ether_shost);
            // send arp reply
            printf("328th\n");
            sr_send_packet(sr,new_packet,sizeof(struct sr_ethernet_hdr) + sizeof(struct sr_arphdr),rc->name);
            printf("330th\n");
            return;
            
        // arp reply
        // send packet which has been saved before
        }else if (ar_op == ARP_REPLY){
            printf("We got a arp reply in here\n");
            sr_r* match_req =  look_insert(arp_header->ar_sha ,arp_header->ar_sip , sr->ac);
            if (match_req != NULL){
                printf("368th\n");
                sr_p* packet = match_req ->packet;
                while(packet!=NULL){
                    uint8_t* new_packet =  ethernet_packet_create(rc->addr,
                                ETHERTYPE_IP,
                                packet->frame,
                                packet->len,
                                arp_header->ar_sha);
                    printf("393th\n");
                    printf("395---------------%d  %ld\n", packet->len,len-sizeof(struct sr_ethernet_hdr));
                    sr_send_packet(sr,new_packet , sizeof(struct sr_ethernet_hdr)+ packet->len,rc->name);
                    packet = packet->next;
                    printf("395th\n");

                }
            }
        }
        // error
        else{
            fprintf(stderr, "wrong arp type \n");
            return;
        }
    }else{
        fprintf(stderr, "no this interface in router");
        return;
    }
}

/*---------------------------------------------------------------------
 * Method: sr_handlepacket(uint8_t* p,char* interface)
 * Scope:  Global
 *
 * This method is called each time the router receives a packet on the
 * interface.  The packet buffer, the packet length and the receiving
 * interface are passed in as parameters. The packet is complete with
 * ethernet headers.
 *
 * Note: Both the packet buffer and the character's memory are handled
 * by sr_vns_comm.c that means do NOT delete either.  Make a copy of the
 * packet instead if you intend to keep it around beyond the scope of
 * the method call.
 *
 *---------------------------------------------------------------------*/

void sr_handlepacket(struct sr_instance* sr,
        uint8_t * packet/* lent */,
        unsigned int len,
        char* interface/* lent */)
{
    /* REQUIRES */
    assert(sr);
    assert(packet);
    assert(interface);
    struct sr_if* rc = sr_get_interface(sr,interface);
    struct sr_ethernet_hdr* eth_header = (struct sr_ethernet_hdr*)packet;
    uint16_t ether_type  =  ntohs(eth_header->ether_type);
    if (ether_type == ETHERTYPE_IP ){
        // if it is ip
        printf("391th\n");
        deal_with_IP(packet,rc,len,sr);
    }
    else if (ether_type == ETHERTYPE_ARP) {
        // if it is arp
        printf("396th\n");
        deal_with_ARP(packet,rc,len,sr);
        
    }
    else{
        fprintf(stdin,"Error: Neither IP or ARP");
    }
    printf("*** -> Received packet of length %d \n",len);

}/* end sr_ForwardPacket */


/*---------------------------------------------------------------------
 * Method:
 *
 *---------------------------------------------------------------------*/