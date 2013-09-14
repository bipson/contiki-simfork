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
#if 0
#include "sys/clock.h"
#include "sys/rtimer.h"
#endif
#include "contiki.h"
#include "contiki-net.h"

#warning "Compiling with powertrace!"
#include "powertrace.h"

#if STATIC_ROUTING
#warning "Compiling with static routing!"
#include "static-routing.h"
#endif

/* Define which resources to include to meet memory constraints. */
#define REST_RES_METER 1

#include "erbium.h"
#ifndef COLLECT
#include "er-smart-meter.h"
#endif


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
#ifdef COLLECT
#else
static char *msg[RESOURCES_SIZE][NR_ENCODINGS];
static uint16_t msg_size[RESOURCES_SIZE][NR_ENCODINGS];
#endif /* COLLECT */

#define CHUNKS_TOTAL    1024
//#define CHUNKS_TOTAL    4096
//#define CHUNKS_TOTAL    8

/********************/
/* helper functions */
/********************/

#ifndef COLLECT
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

  REST.set_header_etag(response, (uint16_t *) &length, 1);
  REST.set_response_payload(response, buffer, length);
}

void
set_error_response(void* response, int8_t error_code)
{
  char *err_msg;

  switch(error_code)
  {
    case ERR_BLOCKOUTOFSCOPE:
      REST.set_response_status(response, REST.status.BAD_OPTION);
      /* A block error message should not exceed the minimum block size (16). */
      err_msg = "BlockOutOfScope";
      break;
    case ERR_WRONGCONTENTTYPE:
      PRINTF("wrong content-type!\n");
      REST.set_response_status(response, REST.status.UNSUPPORTED_MEDIA_TYPE);
      err_msg = "Supporting content-types application/xml and application/exi";
      break;
    case ERR_ALLOC:
      PRINTF("ERROR while creating message!\n");
      REST.set_response_status(response, REST.status.INTERNAL_SERVER_ERROR);
      err_msg = "ERROR while creating message :\\";
      break;
    default:
      PRINTF("AHOYHOY?!\n");
      REST.set_response_status(response, REST.status.INTERNAL_SERVER_ERROR);
      err_msg = "creating message error, this should not happen :\\";
  }

  REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
  REST.set_response_payload(response, err_msg, strlen(err_msg));

  return;
}

#if 0
int16_t
create_response(const char **message, uint8_t resource, void *request, void *response, int8_t *encoding)
{
  const uint16_t *accept = NULL;
  uint8_t num;

  /* decide upon content-format */
  num = REST.get_header_accept(request, &accept);

  PRINTF("num = %u\n", num);
  PRINTF("type = %u\n", accept[0]);

  if (num && (accept[0]==REST.type.APPLICATION_XML || accept[0]==REST.type.APPLICATION_EXI) )
  {
    if (accept[0] == REST.type.APPLICATION_XML)
    {
      *encoding = ENCODING_XML;
    }
    else
    {
      *encoding = ENCODING_EXI;
    }

    if ((*message = msg[resource][*encoding]) == NULL)
    {
      return ERR_ALLOC;
    }
    REST.set_header_content_type(response, *encoding);
    PRINTF("size found: %d\n", msg_size[resource][*encoding]);
    return msg_size[resource][*encoding];
  }
  else
  {
    return ERR_WRONGCONTENTTYPE;
  }
}
#endif
#endif /* !COLLECT */

/********************/
/* Resources ********/
/********************/
PERIODIC_RESOURCE(meter, METHOD_GET, "smart-meter", "title=\"Hello meter: ?len=0..\";rt=\"Text\"", 600*CLOCK_SECOND);

void
meter_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  PRINTF("\nMeter handler called\n");
#if 0
  /* we save the message as static variable, so it is retained through multiple calls (chunked resource) */
  static const char *meter_message;
  static uint16_t size_msg;
  static int8_t acc_encoding;

  /* create response */
  if (*offset <= 0)
  {
    PRINTF("First call (offset <= 0)\n");
    int16_t message_size = -1;
    acc_encoding = -1;
    PRINTF("Will create response\n");
    if ((message_size = create_response(&meter_message, RESOURCES_METER, request, response, &acc_encoding)) <= 0)
    {
      PRINTF("Error caught: %d\n", message_size);
      set_error_response(response, message_size);
      return;
    }
    size_msg = message_size;
  }

  PRINTF("Will send message: %s\n", meter_message);
  PRINTF("Encoding: %d\n", acc_encoding);
  PRINTF("Size: %d\n", size_msg);
