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
 *      Matthias Kovatsch <kovatsch@inf.ethz.ch>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sys/clock.h"
#include "sys/rtimer.h"
#include "contiki.h"
#include "contiki-net.h"
#include "random.h"
#if STATIC_ROUTING
#warning "Compiling with static routing!"
#include "static-routing.h"
#endif

/* Define which resources to include to meet memory constraints. */
#define REST_RES_METER 1

#include "erbium.h"

/* for temperature sensor */
#include "dev/i2cmaster.h"
#include "dev/tmp102.h"

/* For CoAP-specific example: not required for normal RESTful Web service. */
#if WITH_COAP == 3
#include "er-coap-03.h"
#elif WITH_COAP == 7
#include "er-coap-07.h"
#elif WITH_COAP == 12
#include "er-coap-12.h"
#elif WITH_COAP == 13
#include "er-coap-13.h"
#else
#warning "Erbium example without CoAP-specifc functionality"
#endif /* CoAP-specific example */

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

#if REST_RES_METER

#define TEMP_MSG_MAX_SIZE 1024
#define CHUNKS_TOTAL      1024

RESOURCE(meter, METHOD_GET, "smart-meter", "title=\"Hello meter: ?len=0..\";rt=\"Text\"");

void
meter_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{

}

RESOURCE(meter_power_history, METHOD_GET, "smart-meter/power/history", "title=\"Hello meter_power: ?len=0..\";rt=\"Text\"");

void
meter_power_history_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{

}

RESOURCE(meter_power_history_query, METHOD_GET, "smart-meter/power/history/query", "title=\"Hello meter_power: ?len=0..\";rt=\"Text\"");

void
meter_power_history_query_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{

}

RESOURCE(meter_power_history_rollup, METHOD_GET, "smart-meter/power/history/rollup", "title=\"Hello meter_power: ?len=0..\";rt=\"Text\"");

void
meter_power_history_rollup_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{

}

RESOURCE(meter_energy_history, METHOD_GET, "smart-meter/energy/history", "title=\"Hello meter_energy: ?len=0..\";rt=\"Text\"");

void
meter_energy_history_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{

}

RESOURCE(meter_energy_history_query, METHOD_GET, "smart-meter/energy/history/query", "title=\"Hello meter_energy: ?len=0..\";rt=\"Text\"");

void
meter_energy_history_query_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{

}

RESOURCE(meter_energy_history_rollup, METHOD_GET, "smart-meter/energy/history/rollup", "title=\"Hello meter_energy: ?len=0..\";rt=\"Text\"");

void
meter_energy_history_rollup_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{

}

uint8_t create_meter_response(int num, int accept, char *buffer, int window)
{
  size_t size_value;
  int value = 0;
  int size_msgp1, size_msgp2;
  const char *msgp1, *msgp2, *valuestring;
  uint8_t size_msg;

  if (num && accept==REST.type.APPLICATION_XML)
  {
    msgp1 = "value=";
    msgp2 = "\0";
    /* hardcoded length, ugly but faster and necc. for exi-answer */
    size_msgp1 = 6;
    size_msgp2 = 1;
  }
#if 0
  else if (num && accept==REST.type.APPLICATION_EXI)
  {
    msgp1 = "\x80\x10\x01\x04\x6F\x62\x6A\x01\x01\x00\x02\x14\x68\x74\x74\x70\x3A\x2F\x2F\x6D\x79\x68\x6F\x6D\x65\x2F\x74\x65\x6D\x70\x01\x02\x01\x05\x72\x65\x61\x6C\x01\x01\x00\x08\x06\x74\x65\x6D\x70\x01\x01\x01\x06\x75\x6E\x69\x74\x73\x14\x6F\x62\x69\x78\x3A\x75\x6E\x69\x74\x73\x2F\x63\x65\x6C\x73\x69\x75\x73\x02\x01\x01\x00\x11\x09";
    msgp2 = "\x03\x00\x00\0";
    /* hardcoded length, ugly but faster and necc. for exi-answer */
    size_msgp1 = 81;
    size_msgp2 = 3;
  }
#endif
  else
  {
    PRINTF("Unsupported encoding!\n");
    return -1;
  }

  value = 10;

  size_value = snprintf(NULL, 0, "%d", value);
  valuestring = malloc(size_value + 1);
  snprintf(valuestring, size_value + 1, "%d", value);
  
  /* Some data that has the length up to REST_MAX_CHUNK_SIZE. For more, see the chunk resource. */
  /* we assume null-appended strings behind msgp2 and tempstring */
  size_msg = size_msgp1 + size_msgp2 + size_value+ 1;

  if (size_msg > TEMP_MSG_MAX_SIZE)
  {
    PRINTF("Message too big!\n");
    return -1;
  }

  memcpy(buffer, msgp1, size_msgp1);
  memcpy(buffer + size_msgp1, valuestring, size_value);
  memcpy(buffer + size_msgp1 + size_value, msgp2, size_msgp2 + 1);

  return size_msg;
}

