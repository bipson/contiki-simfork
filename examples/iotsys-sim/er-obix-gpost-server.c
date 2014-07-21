/*
 * Copyright (c) 2011, Matthias Kovatsch and other contributors.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This file is part of the Contiki operating system.
 */

/**
 * \file
 *      Erbium (Er) REST Engine example (with CoAP-specific code)
 * \author
 *      Philipp Raich <philipp.raich@student.tuwien.ac.at>
 *      based on CoAP implementation by:
 *      Matthias Kovatsch <kovatsch@inf.ethz.ch>
 */

#include <stdlib.h>
#include <string.h>
#include "contiki.h"
#include "contiki-net.h"
#include "node-id.h"

#if WITH_POWERTRACE
#warning "Compiling with powertrace!"
#include "powertrace.h"
#endif

/* Define which resources to include to meet memory constraints. */
#define REST_RES_METER 1
#define GROUP_COMM 1

#include "rest-engine.h"

#define DEBUG 1
#if DEBUG
#define PRINTF(...) printf(__VA_ARGS__)
#define PRINT6ADDR(addr) PRINTF("[%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x]", ((uint8_t *)addr)[0], ((uint8_t *)addr)[1], ((uint8_t *)addr)[2], ((uint8_t *)addr)[3], ((uint8_t *)addr)[4], ((uint8_t *)addr)[5], ((uint8_t *)addr)[6], ((uint8_t *)addr)[7], ((uint8_t *)addr)[8], ((uint8_t *)addr)[9], ((uint8_t *)addr)[10], ((uint8_t *)addr)[11], ((uint8_t *)addr)[12], ((uint8_t *)addr)[13], ((uint8_t *)addr)[14], ((uint8_t *)addr)[15])
#define PRINTLLADDR(lladdr) PRINTF("[%02x:%02x:%02x:%02x:%02x:%02x]",(lladdr)->addr[0], (lladdr)->addr[1], (lladdr)->addr[2], (lladdr)->addr[3],(lladdr)->addr[4], (lladdr)->addr[5])
#else
#define PRINTF(...)
#define PRINT6ADDR(addr)
#define PRINTLLADDR(addr)
#endif

#define CHUNKS_TOTAL    1024
//#define CHUNKS_TOTAL    4096
//#define CHUNKS_TOTAL    8
// Group communication definition
#define MAX_GC_HANDLERS 1
#define MAX_GC_GROUPS 1

#if REST_RES_METER
resource_t res_meter;
#endif /* REST_RES_METER */

#if GROUP_COMM
typedef void (*gc_handler) (char*);

// Data structure for storing group communication assignments.
// It is intended to store only the group identifier
// of a transient link-local scope multicast address (FF:12::XXXX)
typedef struct {
  int group_identifier;
  gc_handler handlers[MAX_GC_HANDLERS];
} gc_handler_t;

#endif /* GROUP_COMM */

/********************/
/* globals **********/
/********************/
#if GROUP_COMM
gc_handler_t gc_handlers[MAX_GC_GROUPS];
#endif

/********************/
/* helper functions */
/********************/
#if GROUP_COMM
void
extract_group_identifier(uip_ip6addr_t* ipv6Address, uint16_t* groupIdentifier )
{ 
  *groupIdentifier = 0; 
  *groupIdentifier =  ((uint8_t *)ipv6Address)[14]; 
  *groupIdentifier <<= 8; 
  *groupIdentifier += ((uint8_t *)ipv6Address)[15]; 
} 

void
join_group(int groupIdentifier, gc_handler handler  )
{
  int i,l=0;
  // use last 32 bits
  for(i = 0; i < MAX_GC_GROUPS; i++){
    if(gc_handlers[i].group_identifier == 0 || gc_handlers[i].group_identifier == groupIdentifier){ // free slot or same slot

      gc_handlers[i].group_identifier = groupIdentifier;
      //gc_handlers[i].group_identifier &= (groupAddress.u16[6] << 16);
      PRINTF("Assigned slot: %d\n", gc_handlers[i].group_identifier);

      // adding gc handler
      for(l=0; l < MAX_GC_HANDLERS; l++){
        if(gc_handlers[i].handlers[l] == NULL ||  gc_handlers[i].handlers[l] == &handler ){
          gc_handlers[i].handlers[l] = handler;
          PRINTF("(Re-)Assigned callback on slot %d\n", l);
          break;
        }
      }
      break;
    }
  }
}