#endif 
  send_message(msg[RESOURCES_METER][ENCODING_XML], msg_size[RESOURCES_METER][ENCODING_XML], request, response, buffer, preferred_size, offset);
  return;
}

void
meter_periodic_handler(resource_t *r) {
  static uint16_t obs_counter = 0;
  PRINTF("periodic meter handler triggered\n");

  obs_counter++;
  
  /* Build notification. */
  coap_packet_t notification[1]; /* This way the packet can be treated as pointer as usual. */
  coap_init_message(notification, COAP_TYPE_NON, REST.status.OK, 0 );
  coap_set_header_block2(notification, 0, 1, 64);
  coap_set_payload(notification, msg[RESOURCES_METER][ENCODING_XML], 64);

  /* Notify the registered observers with the given message type, observe option, and payload. */
  REST.notify_subscribers(r, obs_counter, notification);
}


#if 0
RESOURCE(meter_power_history, METHOD_GET, "smart-meter/power/history", "title=\"Hello meter_power: ?len=0..\";rt=\"Text\"");

void
meter_power_history_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  PRINTF("\nMeter Power history handler called\n");
  /* we save the message as static variable, so it is retained through multiple calls (chunked resource) */
  static const char *meter_message;
  static uint16_t size_msg;
  static int8_t acc_encoding;

  /* compute message once */
  if (*offset <= 0)
  {
    PRINTF("First call (offset <= 0)\n");
    int16_t message_size = -1;
    acc_encoding = -1;
    PRINTF("Will create response\n");
    if ((message_size = create_response(&meter_message, RESOURCES_METER_POWER_HISTORY, request, response, &acc_encoding)) <= 0)
    {
      PRINTF("Error caught: %d\n", message_size);
      set_error_response(response, message_size);
      return;
    }
    size_msg = message_size;
  }

  PRINTF("Will send message: %s\n", meter_message);
  PRINTF("Encoding: %d\n", acc_encoding);
  send_message(meter_message, size_msg, request, response, buffer, offset);
  return;

}

RESOURCE(meter_power_history_query, METHOD_GET, "smart-meter/power/history/query", "title=\"Hello meter_power: ?len=0..\";rt=\"Text\"");

void
meter_power_history_query_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  PRINTF("\nMeter Power history query handler called\n");
  /* we save the message as static variable, so it is retained through multiple calls (chunked resource) */
  static const char *meter_message;
  static uint16_t size_msg;
  static int8_t acc_encoding;

  /* compute message once */
  if (*offset <= 0)
  {
    PRINTF("First call (offset <= 0)\n");
    int16_t message_size = -1;
    acc_encoding = -1;
    PRINTF("Will create response\n");
    if ((message_size = create_response(&meter_message, RESOURCES_METER_POWER_HISTORY_QUERY, request, response, &acc_encoding)) <= 0)
    {
      PRINTF("Error caught: %d\n", message_size);
      set_error_response(response, message_size);
      return;
    }
    size_msg = message_size;
  }

  send_message(meter_message, size_msg, request, response, buffer, offset);
  return;
}

RESOURCE(meter_power_history_rollup, METHOD_GET, "smart-meter/power/history/rollup", "title=\"Hello meter_power: ?len=0..\";rt=\"Text\"");

void
meter_power_history_rollup_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  PRINTF("\nMeter Power history rollup handler called\n");
  /* we save the message as static variable, so it is retained through multiple calls (chunked resource) */
  static const char *meter_message;
  static uint16_t size_msg;
  static int8_t acc_encoding;

  /* compute message once */
  if (*offset <= 0)
  {
    PRINTF("First call (offset <= 0)\n");
    int16_t message_size = -1;
    acc_encoding = -1;
    PRINTF("Will create response\n");
    if ((message_size = create_response(&meter_message, RESOURCES_METER_POWER_HISTORY_ROLLUP, request, response, &acc_encoding)) <= 0)
    {
      PRINTF("Error caught: %d\n", message_size);
      set_error_response(response, message_size);
      return;
    }
    size_msg = message_size;
  }

  PRINTF("Size: %d\n", size_msg);
  send_message(meter_message, size_msg, request, response, buffer, offset);
  return;
}

