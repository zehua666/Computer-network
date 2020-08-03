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
 Create ethernet header
 */
void ethernet_header_create(struct sr_ethernet_hdr* eth_header,
                            struct sr_ethernet_hdr *new_eth_header,
                            struct sr_if *rc){
    assert(eth_header);
    assert(new_eth_header);
    
    // swap sender and receiver ethernet address
    memcpy(new_eth_header->ether_dhost, eth_header->ether_shost, ETHER_ADDR_LEN);
    memcpy(new_eth_header->ether_shost, rc->addr, ETHER_ADDR_LEN);
    
    // type should be same with coming ethernet
    new_eth_header->ether_type = eth_header->ether_type;
    return;
}

/*
 Create arp reply packet header
 */
void arp_back_header_create(struct sr_arphdr * arp_header,
                            struct sr_arphdr * new_arp_header,
                            struct sr_if *rc){
    
    // same fields
    new_arp_header->ar_hrd = arp_header->ar_hrd;
    new_arp_header->ar_pro = arp_header->ar_pro;
    new_arp_header->ar_hln = arp_header->ar_hln;
    new_arp_header->ar_pln = arp_header->ar_pln;
    
    //opcode is reply
    new_arp_header->ar_op = htons(ARP_REPLY);
    // targer ip is sender ip
    new_arp_header->ar_tip = arp_header->ar_sip;
    // sender ip addr is the router ip
    printf("90----------------%d\n",arp_header->ar_tip == rc->ip);
    new_arp_header->ar_sip = arp_header->ar_tip;
    // targer mac addr is sender mac addr
    memcpy(new_arp_header->ar_tha, arp_header->ar_sha, ETHER_ADDR_LEN);
    // sender mac addr is the router mac addr
    memcpy(new_arp_header->ar_sha, rc->addr, ETHER_ADDR_LEN);
    return;
}

/*---------------------------------------------------------------------
 * Method: search new router (gateway) in list of router
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
        //todo  fix
        printf("122th\n");
    }
    return rc;
}

/*---------------------------------------------------------------------
 * Method: return a new ip_packet
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

uint8_t *ethernet_packet_create(uint8_t *host_source,
                                uint16_t type,
                                uint8_t *packet,
                                size_t length,
                                uint8_t *host_destination){
    printf("147th\n");
    struct sr_ethernet_hdr ethernet_header;
    memcpy(ethernet_header.ether_dhost, host_destination, ETHER_ADDR_LEN);
    memcpy(ethernet_header.ether_shost, host_source, ETHER_ADDR_LEN);
    ethernet_header.ether_type = htons(type);
    printf("152th\n");
    // put the header and packet into the buffer
    uint8_t *buffer = malloc(sizeof(struct sr_ethernet_hdr) + length);
    memcpy(buffer, &ethernet_header, sizeof(struct sr_ethernet_hdr));
    memcpy(buffer+sizeof(struct sr_ethernet_hdr), packet, length);
    printf("157th\n");
    return buffer;
}
/*---------------------------------------------------------------------
* Method: send_arp_request()
* Broadcasts an ARP request looking for ip_to_find on the interface supplied
* ---------------------------------------------------------------------*/
void send_arp_request(struct sr_instance *sr, struct sr_if *rc, struct in_addr destination){
    /*
      * Important info from header
      *  ----------------------------------------------------------
      * |   Sender MAC   |   Sender IP  | Target MAC  | Target IP  |
      * | interface MAC  | interface IP |  UNKNOWN    | ip_to_find |
      *  ----------------------------------------------------------
      *
      * */
    printf("171th\n");
    struct sr_arphdr * new_arp_header = malloc(sizeof(struct sr_arphdr));
    
    // Protocol information
    new_arp_header -> ar_hrd = htons(ARPHDR_ETHER);
    new_arp_header->ar_pro = htons(ETHERTYPE_IP);
    new_arp_header->ar_hln = ETHER_ADDR_LEN;
    new_arp_header->ar_pln = 4;
    new_arp_header->ar_op = htons(ARP_REQUEST);
    
    // fileds in request
    memcpy(new_arp_header->ar_sha, rc->addr, ETHER_ADDR_LEN);
    bzero(new_arp_header->ar_tha, ETHER_ADDR_LEN);
    new_arp_header->ar_sip = rc->ip;
    new_arp_header->ar_tip = destination.s_addr;
   // memcpy(&new_arp_header->ar_tip, &destination.s_addr, sizeof(uint32_t));
    printf("187th\n");
    struct in_addr ip_addr = { rc->ip };
    printf("*** -> Sent ARP request from %s (%s) looking for ", inet_ntoa(ip_addr), rc->name);
    printf("%s\n", inet_ntoa(destination));
  
    // put packet into an ethernet header
    uint8_t bc[ETHER_ADDR_LEN];
    memset(bc, 0xFF,ETHER_ADDR_LEN);
    uint8_t *buffer  = ethernet_packet_create(rc->addr, ETHERTYPE_ARP, (uint8_t *)new_arp_header, sizeof(struct sr_arphdr), bc);
    printf("192th\n");
    sr_send_packet(sr, buffer, sizeof(struct sr_ethernet_hdr)+ sizeof (struct sr_arphdr), rc->name);
    
}
/*---------------------------------------------------------------------
 * Method: checksum
 * get checksum for ip header and compare
 * ---------------------------------------------------------------------*/
