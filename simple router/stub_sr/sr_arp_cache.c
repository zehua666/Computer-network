/* ----------------------------------------------------------------------------
 * struct arp_cache
 *
 * simple representation of arp_cache
 *
 * -------------------------------------------------------------------------- */
#include "sr_arp_cache.h"
#include <stdlib.h>
#include <string.h>
sr_m * look_up(arp_cache* arp_cache, uint32_t ip){
    sr_m *p = arp_cache->mappings;
    while (p) {
        if(p->ip == ip){
            return p;
        }
        p = p->next;
    }
    
    return NULL;
}
sr_m* new_entry (unsigned char* mac, uint32_t ip){
    sr_m* rc= malloc(sizeof(sr_m));
    rc -> next = NULL;
    rc -> ip = ip;
    memcpy(rc-> mac_addr,mac,6);
    return rc;

}
sr_r* new_request(uint32_t ip,arp_cache* table){
    sr_r* rc = malloc(sizeof(sr_r));
    rc -> ip = ip;
    rc -> next = table->requests;
    rc -> packet = NULL;
    return rc;
}
sr_p* new_packets(unsigned int len,uint8_t* packet,char* name, sr_r* req){
    sr_p* rc = malloc(sizeof(sr_p));
    rc -> frame = (uint8_t*)malloc(len);
    rc -> len = len;
    rc->frame = packet;
    rc -> interface = strdup(name);
    rc -> next = req->packet;
    return rc ;
}
void add_req (uint32_t ip,uint8_t* packet,arp_cache* table,unsigned int len, char* name){
    sr_r* req = table->requests;
    while(req!=NULL){
        if(req->ip == ip){
            break;
        }
        req = req -> next;
    }
    if (req ==NULL){
        table->requests = new_request(ip,table);
        table->requests -> packet = new_packets(len,packet,name,table->requests);
        return;
    }
    //todo do we need to check NULL valu
    req-> packet = new_packets(len,packet,name,req);

}
sr_r* look_insert(unsigned char* mac, uint32_t ip, arp_cache* table){
    sr_r* cur = table->requests;
    sr_r* prev = NULL;
    sr_r* next = NULL;
    while(cur!=NULL){
        if(cur->ip == ip){
            // not head
            if(prev !=NULL){
               next = cur->next;
               prev->next = next;
            }
            // is head
            else{
               table->requests = cur->next;
            }
            break;
        }
        prev = cur;
        cur = cur -> next;
    }
    if (table-> mappings ==NULL){
        table-> mappings = new_entry(mac,ip);
        
    }
    else{
        sr_m* copy = table -> mappings;
        while(copy->next!=NULL){
            copy = copy -> next;
        }
        copy-> next = new_entry(mac,ip);
    }
    return cur;
}