RESOURCE(meter_energy_history, METHOD_GET, "smart-meter/energy/history", "title=\"Hello meter_energy: ?len=0..\";rt=\"Text\"");

void
meter_energy_history_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  PRINTF("\nMeter Energy history handler called\n");
  /* we save the message as static variable, so it is retained through multiple calls (chunked resource) */
  static const char *meter_message;
  static uint16_t size_msg;
  static int8_t acc_encoding;

  /* compute message once */
  if (*offset <= 0)
  {
    PRINTF("First call (offset <= 0)\n");
    int16_t message_size = -1;
    acc_encoding = -1;
    PRINTF("Will create response\n");
    if ((message_size = create_response(&meter_message, RESOURCES_METER_ENERGY_HISTORY, request, response, &acc_encoding)) <= 0)
    {
      PRINTF("Error caught: %d\n", message_size);
      set_error_response(response, message_size);
      return;
    }
    size_msg = message_size;
  }

  PRINTF("Will send message: %s\n", meter_message);
  PRINTF("Encoding: %d\n", acc_encoding);
  send_message(meter_message, size_msg, request, response, buffer, offset);
  return;
}

RESOURCE(meter_energy_history_query, METHOD_GET, "smart-meter/energy/history/query", "title=\"Hello meter_energy: ?len=0..\";rt=\"Text\"");

void
meter_energy_history_query_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  PRINTF("\nMeter Energy history query handler called\n");
  /* we save the message as static variable, so it is retained through multiple calls (chunked resource) */
  static const char *meter_message;
  static uint16_t size_msg;
  static int8_t acc_encoding;

  /* compute message once */
  if (*offset <= 0)
  {
    PRINTF("First call (offset <= 0)\n");
    int16_t message_size = -1;
    acc_encoding = -1;
    PRINTF("Will create response\n");
    if ((message_size = create_response(&meter_message, RESOURCES_METER_ENERGY_HISTORY_QUERY, request, response, &acc_encoding)) <= 0)
    {
      PRINTF("Error caught: %d\n", message_size);
      set_error_response(response, message_size);
      return;
    }
    size_msg = message_size;
  }

  PRINTF("Will send message: %s\n", meter_message);
  PRINTF("Encoding: %d\n", acc_encoding);
  send_message(meter_message, size_msg, request, response, buffer, offset);
  return;
}

RESOURCE(meter_energy_history_rollup, METHOD_GET, "smart-meter/energy/history/rollup", "title=\"Hello meter_energy: ?len=0..\";rt=\"Text\"");

void
meter_energy_history_rollup_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  PRINTF("\nMeter Energy history rollup handler called\n");
  /* we save the message as static variable, so it is retained through multiple calls (chunked resource) */
  static const char *meter_message;
  static uint16_t size_msg;
  static int8_t acc_encoding;

  /* compute message once */
  if (*offset <= 0)
  {
    PRINTF("First call (offset <= 0)\n");
    int16_t message_size = -1;
    acc_encoding = -1;
    PRINTF("Will create response\n");
    if ((message_size = create_response(&meter_message, RESOURCES_METER_ENERGY_HISTORY_ROLLUP, request, response, &acc_encoding)) <= 0)
    {
      PRINTF("Error caught: %d\n", message_size);
      set_error_response(response, message_size);
      return;
    }
    size_msg = message_size;
  }

  PRINTF("Will send message: %s\n", meter_message);
  PRINTF("Encoding: %d\n", acc_encoding);
  send_message(meter_message, size_msg, request, response, buffer, offset);
  return;
}
#endif /* 0 */

#endif

PROCESS(rest_server_example, "E");
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
  #if STATIC_ROUTING
    set_global_address();
    configure_routing();
  #endif

  /* Initialize the REST engine. */
  rest_init_engine();

  /* Activate the application-specific resources. */

#if REST_RES_METER

#if 0
  /* clear msg_size array to prevent programmer errors */
  memset(&msg_size[0][0], -1, RESOURCES_SIZE * NR_ENCODINGS * sizeof(uint16_t));
