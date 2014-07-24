/*
 * Copyright (c) 2011, Institute of Computer Aided Automation.
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
 * This file is part of the IoTSyS project.
 *
 */

#include "contiki.h"
#include "lib/random.h"
#include "sys/ctimer.h"
#include "sys/etimer.h"
#include "net/uip.h"
#include "net/uip-ds6.h"

#include "simple-udp.h"
#include "er-coap-13-engine.h"

#include <stdio.h>
#include <string.h>

#define UDP_PORT 5683

#define SEND_INTERVAL		(10 * CLOCK_SECOND)
#define SEND_TIME		(random_rand() % (SEND_INTERVAL))

#define DEBUG 0
#if DEBUG
#define PRINTF(...) printf(__VA_ARGS__)
#define PRINT6ADDR(addr) PRINTF(" %02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x ", ((uint8_t *)addr)[0], ((uint8_t *)addr)[1], ((uint8_t *)addr)[2], ((uint8_t *)addr)[3], ((uint8_t *)addr)[4], ((uint8_t *)addr)[5], ((uint8_t *)addr)[6], ((uint8_t *)addr)[7], ((uint8_t *)addr)[8], ((uint8_t *)addr)[9], ((uint8_t *)addr)[10], ((uint8_t *)addr)[11], ((uint8_t *)addr)[12], ((uint8_t *)addr)[13], ((uint8_t *)addr)[14], ((uint8_t *)addr)[15])
#define PRINTLLADDR(lladdr) PRINTF(" %02x:%02x:%02x:%02x:%02x:%02x ",(lladdr)->addr[0], (lladdr)->addr[1], (lladdr)->addr[2], (lladdr)->addr[3],(lladdr)->addr[4], (lladdr)->addr[5])
#else
#define PRINTF(...)
#define PRINT6ADDR(addr)
#define PRINTLLADDR(addr)
#endif

static struct simple_udp_connection broadcast_connection;

void
client_chunk_handler(void *response)
{

}


/*---------------------------------------------------------------------------*/
PROCESS(broadcast_example_process, "IoTSyS group communication example");
AUTOSTART_PROCESSES(&broadcast_example_process);
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(broadcast_example_process, ev, data)
{
  static struct etimer periodic_timer;
  static struct etimer send_timer;
  static uip_ipaddr_t addr;
  uip_ds6_maddr_t *maddr;
  const char msg[] = "<bool val=\"true\"/>\0";
  const char msg2[] = "<bool val=\"false\"/>\0";
  static int toogle = 0;
  static int msgid = 0;

  coap_packet_t request; /* This way the packet can be treated as pointer as usual. */

  PROCESS_BEGIN();
  uip_ip6addr(&addr, 0xff15, 0, 0, 0, 0, 0, 0, 0x1);
  PRINT6ADDR(&addr);
  maddr = uip_ds6_maddr_add(&addr);
  if(maddr == NULL){
	  PRINTF("NULL returned.");
  }
  else{
	  PRINTF("Is used: %d", maddr->isused);
	  PRINT6ADDR(&(maddr->ipaddr));
  }

  etimer_set(&periodic_timer, SEND_INTERVAL);
  while(1) {
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&periodic_timer));
    etimer_reset(&periodic_timer);
    etimer_set(&send_timer, SEND_TIME);
    printf("### Send time is %d\n", (unsigned short) SEND_TIME);
    printf("Current process name is %s\n", PROCESS_CURRENT()->name);
    //uip_ip6addr(&addr, 0xff12, 0, 0, 0, 0, 0, 0, 0x1);
    //simple_udp_sendto(&broadcast_connection, "Test", 4, &addr);
    coap_init_connection(uip_htons(5683));

    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&send_timer));

    coap_init_message(&request, COAP_TYPE_NON, COAP_PUT, msgid++);

    if(toogle == 1){
    	toogle = 0;
    	printf("Msg2\n");
    	coap_set_payload(&request, (uint8_t *)msg2, sizeof(msg2)-1);
    }
    else{
    	toogle = 1;
    	printf("Msg1\n");
    	coap_set_payload(&request, (uint8_t *)msg, sizeof(msg)-1);
    }

    coap_set_header_uri_path(&request, "");

    printf("Simple request.\n");
    PRINT6ADDR(&addr);
    coap_simple_request(&addr, 5683, &request);

    printf("\n--Done--\n");
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
