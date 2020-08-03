/* ----------------------------------------------------------------------------
 * struct arp_cache
 *
 * simple representation of arp_cache
 *
 * -------------------------------------------------------------------------- */
#include <stdio.h>
#include <stdint.h>
#ifndef ARPCACHE
#define ARPCACHE
typedef struct packet{
    unsigned int len;
    char* interface;
    uint8_t* frame;
    struct packet* next;
}sr_p;
typedef struct map{
  unsigned char mac_addr[6];
  uint32_t ip;
  struct map* next;
 // int occupy;
  
}sr_m;
typedef struct request{
     uint32_t ip;
     struct request* next;
     sr_p* packet;
}sr_r;
typedef struct arp_cache{
    sr_r* requests;
    sr_m* mappings;

}arp_cache;
sr_r* look_insert(unsigned char* , uint32_t , arp_cache*);
sr_m* new_entry (unsigned char* , uint32_t );
void add_req (uint32_t,uint8_t*,arp_cache*,unsigned int, char*);
sr_p* new_packet(unsigned int,uint8_t* ,char* , sr_r*);
sr_r* new_request(uint32_t,arp_cache* );
sr_m * look_up(arp_cache*, uint32_t);
//void delete(arp_cache*, sr_r*);
#endif