#endif

  rest_activate_periodic_resource(&periodic_resource_meter);
  /*
  msg[RESOURCES_METER][ENCODING_XML] = "value xml\0";
  msg_size[RESOURCES_METER][ENCODING_XML] = 9;
  */
  msg[RESOURCES_METER][ENCODING_XML] = "value xml, value xmi, value xml, value xmi, foo faa faeh, fadfadfaefadfaefadfaefadgajlödkfjaoihkaflijdflknaoiefhabjaodfoiauefnalkjdfoiargiaenlvknaldofjaoienfalkfnbaldjflakjeflkjaldkjalvlaknfalkfaeiognaknvalkndflaeioqöigqnalkndva\0";
  msg_size[RESOURCES_METER][ENCODING_XML] = 140;
#if 0
  msg[RESOURCES_METER][REST.type.APPLICATION_EXI] = "value exi\0";
  msg_size[RESOURCES_METER][REST.type.APPLICATION_EXI] = 10;

  rest_activate_resource(&resource_meter_power_history);
  msg[RESOURCES_METER_POWER_HISTORY][REST.type.APPLICATION_XML] = "historyxm\0";
  msg_size[RESOURCES_METER_POWER_HISTORY][REST.type.APPLICATION_XML] = 10;
  msg[RESOURCES_METER_POWER_HISTORY][REST.type.APPLICATION_EXI] = "historyex\0";
  msg_size[RESOURCES_METER_POWER_HISTORY][REST.type.APPLICATION_EXI] = 10;

  rest_activate_resource(&resource_meter_power_history_query);
  msg[RESOURCES_METER_POWER_HISTORY_QUERY][ENCODING_XML] = "<obj is=\"obix:HistoryQueryOut\">\n\
\t<int name=\"count\" href=\"count\" val=\"5\"/>\n\
\t<abstime name=\"start\" href=\"start\" val=\"2013-06-19T15:41:56.133+02:00\" tz=\"Europe/Vienna\"/>\n\
\t<abstime name=\"end\" href=\"end\" val=\"2013-06-19T15:47:35.950+02:00\" tz=\"Europe/Vienna\"/>\n\
\t<list of=\"obix:HistoryRecord\">\n\
\t\t<obj>\n\
\t\t\t<abstime val=\"2013-06-19T15:41:56.133+02:00\" tz=\"Europe/Vienna\"/>\n\
\t\t\t<real val=\"300.0\"/>\n\
\t\t</obj>\n\
\t\t<obj>\n\
\t\t\t<abstime val=\"2013-06-19T15:47:26.796+02:00\" tz=\"Europe/Vienna\"/>\n\
\t\t\t<real val=\"450.0\"/>\n\
\t\t</obj>\n\
\t\t<obj>\n\
\t\t\t<abstime val=\"2013-06-19T15:47:30.878+02:00\" tz=\"Europe/Vienna\"/>\n\
\t\t\t<real val=\"500.0\"/>\n\
\t\t</obj>\n\
\t\t<obj>\n\
\t\t\t<abstime val=\"2013-06-19T15:47:33.923+02:00\" tz=\"Europe/Vienna\"/>\n\
\t\t\t<real val=\"650.0\"/>\n\
\t\t</obj>\n\
\t\t<obj>\n\
\t\t\t<abstime val=\"2013-06-19T15:47:35.950+02:00\" tz=\"Europe/Vienna\"/>\n\
\t\t\t<real val=\"1650.0\"/>\n\
\t\t</obj>\n\
\t</list>\n\
</obj>";
  msg_size[RESOURCES_METER_POWER_HISTORY_QUERY][ENCODING_XML] = 849;
  msg[RESOURCES_METER_POWER_HISTORY_QUERY][ENCODING_EXI] = "\x80\x41\x1b\xd8\x9a\x94\x0d\xa5\xcc\x59\xbd\x89\xa5\xe0\xe9\x21\xa5\xcd\xd1\xbd\xc9\xe5\x45\xd5\x95\xc9\
  \xe5\x3d\xd5\xd3\x20\x8d\x2d\xce\x8a\x0a\xd0\xe4\xca\xcc\x0e\xc6\xde\xea\xdc\xe9\x50\x56\xe6\x16\xd6\x50\
  \x1c\xa0\x8e\xcc\x2d\x80\x66\xb9\x21\x0c\x2c\x4e\x6e\x8d\x2d\xac\xaa\x00\xc1\xdc\xdd\x18\x5c\x9d\x2a\x01\
  \x00\x79\x40\xdd\x1e\x83\xd1\x5d\x5c\x9b\xdc\x19\x4b\xd5\x9a\x59\x5b\x9b\x98\x75\x00\xa3\xe6\x46\x06\x26\
  \x65\xa6\x06\xc5\xa6\x27\x2a\x86\x26\xa7\x46\x86\x27\x46\xa6\xc5\xc6\x26\x66\x65\x66\x06\x47\x46\x06\x10\
  \x20\x2b\x2b\x73\x23\x01\xc8\x00\x8f\x99\x18\x18\x99\x96\x98\x1b\x16\x98\x9c\xaa\x18\x9a\x9d\x1a\x1b\x9d\
  \x19\x9a\x97\x1c\x9a\x98\x15\x98\x19\x1d\x18\x18\x08\x82\xb6\x34\xb9\xba\x28\x1b\x7b\x30\xa3\x7b\x13\x4b\
  \xc1\xd2\x43\x4b\x9b\xa3\x7b\x93\xca\x93\x2b\x1b\x7b\x93\x26\x40\x02\x90\x06\x40\x04\x01\x19\x05\x72\x65\
  \x61\x6c\x50\x05\x07\x33\x30\x30\x2e\x30\x8e\x40\x00\x40\x04\x7c\xc8\xc0\xc4\xcc\xb4\xc0\xd8\xb4\xc4\xe5\
  \x50\xc4\xd4\xe8\xd0\xdc\xe8\xc8\xd8\xb8\xdc\xe4\xd8\xac\xc0\xc8\xe8\xc0\xc0\x04\x1c\xd0\xd4\xc0\xb8\xc0\
  \x60\x80\x08\xf9\x91\x81\x89\x99\x69\x81\xb1\x69\x89\xca\xa1\x89\xa9\xd1\xa1\xb9\xd1\x99\x81\x71\xc1\xb9\
  \xc1\x59\x81\x91\xd1\x81\x80\x08\x39\xa9\x81\x81\x71\x80\xc1\x00\x11\xf3\x23\x03\x13\x32\xd3\x03\x62\xd3\
  \x13\x95\x43\x13\x53\xa3\x43\x73\xa3\x33\x32\xe3\x93\x23\x32\xb3\x03\x23\xa3\x03\x00\x10\x73\x63\x53\x02\
  \xe3\x01\x82\x00\x20\x04\x02\x10\x62\x6c\x6a\x60\x5c\x60\x35\x80";
  msg_size[RESOURCES_METER_POWER_HISTORY_QUERY][ENCODING_EXI] = 354;

  rest_activate_resource(&resource_meter_power_history_rollup);
  msg[RESOURCES_METER_POWER_HISTORY_ROLLUP][ENCODING_XML] = "<obj is=\"obix:HistoryRollupOut\">\n\
\t<int name=\"count\" href=\"count\" val=\"5\"/>\n\
\t<abstime name=\"start\" href=\"start\" val=\"2013-06-19T15:40:59.461+02:00\" tz=\"Europe/Vienna\"/>\n\
\t<abstime name=\"end\" href=\"end\" val=\"2013-06-19T15:48:06.584+02:00\" tz=\"Europe/Vienna\"/>\n\
\t<list of=\"obix:HistoryRecord\">\n\
\t\t<obj>\n\
\t\t\t<int name=\"count\" href=\"count\" val=\"1\"/>\n\
\t\t\t<abstime name=\"start\" href=\"start\" val=\"2013-06-19t15:41:55.461+02:00\" tz=\"Europe/Vienna\"/>\n\
\t\t\t<abstime name=\"end\" href=\"end\" val=\"2013-06-19t15:41:56.461+02:00\" tz=\"Europe/Vienna\"/>\n\
\t\t\t<real name=\"min\" href=\"min\" val=\"300.0\"/>\n\
\t\t\t<real name=\"max\" href=\"max\" val=\"300.0\"/>\n\
\t\t\t<real name=\"avg\" href=\"avg\" val=\"300.0\"/>\n\
\t\t\t<real name=\"sum\" href=\"sum\" val=\"300.0\"/>\n\
\t\t</obj>\n\
\t\t<obj>\n\
\t\t\t<int name=\"count\" href=\"count\" val=\"1\"/>\n\
\t\t\t<abstime name=\"start\" href=\"start\" val=\"2013-06-19T15:47:26.461+02:00\" tz=\"Europe/Vienna\"/>\n\
\t\t\t<abstime name=\"end\" href=\"end\" val=\"2013-06-19T15:47:27.461+02:00\" tz=\"Europe/Vienna\"/>\n\
\t\t\t<real name=\"min\" href=\"min\" val=\"450.0\"/>\n\
\t\t\t<real name=\"max\" href=\"max\" val=\"450.0\"/>\n\
\t\t\t<real name=\"avg\" href=\"avg\" val=\"450.0\"/>\n\
\t\t\t<real name=\"sum\" href=\"sum\" val=\"450.0\"/>\n\
\t\t</obj>\n\
\t\t<obj>\n\
\t\t\t<int name=\"count\" href=\"count\" val=\"1\"/>\n\
\t\t\t<abstime name=\"start\" href=\"start\" val=\"2013-06-19T15:47:30.461+02:00\" tz=\"Europe/Vienna\"/>\n\
\t\t\t<abstime name=\"end\" href=\"end\" val=\"2013-06-19T15:47:31.461+02:00\" tz=\"Europe/Vienna\"/>\n\
\t\t\t<real name=\"min\" href=\"min\" val=\"500.0\"/>\n\
\t\t\t<real name=\"max\" href=\"max\" val=\"500.0\"/>\n\
\t\t\t<real name=\"avg\" href=\"avg\" val=\"500.0\"/>\n\
\t\t\t<real name=\"sum\" href=\"sum\" val=\"500.0\"/>\n\
\t\t</obj>\n\
\t\t<obj>\n\
\t\t\t<int name=\"count\" href=\"count\" val=\"1\"/>\n\
\t\t\t<abstime name=\"start\" href=\"start\" val=\"2013-06-19T15:47:33.461+02:00\" tz=\"Europe/Vienna\"/>\n\
\t\t\t<abstime name=\"end\" href=\"end\" val=\"2013-06-19T15:47:34.461+02:00\" tz=\"Europe/Vienna\"/>\n\
\t\t\t<real name=\"min\" href=\"min\" val=\"650.0\"/>\n\
\t\t\t<real name=\"max\" href=\"max\" val=\"650.0\"/>\n\
\t\t\t<real name=\"avg\" href=\"avg\" val=\"650.0\"/>\n\
\t\t\t<real name=\"sum\" href=\"sum\" val=\"650.0\"/>\n\
\t\t</obj>\n\
\t\t<obj>\n\
\t\t\t<int name=\"count\" href=\"count\" val=\"1\"/>\n\
\t\t\t<abstime name=\"start\" href=\"start\" val=\"2013-06-19T15:47:35.461+02:00\" tz=\"Europe/Vienna\"/>\n\
\t\t\t<abstime name=\"end\" href=\"end\" val=\"2013-06-19T15:47:36.461+02:00\" tz=\"Europe/Vienna\"/>\n\
\t\t\t<real name=\"min\" href=\"min\" val=\"1650.0\"/>\n\
\t\t\t<real name=\"max\" href=\"max\" val=\"1650.0\"/>\n\
\t\t\t<real name=\"avg\" href=\"avg\" val=\"1650.0\"/>\n\
\t\t\t<real name=\"sum\" href=\"sum\" val=\"1650.0\"/>\n\
\t\t</obj>\n\
\t</list>\n\
</obj>";
  msg_size[RESOURCES_METER_POWER_HISTORY_ROLLUP][ENCODING_XML] = 2443;
  msg[RESOURCES_METER_POWER_HISTORY_ROLLUP][ENCODING_EXI] = "\x80\x41\x1b\xd8\x9a\x94\x0d\xa5\xcc\x5d\xbd\x89\xa5\xe0\xe9\x21\xa5\xcd\xd1\xbd\xc9\xe5\x49\xbd\xb1\xb1\
  \xd5\xc1\x3d\xd5\xd3\x20\x8d\x2d\xce\x8a\x0a\xd0\xe4\xca\xcc\x0e\xc6\xde\xea\xdc\xe9\x50\x56\xe6\x16\xd6\
  \x50\x1c\xa0\x8e\xcc\x2d\x80\x66\xb9\x21\x0c\x2c\x4e\x6e\x8d\x2d\xac\xaa\x00\xc1\xdc\xdd\x18\x5c\x9d\x2a\
  \x01\x00\x79\x40\xdd\x1e\x83\xd1\x5d\x5c\x9b\xdc\x19\x4b\xd5\x9a\x59\x5b\x9b\x98\x75\x00\xa3\xe6\x46\x06\
  \x26\x65\xa6\x06\xc5\xa6\x27\x2a\x86\x26\xa7\x46\x86\x07\x46\xa7\x25\xc6\x86\xc6\x25\x66\x06\x47\x46\x06\
  \x10\x20\x2b\x2b\x73\x23\x01\xc8\x00\x8f\x99\x18\x18\x99\x96\x98\x1b\x16\x98\x9c\xaa\x18\x9a\x9d\x1a\x1c\
  \x1d\x18\x1b\x17\x1a\x9c\x1a\x15\x98\x19\x1d\x18\x18\x08\x82\xb6\x34\xb9\xba\x28\x1b\x7b\x30\xa3\x7b\x13\
  \x4b\xc1\xd2\x43\x4b\x9b\xa3\x7b\x93\xca\x93\x2b\x1b\x7b\x93\x26\x40\x00\x60\x02\x01\x12\x06\x62\x18\x00\
  \xb0\x13\x40\x04\x7c\xc8\xc0\xc4\xcc\xb4\xc0\xd8\xb4\xc4\xe5\x50\xc4\xd4\xe8\xd0\xc4\xe8\xd4\xd4\xb8\xd0\
  \xd8\xc4\xac\xc0\xc8\xe8\xc0\xc0\x30\x02\x60\x2c\x80\x08\xf9\x91\x81\x89\x99\x69\x81\xb1\x69\x89\xca\xa1\
  \x89\xa9\xd1\xa1\x89\xd1\xa9\xb1\x71\xa1\xb1\x89\x59\x81\x91\xd1\x81\x80\xc8\x2b\x93\x2b\x0b\x62\x80\x18\
  \x2b\x6b\x4b\x75\x40\x10\x07\x25\x00\x50\x73\x33\x03\x02\xe3\x0c\x0c\x15\xb5\x85\xe1\x00\xf1\x00\xc0\x30\
  \x56\x17\x66\x74\x03\xe4\x03\x00\xc1\x5c\xdd\x5b\x50\x0c\x08\x06\x0e\x40\x00\x60\x01\x00\x84\x80\x30\xa0\
  \x01\x60\x23\x40\x04\x7c\xc8\xc0\xc4\xcc\xb4\xc0\xd8\xb4\xc4\xe5\x50\xc4\xd4\xe8\xd0\xdc\xe8\xc8\xd8\xb8\
  \xd0\xd8\xc4\xac\xc0\xc8\xe8\xc0\xc0\x28\x00\x98\x09\x90\x01\x1f\x32\x30\x31\x33\x2d\x30\x36\x2d\x31\x39\
  \x54\x31\x35\x3a\x34\x37\x3a\x32\x37\x2e\x34\x36\x31\x2b\x30\x32\x3a\x30\x30\x01\x80\x34\x02\xc2\x0e\x68\
  \x6a\x60\x5c\x60\x03\x00\x88\x05\xc4\x02\x40\x60\x15\x00\xbc\x80\x48\x0c\x03\x20\x18\x10\x09\x0c\x18\x00\
  \x40\x21\x20\x06\x14\x00\x2c\x04\x68\x00\x8f\x99\x18\x18\x99\x96\x98\x1b\x16\x98\x9c\xaa\x18\x9a\x9d\x1a\
  \x1b\x9d\x19\x98\x17\x1a\x1b\x18\x95\x98\x19\x1d\x18\x18\x05\x00\x13\x01\x32\x00\x23\xe6\x46\x06\x26\x65\
  \xa6\x06\xc5\xa6\x27\x2a\x86\x26\xa7\x46\x86\xe7\x46\x66\x25\xc6\x86\xc6\x25\x66\x06\x47\x46\x06\x00\x30\
  \x06\x80\x58\x41\xcd\x4c\x0c\x0b\x8c\x00\x60\x11\x00\xb8\x80\x60\x0c\x02\xa0\x17\x90\x0c\x01\x80\x64\x03\
  \x02\x01\x81\x83\x00\x08\x04\x24\x00\xc2\x80\x05\x80\x8d\x00\x11\xf3\x23\x03\x13\x32\xd3\x03\x62\xd3\x13\
  \x95\x43\x13\x53\xa3\x43\x73\xa3\x33\x32\xe3\x43\x63\x12\xb3\x03\x23\xa3\x03\x00\xa0\x02\x60\x26\x40\x04\
  \x7c\xc8\xc0\xc4\xcc\xb4\xc0\xd8\xb4\xc4\xe5\x50\xc4\xd4\xe8\xd0\xdc\xe8\xcc\xd0\xb8\xd0\xd8\xc4\xac\xc0\
  \xc8\xe8\xc0\xc0\x06\x00\xd0\x0b\x08\x39\xb1\xa9\x81\x71\x80\x0c\x02\x20\x17\x10\x0f\x01\x80\x54\x02\xf2\
  \x01\xe0\x30\x0c\x80\x60\x40\x3c\x30\x60\x01\x00\x84\x80\x18\x50\x00\xb0\x11\xa0\x02\x3e\x64\x60\x62\x66\
  \x5a\x60\x6c\x5a\x62\x72\xa8\x62\x6a\x74\x68\x6e\x74\x66\x6a\x5c\x68\x6c\x62\x56\x60\x64\x74\x60\x60\x14\
  \x00\x4c\x04\xc8\x00\x8f\x99\x18\x18\x99\x96\x98\x1b\x16\x98\x9c\xaa\x18\x9a\x9d\x1a\x1b\x9d\x19\x9b\x17\
  \x1a\x1b\x18\x95\x98\x19\x1d\x18\x18\x00\xc0\x1a\x01\x61\x08\x31\x36\x35\x30\x2e\x30\x01\x80\x44\x02\xe2\
  \x01\x20\x18\x05\x40\x2f\x20\x12\x01\x80\x64\x03\x02\x01\x20\xd6";
  msg_size[RESOURCES_METER_POWER_HISTORY_ROLLUP][ENCODING_EXI] = 770;