uint16_t checksum(const void* ip_head){
    const uint8_t* rv = (const uint8_t*)ip_head;
    uint32_t rc = 0;
    int len = sizeof(sizeof(struct ip));
    while(len>=2){
        // combine two group
        rc += rv[0] <<8 | rv[1];
        rv +=2;
        len -=2;
    }
    // add zero if there is any bit left
    if (len >0){
        rc += rv[0] <<8;
    }
    while(rc >0xffff){
        // low bit and hight bit together
        rc = (rc >>16) + (rc && 0xffff);
    }
    rc = htons(~rc);
    if (rc == 0){
        rc = 0xffff;
    }
    return rc;
}

uint16_t header_checksum(uint16_t *header, int count) {
    uint32_t sum = 0;

    while(count--) {
        sum += *header++;

        if(sum & 0xFFFF0000) {
            // Carry Occurred
            sum &= 0xFFFF;
            sum++;
        }
    }

    return ~(sum & 0xFFFF);
}
/*---------------------------------------------------------------------
 * Method: deal_with_IP(1,2,3,4)
 * get ip header and then follow the IP logic
 * firstly we need to get ip header
 * ---------------------------------------------------------------------*/

void deal_with_IP (uint8_t * packet,struct sr_if* rc ,unsigned int len, struct sr_instance* sr){
     struct ip* ip_head =  get_ip_header(packet);
     ip_head -> ip_ttl -=1;
     if (ip_head->ip_ttl <=0){
         return;
     }
     // Create a buffer to calculate the checksum
    
    uint16_t *header_buffer;
    ip_head->ip_sum = 0;
    int actual_header_length = ip_head->ip_hl * 4;
    //posix_memalign((void **) &header_buffer, 16, actual_header_length);
    header_buffer = malloc(sizeof(uint16_t)*actual_header_length);
    bzero(header_buffer, sizeof(uint16_t) * actual_header_length);

    
    memcpy(header_buffer, ip_head, sizeof(struct ip));

   
   // memcpy(header_buffer + sizeof(struct ip) , packet + sizeof(struct sr_ethernet_hdr) + sizeof(struct ip),
     //      ip_head->ip_hl * 4 - sizeof(struct ip));

    // Calculate the new checksum
    ip_head->ip_sum = header_checksum(header_buffer, actual_header_length / 2);
     // todo we need to calculate the checksum in this packet
     /**
     uint16_t ip_sum =  ip_head -> ip_sum;
     ip_head -> ip_sum = 0;
     ip_head -> ip_sum = checksum(ip_head);
     if(ip_sum != ip_head->ip_sum){
         fprintf(stderr,"CheckSum is incorrect\n");
        // return ;
     }
     **/
     //printf("%d\n",ip_head->ip_sum);
     // get current interface and see if we are the destination
     uint32_t destination_ip = ip_head ->ip_dst.s_addr;
     //struct sr_if* face = sr_get_interface(sr,interface);
     if (rc->ip == destination_ip){
         // todo not sure we should we do (fix already)
     }
     struct sr_rt* router =  look_router(sr->routing_table,destination_ip);
     printf("251th\n");
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
             // todo not sure what to do (fix already)
              printf("262th\n");
             next_hop = ip_head->ip_dst;
         }
     
        // to do construct a packet in here and pass it as second argument in add_req;
        uint8_t* new_packet = copy_ip_packet(len-sizeof(struct sr_ethernet_hdr),ip_head,packet,len);
        printf("268th\n");
        sr_m* rv = look_up(sr->ac, next_hop.s_addr);
        printf("270th\n");
        if(rv!=NULL){
               //todo zehua
            printf("\tForwarding packet bound for %s through next hop @ ",
                    inet_ntoa(ip_head ->ip_dst));
            uint8_t *b = ethernet_packet_create(gateway_face->addr, ETHERTYPE_IP, new_packet, len - sizeof(struct sr_ethernet_hdr), rv->mac_addr);
            printf("274th\n");
            sr_send_packet(sr, b, len, gateway_face->name);
            printf("276th\n");
        }else{
             // we need to add arp request and send it as a arp_request
            printf("\tSending ARP request to %s (%s), to forward packet bound for ",
                    inet_ntoa(next_hop), gateway_face->name);
            printf("%s\n", inet_ntoa(ip_head ->ip_dst));
            printf("279th\n");
            // if we have the mac
            add_req (next_hop.s_addr,new_packet,sr->ac,len-sizeof(struct sr_ethernet_hdr), router->interface);
            printf("281th\n");
            // to do zehua
            send_arp_request(sr, gateway_face, next_hop);
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
    
    // check arp packet minimum length
    if(len < arp_packet_length){
        fprintf(stderr, "length of arp packet is short than normal");
        return;
    }
    
    // opcode to see it is request or reply
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
                printf("we are don't destination\n");
                return;
            }
            int LengthOfPacket = arp_packet_length;
            uint8_t *arp_reply_header = (uint8_t *)malloc(LengthOfPacket);
            printf("320th\n");
            // create ethernet header
           // ethernet_header_create(eth_header, (struct sr_ethernet_hdr *)arp_reply_header,rc);
           // printf("323th\n");
            // create arp header
            arp_back_header_create(arp_header, (struct sr_arphdr*) arp_reply_header,rc
                                   );
            printf("354th\n");
            uint8_t* new_packet = ethernet_packet_create(rc->addr,
                                ETHERTYPE_ARP,
                                arp_reply_header,
                                sizeof(struct sr_arphdr),
                                eth_header->ether_shost);
            // send arp reply
            printf("328th\n");
            sr_send_packet(sr,new_packet,sizeof(struct sr_ethernet_hdr) + sizeof(struct sr_arphdr),rc->name);
            printf("330th\n");
            //free(arp_header);
            return;
            
        // arp reply
        // cache and put it into request queue and send outstanding packets
        // arp reply processing code should move entries from the ARP request queue to the ARP cache
        }else if (ar_op == ARP_REPLY){
            printf("We got a arp reply in here\n");
            sr_r* match_req =  look_insert(arp_header->ar_sha ,arp_header->ar_sip , sr->ac);
            if (match_req != NULL){
                printf("368th\n");
                sr_p* packet = match_req ->packet;
                while(packet!=NULL){
                    printf("371th\n");
                    /**
                    uint8_t* etherenet_frame = packet->frame;
                    struct sr_ethernet_hdr* ete_head = (struct sr_ethernet_hdr*)etherenet_frame;
                    memcpy(ete_head->ether_dhost,  arp_header->ar_sha, ETHER_ADDR_LEN);
                    memcpy(ete_head->ether_shost, rc->addr, ETHER_ADDR_LEN);
                    sr_send_packet(sr,etherenet_frame , packet->len,rc->name);
                    **/
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