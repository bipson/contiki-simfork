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

#include "dev/button-sensor.h"

#include "er-coap-engine.h"

#if !UIP_CONF_IPV6_RPL && !defined (CONTIKI_TARGET_MINIMAL_NET) && !defined (CONTIKI_TARGET_NATIVE)
#warning "Compiling with static routing!"
#include "static-routing.h"
#endif

#include "dev/button-sensor.h"

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
/* toggle between continuous requests or limited to 10 requests */
#define CONTINUOUS 0
#define WAIT_AFTER_SEND 1

PROCESS(coap_client_example, "COAP Client Example");
AUTOSTART_PROCESSES(&coap_client_example);

PROCESS(request_proc, "Watchdog");

uip_ipaddr_t server_ipaddr;
static coap_packet_t request[1]; /* This way the packet can be treated as pointer as usual. */
static struct etimer et;

/* leading and ending slashes only for demo purposes, get cropped automatically when setting the Uri-Path */
char* service_url = "h";
static int exp_response;

/* This function is will be passed to COAP_BLOCKING_REQUEST() to handle responses. */
  void
client_chunk_handler(void *response)
{
  uint32_t num;
  uint8_t more;
  uint16_t size;
  uint32_t offset;

  /* just register if this was the last package */
  coap_get_header_block2(response, &num, &more, &size, &offset);
  if (!more)
  {
    printf("OK\n");
    exp_response = 0;
  }
}

PROCESS_THREAD(request_proc, ev, data)
{
  PROCESS_BEGIN();

  COAP_BLOCKING_REQUEST(&server_ipaddr, REMOTE_PORT, request, client_chunk_handler);

  PROCESS_END();
}

PROCESS_THREAD(coap_client_example, ev, data)
{
  PROCESS_BEGIN();

#if CONTINUOUS == 0
  static int count = 0;
#endif /* CONTINUOUS */
  static int active = 0;
  static int idle = 0;
  exp_response = 0;

  SERVER_NODE(&server_ipaddr, 2);
#if 0
  SERVER_NODE(&server_ipaddr[1], 3);
  SERVER_NODE(&server_ipaddr[2], 4);
  SERVER_NODE(&server_ipaddr[3], 5);
#endif

  /* receives all CoAP messages */
  coap_init_engine();

  SENSORS_ACTIVATE(button_sensor);

  etimer_set(&et, TOGGLE_INTERVAL * CLOCK_SECOND);

  while(1) {
    PROCESS_YIELD();

    if (etimer_expired(&et)) 
    {
      // timer is always running and resetting, we just hook in
      if (active)
      {
        PRINTF("--Toggle timer--\n");
        if (exp_response == 1)
        {
          process_exit(&request_proc);
          printf("ERROR\n");
          exp_response = 0;
        }
#if CONTINUOUS == 0
        if (count >= 10)
        {
          process_exit(&request_proc);
          active = 0;
          goto done;
        }
#endif /* CONTINUOUS */

        /* only send next request if idling over */
        if (idle == 0)
        {
#if WAIT_AFTER_SEND == 1
          idle = 1;
#endif /* WAIT_AFTER_SEND */
          /* prepare request, TID is set by COAP_BLOCKING_REQUEST() */
          coap_init_message(request, COAP_TYPE_CON, COAP_GET, 0 );
          coap_set_header_uri_path(request, service_url);

          PRINT6ADDR(&server_ipaddr);
          PRINTF(" : %u\n", UIP_HTONS(REMOTE_PORT));

          exp_response = 1;
          PRINTF("Sending request\n");
          process_start(&request_proc, NULL);

#if CONTINUOUS == 0
          count++;
          if (count >= 10)
          {
            printf("LAST!\n");
          }
#endif /* CONTINUOUS */

          PRINTF("--Done--\n");
        }
        else
        {
          /* toogle waiting */
          PRINTF("toggling idle\n");
          idle = 0;
        }

      }
done:
      etimer_reset(&et);
    }

    if (ev == sensors_event && data == &button_sensor)
    {
      printf("starting!\n");
#if CONTINUOUS == 0
      count = 0;
#endif /* CONTINUOUS */
      active = 1;
    }
  }

  PROCESS_END();
}
