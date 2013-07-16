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
#include "er-smart-meter.h"

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
static char *msg[RESOURCES_SIZE][NR_ENCODINGS];
static uint16_t msg_size[RESOURCES_SIZE][NR_ENCODINGS];

#define CHUNKS_TOTAL    4096

/********************/
/* helper functions */
/********************/

void
send_message(const char* message, const uint16_t size_msg, void* request, void* response, uint8_t *buffer, int32_t *offset)
{
  uint16_t length;
  char *err_msg;
  const char* len;

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
    if (length < 0)
      length = 0;
  }

  if (length>REST_MAX_CHUNK_SIZE)     
  {
    length = REST_MAX_CHUNK_SIZE;

    memcpy(buffer, message + *offset, length);

    /* Truncate if above CHUNKS_TOTAL bytes. */
    if (*offset+length > CHUNKS_TOTAL)
      length = CHUNKS_TOTAL - *offset;

    /* IMPORTANT for chunk-wise resources: Signal chunk awareness to REST engine. */
    *offset += length;

    /* Signal end of resource representation. */
    if (*offset>=CHUNKS_TOTAL)
      *offset = -1;
  }
  else
  {
    memcpy(buffer, message + *offset, length);
    *offset = -1;
  }

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

int16_t
create_response(const char **message, uint8_t resource, void *request, void *response, int8_t *encoding)
{
  const uint16_t *accept = NULL;
  uint8_t num;

  /* decide upon content-format */
  num = REST.get_header_accept(request, &accept);

  if (num && (accept[0]==REST.type.APPLICATION_XML || accept[0]==REST.type.APPLICATION_EXI) )
  {
    *encoding = accept[0];
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

/********************/
/* Resources ********/
/********************/

RESOURCE(meter, METHOD_GET, "smart-meter", "title=\"Hello meter: ?len=0..\";rt=\"Text\"");

void
meter_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  PRINTF("\nMeter handler called\n");
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
  send_message(meter_message, size_msg, request, response, buffer, offset);
  return;
}

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

  PRINTF("Will send message: %s\n", meter_message);
  PRINTF("Encoding: %d\n", acc_encoding);
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

  PRINTF("Will send message: %s\n", meter_message);
  PRINTF("Encoding: %d\n", acc_encoding);
  PRINTF("Size: %d\n", size_msg);
  send_message(meter_message, size_msg, request, response, buffer, offset);
  return;
}

#if 0
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

  /* clear msg_size array to prevent programmer errors */
  memset(&msg_size[0][0], -1, RESOURCES_SIZE * NR_ENCODINGS * sizeof(uint16_t));

  rest_activate_resource(&resource_meter);
  msg[RESOURCES_METER][REST.type.APPLICATION_XML] = "value xml\0";
  msg_size[RESOURCES_METER][REST.type.APPLICATION_XML] = 10;
  msg[RESOURCES_METER][REST.type.APPLICATION_EXI] = "value exi\0";
  msg_size[RESOURCES_METER][REST.type.APPLICATION_EXI] = 10;

  rest_activate_resource(&resource_meter_power_history);
  msg[RESOURCES_METER_POWER_HISTORY][REST.type.APPLICATION_XML] = "historyxm\0";
  msg_size[RESOURCES_METER_POWER_HISTORY][REST.type.APPLICATION_XML] = 10;
  msg[RESOURCES_METER_POWER_HISTORY][REST.type.APPLICATION_EXI] = "historyex\0";
  msg_size[RESOURCES_METER_POWER_HISTORY][REST.type.APPLICATION_EXI] = 10;

  rest_activate_resource(&resource_meter_power_history_query);
  msg[RESOURCES_METER_POWER_HISTORY_QUERY][REST.type.APPLICATION_XML] = "<obj is=\"obix:HistoryQueryOut\">\n\
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
  msg_size[RESOURCES_METER_POWER_HISTORY_QUERY][REST.type.APPLICATION_XML] = 849;
  msg[RESOURCES_METER_POWER_HISTORY_QUERY][REST.type.APPLICATION_EXI] = "historyex\0";
  msg_size[RESOURCES_METER_POWER_HISTORY_QUERY][REST.type.APPLICATION_EXI] = 10;

  rest_activate_resource(&resource_meter_power_history_rollup);
  msg[RESOURCES_METER_POWER_HISTORY_ROLLUP][REST.type.APPLICATION_XML] = "<obj is=\"obix:HistoryRollupOut\">\n\
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
  msg_size[RESOURCES_METER_POWER_HISTORY_ROLLUP][REST.type.APPLICATION_XML] = 2443;
  msg[RESOURCES_METER_POWER_HISTORY_ROLLUP][REST.type.APPLICATION_EXI] = "historyex\0";
  msg_size[RESOURCES_METER_POWER_HISTORY_ROLLUP][REST.type.APPLICATION_EXI] = 10;

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

  /* Define application-specific events here. */
  while(1) {
    PROCESS_WAIT_EVENT();
  } /* while (1) */

  PROCESS_END();
}
