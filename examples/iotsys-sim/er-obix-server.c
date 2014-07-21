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

#if WITH_POWERTRACE
#warning "Compiling with powertrace!"
#include "powertrace.h"
#endif

#include "rest-engine.h"

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

#define CHUNKS_TOTAL    1024

extern resource_t res_meter;

/********************/
/* helper functions */
/********************/

void
send_message(const char* message, const uint16_t size_msg, void *request, void *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  PRINTF("Send Message: Size = %d, Offset = %d\n", size_msg, *offset);
  PRINTF("Preferred Size: %d\n", preferred_size);

  uint16_t length;
  char *err_msg;
  const char* len;

  length = size_msg - *offset;

  if (length <= 0)
  {
    PRINTF("AHOYHOY?!\n");
    REST.set_response_status(response, REST.status.INTERNAL_SERVER_ERROR);
    err_msg = "calculation of message length error";
    REST.set_response_payload(response, err_msg, strlen(err_msg));
    return;
  }

  if (preferred_size < 0 || preferred_size > REST_MAX_CHUNK_SIZE)
  {
    preferred_size = REST_MAX_CHUNK_SIZE;
    PRINTF("Preferred size set to REST_MAX_CHUNK_SIZE = %d\n", preferred_size);
  }

  if (length > preferred_size)
  {
    PRINTF("Message still larger then preferred_size, trunkating...\n");
    length = preferred_size;
    PRINTF("Length is now %u\n", length);

    memcpy(buffer, message + *offset, length);

    /* Truncate if above CHUNKS_TOTAL bytes. */
    if (*offset+length > CHUNKS_TOTAL)
    {
      PRINTF("Reached CHUNKS_TOTAL, truncating...\n");
      length = CHUNKS_TOTAL - *offset;
      PRINTF("Length is now %u\n", length);
      PRINTF("End of resource, setting offset to -1\n");
      *offset = -1;
    }
    else
    {
      /* IMPORTANT for chunk-wise resources: Signal chunk awareness to REST engine. */
      *offset += length;
      PRINTF("Offset refreshed to %u\n", *offset);
    }
  }
  else
  {
    memcpy(buffer, message + *offset, length);
    *offset = -1;
  }
  
  PRINTF("Sending response chunk: length = %u, offset = %d\n", length, *offset);

  REST.set_header_etag(response, (uint8_t *) &length, 1);
  REST.set_response_payload(response, buffer, length);
}

/********************/
/* Resources ********/
/********************/
static void meter_get_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset);
RESOURCE(res_meter, "title=\"Hello meter: ?len=0..\";rt=\"Text\"", meter_get_handler, NULL, NULL, NULL);

static void
meter_get_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  PRINTF("\nMeter handler called\n");
  PRINTF("Preffered Size: %u\n", preferred_size);

  char const *const msg = "aaaabbbbccccddddeeeeffffgggghhhhaaaabbbbccccddddeeeeffffgggghhhhaaaabbbbccccddddeeeeffffgggghhhhaaaabbbbccccddddeeeeffffgggghhhh";
  const uint8_t msg_size = REQUEST_SIZE;

  send_message(msg, msg_size, request, response, buffer, preferred_size, offset);

  return;
}

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

  /* if static routes are used rather than RPL */
  #if STATIC_ROUTING
    set_global_address();
    configure_routing();
  #endif

  /* Initialize the REST engine. */
  rest_init_engine();

  /* Activate the application-specific resources. */

#if 0
  /* clear msg_size array to prevent programmer errors */
  memset(&msg_size[0][0], -1, RESOURCES_SIZE * NR_ENCODINGS * sizeof(uint16_t));
#endif

  rest_activate_resource(&res_meter, "h");

#if WITH_POWERTRACE
  powertrace_start(CLOCK_SECOND * 10);
#endif 

  /* Define application-specific events here. */
  while(1) {
    PROCESS_WAIT_EVENT();
  } /* while (1) */

  PROCESS_END();
}
