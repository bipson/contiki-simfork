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
*      Erbium (Er) CoAP client example
* \author
*      Philipp Raich <philipp.raich@student.tuwien.ac.at>
*      based mostly on COAP-client written by:
*      Matthias Kovatsch <kovatsch@inf.ethz.ch>
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "contiki.h"
#include "contiki-net.h"

#include "node-id.h"

#include "er-coap-engine.h"

#if WITH_POWERTRACE
#warning "Compiling with powertrace!"
#include "powertrace.h"
#endif

#define DEBUG 0
#if DEBUG
#define PRINTF(...) printf(__VA_ARGS__)
#define PRINT6ADDR(addr) PRINTF("[%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x]", ((uint8_t *)addr)[0], ((uint8_t *)addr)[1], ((uint8_t *)addr)[2], ((uint8_t *)addr)[3], ((uint8_t *)addr)[4], ((uint8_t *)addr)[5], ((uint8_t *)addr)[6], ((uint8_t *)addr)[7], ((uint8_t *)addr)[8], ((uint8_t *)addr)[9], ((uint8_t *)addr)[10], ((uint8_t *)addr)[11], ((uint8_t *)addr)[12], ((uint8_t *)addr)[13], ((uint8_t *)addr)[14], ((uint8_t *)addr)[15])
#define PRINTLLADDR(lladdr) PRINTF("[%02x:%02x:%02x:%02x:%02x:%02x]",(lladdr)->addr[0], (lladdr)->addr[1], (lladdr)->addr[2], (lladdr)->addr[3],(lladdr)->addr[4], (lladdr)->addr[5])
#else
#define PRINTF(...)
#define PRINT6ADDR(addr)
#define PRINTLLADDR(addr)
#endif

/* TODO: This server address is hard-coded for Cooja. */
//#define SERVER_NODE(ipaddr)   uip_ip6addr(ipaddr, 0xaaaa, 0, 0, 0, 0x202, 0x2, 0x2, 0x2) /* cooja2 */
#define SERVER_NODE(ipaddr, id)   uip_ip6addr(ipaddr, 0xaaaa, 0, 0, 0, 0xc30c, 0x0, 0x0, id) /* cooja2 */

#define LOCAL_PORT      UIP_HTONS(COAP_DEFAULT_PORT+1)
#define REMOTE_PORT     UIP_HTONS(COAP_DEFAULT_PORT)

#define TOGGLE_INTERVAL 10
/* continuous requests or limited to 10 requests */
#define CONTINUOUS 1

PROCESS(coap_client_example, "S");
AUTOSTART_PROCESSES(&coap_client_example);

/* processes for blocking call */
PROCESS(request, "R");

uip_ipaddr_t server_ipaddr;
static struct etimer et;

char *msg = "aaaabbbbccccddddeeeeffffgggghhhh";

/* leading and ending slashes only for demo purposes, get cropped automatically when setting the Uri-Path */
char* service_url = "h";

/* This function is will be passed to COAP_BLOCKING_REQUEST() to handle responses. */
void
client_chunk_handler(void *response)
{
#if 0
  uint32_t num;
  uint8_t more;
  uint16_t size;
  uint32_t offset;

  /* just register if this was the last package */
  coap_get_header_block2(response, &num, &more, &size, &offset);
  if (!more)
  {
    printf("OK\n");
  } 
#endif /* 0 */
}

#define COAP_WRAPPER(i) \
{ \
  coap_packet_t request[1]; \
  coap_init_message(request, COAP_TYPE_CON, COAP_PUT, 0); \
  coap_set_header_uri_path(request, service_url); \
  coap_set_payload(request, (uint8_t *) msg, REQUEST_SIZE); \
  SERVER_NODE(&server_ipaddr, i); \
  PRINT6ADDR(&server_ipaddr); \
  PRINTF(" : %u\n", UIP_HTONS(REMOTE_PORT)); \
  COAP_BLOCKING_REQUEST(&server_ipaddr, REMOTE_PORT, request, client_chunk_handler); \
}

PROCESS_THREAD(request, ev, data)
{
  PROCESS_BEGIN();
  static int i = 0;

  for(i = 0; i < 4; i++)
  {
    int target = node_id - (i + 1);
    PRINTF("sending to: %u\n", target);

    COAP_WRAPPER(target);
  }

  PROCESS_END();
}

PROCESS_THREAD(coap_client_example, ev, data)
{
  PROCESS_BEGIN();

#if CONTINUOUS == 0
  static int count = 0;
#endif /* CONTINUOUS */
  static int active = 0;

  /* receives all CoAP messages */
  coap_init_engine();

#if WITH_POWERTRACE
  powertrace_start(CLOCK_SECOND * 10);
#endif 

  /* delay the toggle timer for some time, so it does not always coincide with
   * the timer for powertrace */
  etimer_set(&et, TOGGLE_INTERVAL * 1.5 * CLOCK_SECOND);

  while(1) {
    PROCESS_YIELD();

    if (etimer_expired(&et)) 
    {
      // timer is always running and resetting, we just hook in
      if (active)
      {
        PRINTF("--Toggle timer--\n");

        process_exit(&request);
        printf("Sent\n");
        process_start(&request, NULL);

#if CONTINUOUS == 0
        count++;
        if (count >= 50)
        {
          printf("End\n");
          active = 0;
          break;
        }
#endif /* CONTINUOUS */

        PRINTF("--Done--\n");
        etimer_reset(&et);
      } else {
        active = 1;
        etimer_set(&et, TOGGLE_INTERVAL * CLOCK_SECOND);
      }
    }
  }

  PROCESS_END();
}