#endif

#if 0
  rest_activate_resource(&resource_meter_energy_history);
  msg[RESOURCES_METER_ENERGY_HISTORY][REST.type.APPLICATION_XML] = "historyxm\0";
  msg_size[RESOURCES_METER_ENERGY_HISTORY][REST.type.APPLICATION_XML] = 10;
  msg[RESOURCES_METER_ENERGY_HISTORY][REST.type.APPLICATION_EXI] = "historyex\0";
  msg_size[RESOURCES_METER_ENERGY_HISTORY][REST.type.APPLICATION_EXI] = 10;

  rest_activate_resource(&resource_meter_energy_history_query);
  msg[RESOURCES_METER_ENERGY_HISTORY_QUERY][REST.type.APPLICATION_XML] = "historyxm\0";
  msg_size[RESOURCES_METER_ENERGY_HISTORY_QUERY][REST.type.APPLICATION_XML] = 10;
  msg[RESOURCES_METER_ENERGY_HISTORY_QUERY][REST.type.APPLICATION_EXI] = "historyex\0";
  msg_size[RESOURCES_METER_ENERGY_HISTORY_QUERY][REST.type.APPLICATION_EXI] = 10;

  rest_activate_resource(&resource_meter_energy_history_rollup);
  msg[RESOURCES_METER_ENERGY_HISTORY_ROLLUP][REST.type.APPLICATION_XML] = "historyxm\0";
  msg_size[RESOURCES_METER_ENERGY_HISTORY_ROLLUP][REST.type.APPLICATION_XML] = 10;
  msg[RESOURCES_METER_ENERGY_HISTORY_ROLLUP][REST.type.APPLICATION_EXI] = "historyex\0";
  msg_size[RESOURCES_METER_ENERGY_HISTORY_ROLLUP][REST.type.APPLICATION_EXI] = 10;
#endif /* 0 */

#endif

  powertrace_start(CLOCK_SECOND * 10);

  /* Define application-specific events here. */
  while(1) {
    PROCESS_WAIT_EVENT();
  } /* while (1) */

  PROCESS_END();
}