void
meter_foo_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  /* we save the message as static variable, so it is retained through multiple calls (chunked resource) */
  static char meter_message[TEMP_MSG_MAX_SIZE];
  static uint8_t size_msg;

  const uint16_t *accept = NULL;
  int num = 0, length = 0;
  char *err_msg;

  const char *len = NULL;

  /* Check the offset for boundaries of the resource data. */
  if (*offset>=CHUNKS_TOTAL)
  {
    REST.set_response_status(response, REST.status.BAD_OPTION);
    /* A block error message should not exceed the minimum block size (16). */
    err_msg = "BlockOutOfScope";
    REST.set_response_payload(response, err_msg, strlen(err_msg));
    return;
  }

  /* compute message once */
  if (*offset <= 0)
  {
    /* decide upon content-format */
    num = REST.get_header_accept(request, &accept);

    if (num && accept[0]==REST.type.APPLICATION_XML)
    {
      REST.set_header_content_type(response, REST.type.APPLICATION_XML);
    }
    else if (num && accept[0]==REST.type.APPLICATION_EXI)
    {
      REST.set_header_content_type(response, REST.type.APPLICATION_EXI);
    }
    else
    {
      PRINTF("wrong content-type!\n");
      REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
      REST.set_response_status(response, REST.status.UNSUPPORTED_MEDIA_TYPE);
      err_msg = "Supporting content-types application/xml and application/exi";
      REST.set_response_payload(response, err_msg, strlen(err_msg));
      return;
    }

    if ((size_msg = create_meter_response(num, accept[0], meter_message, 5)) <= 0)
    {
      PRINTF("ERROR while creating message!\n");
      REST.set_response_status(response, REST.status.INTERNAL_SERVER_ERROR);
      err_msg = "ERROR while creating message :\\";
      REST.set_response_payload(response, err_msg, strlen(err_msg));
      return;
    }

  }
  
  length = size_msg - *offset;

  if (length <= 0)
  {
    PRINTF("AHOYHOY?!\n");
    REST.set_response_status(response, REST.status.INTERNAL_SERVER_ERROR);
    err_msg = "calculation of message length error, this should not happen :\\";
    REST.set_response_payload(response, err_msg, strlen(err_msg));
    return;
  }

  /* The query string can be retrieved by rest_get_query() or parsed for its key-value pairs. */
  if (REST.get_query_variable(request, "len", &len))
  {
    length = atoi(len);
    if (length<0) length = 0;
  }

  if (length>REST_MAX_CHUNK_SIZE)     
  {
    length = REST_MAX_CHUNK_SIZE;

    memcpy(buffer, meter_message + *offset, length);

    /* Truncate if above CHUNKS_TOTAL bytes. */
    if (*offset+length > CHUNKS_TOTAL)
    {
      length = CHUNKS_TOTAL - *offset;
    }

    /* IMPORTANT for chunk-wise resources: Signal chunk awareness to REST engine. */
    *offset += length;

    /* Signal end of resource representation. */
    if (*offset>=CHUNKS_TOTAL)
    {
      *offset = -1;
    }
  } else {
    memcpy(buffer, meter_message + *offset, length);
    *offset = -1;
  }

  REST.set_header_etag(response, (uint8_t *) &length, 1);
  REST.set_response_payload(response, buffer, length);

}
#endif

PROCESS(rest_server_example, "Erbium Example Server");
AUTOSTART_PROCESSES(&rest_server_example);

PROCESS_THREAD(rest_server_example, ev, data)
{
  PROCESS_BEGIN();

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
#if 0
  #if STATIC_ROUTING
    set_global_address();
    configure_routing();
  #endif
#endif

  #if STATIC_ROUTING
    configure_routing();
  #endif

  /* Initialize the REST engine. */
  rest_init_engine();

  /* Activate the application-specific resources. */

#if REST_RES_METER
  rest_activate_resource(&resource_meter_power_history);
  rest_activate_resource(&resource_meter_power_history_query);
  rest_activate_resource(&resource_meter_power_history_rollup);
  rest_activate_resource(&resource_meter_energy_history);
  rest_activate_resource(&resource_meter_energy_history_query);
  rest_activate_resource(&resource_meter_energy_history_rollup);
#endif

  /* Define application-specific events here. */
  while(1) {
    PROCESS_WAIT_EVENT();
  } /* while (1) */

  PROCESS_END();
}