#endif /* GROUP_COMM */

/********************/
/* Resources ********/
/********************/
#if REST_RES_METER
static void put_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset);

RESOURCE(res_meter, "title=\"Hello: ?len=0..\";rt=\"Text\"", NULL, NULL, put_handler, NULL);

static void
put_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  const uint8_t *payload;

  coap_get_payload(request, &payload);

  group_handler(payload);
#if 0
  PRINTF("\nMeter handler called\n");
  PRINTF("Preffered Size: %u\n", preferred_size);

  char const *const msg = "aaaabbbbccccddddeeeeffffgggghhhhaaaabbbbccccddddeeeeffffgggghhhhaaaabbbbccccddddeeeeffffgggghhhhaaaabbbbccccddddeeeeffffgggghhhh";
  const uint8_t msg_size = 128;

  send_message(msg, msg_size, request, response, buffer, preferred_size, offset);

#endif /* 0 */
  return;
}
#endif /* REST_RES_METER */

void group_handler(char *payload)
{
  char *msg = "aaaabbbbccccddddeeeeffffgggghhhh";

  if (strncmp(payload, msg, REQUEST_SIZE) == 0)
  {
    printf("OK, received message\n");
  }
  else
  {
    printf("ERROR, payload does not match!\n");
  }
}
#if GROUP_COMM

static void 
group_comm_handler(const uip_ipaddr_t *sender_addr,
    const uip_ipaddr_t *receiver_addr,
    const uint8_t *data,
    uint16_t datalen)
{
  uint16_t groupIdentifier;
  PRINT6ADDR(sender_addr);
  PRINT6ADDR(receiver_addr);
  uint8_t i,l=0;

  groupIdentifier =  ((uint8_t *)receiver_addr)[14];
  groupIdentifier <<= 8;
  groupIdentifier += ((uint8_t *)receiver_addr)[15];
  PRINTF("\n######### Data received on group comm handler with length %d for group identifier %d\n",
      datalen, groupIdentifier);

  for(i = 0; i < MAX_GC_GROUPS; i++)
  {
    if(gc_handlers[i].group_identifier == groupIdentifier)
    { // free slot or same slot
      for(l=0; l < MAX_GC_HANDLERS; l++)
      {
        if(gc_handlers[i].handlers[l] != NULL)
        {
          gc_handlers[i].handlers[l](data);
        }    
      }    
    }    
  }    
}
#endif /* GROUP_COMM */

PROCESS(rest_server_example, "E");
AUTOSTART_PROCESSES(&rest_server_example);

PROCESS_THREAD(rest_server_example, ev, data)
{
  PROCESS_BEGIN();
  PROCESS_PAUSE();
  PRINTF("Starting Erbium Example Server\n");

#ifdef RF_CHANNEL
  PRINTF("RF channel: %u\n", RF_CHANNEL);
#endif
#ifdef IEEE802154_PANID
  PRINTF("PAN ID: 0x%04X\n", IEEE802154_PANID);
#endif

  PRINTF("uIP buffer: %u\n", UIP_BUFSIZE);
  PRINTF("LL header: %u\n", UIP_LLH_LEN);
  PRINTF("IP+UDP header: %u\n", UIP_IPUDPH_LEN);
  PRINTF("REST max chunk: %u\n", REST_MAX_CHUNK_SIZE);

  /* Initialize the REST engine. */
  rest_init_engine();

  /* Activate the application-specific resources. */

#if REST_RES_METER
  rest_activate_resource(&res_meter, "h");
#endif /* REST_RES_METER */

#if GROUP_COMM
  coap_rest_implementation.set_group_comm_callback(group_comm_handler);

  /* statically joing group */
  uip_ipaddr_t groupAddress;
  int16_t groupIdentifier = 0;

  uip_ip6addr(&groupAddress, 0xff15,0,0,0,0,0,0,((node_id / 6)+1));
  uip_ds6_maddr_add(&groupAddress);

  extract_group_identifier(&groupAddress, &groupIdentifier);
  PRINT6ADDR(&groupAddress);
  PRINTF("\n group identifier: %d\n", groupIdentifier);
  join_group(groupIdentifier, &group_handler);
#endif

#if WITH_POWERTRACE
  powertrace_start(CLOCK_SECOND * 10);
#endif 

  /* Define application-specific events here. */
  while(1) {
    PROCESS_WAIT_EVENT();
  } /* while (1) */

  PROCESS_END();
}
