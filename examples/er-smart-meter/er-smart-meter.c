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
#define REST_RES_TEMP 0
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

/******************************************************************************/
#if REST_RES_TEMP

#define CHUNKS_TOTAL      1024 
#define TEMP_MSG_MAX_SIZE 140   // more than enough right now
#define TEMP_BUFF_MAX     7     // -234.6\0

/******************************************************************************/
/* globals ********************************************************************/
/******************************************************************************/
char tempstring[TEMP_BUFF_MAX];

/******************************************************************************/
/* helper functions ***********************************************************/
/******************************************************************************/
int temp_to_buff(char* buffer) {
  int16_t  tempint;
  uint16_t tempfrac;
  int16_t  raw;
  uint16_t absraw;
  int16_t  sign = 1;

  /* get temperature */
  raw = tmp102_read_temp_raw();

  absraw = raw;
  if (raw < 0)
  {
    // Perform 2C's if sensor returned negative data
    absraw = (raw ^ 0xFFFF) + 1;
    sign = -1;
  }
  tempint  = (absraw >> 8) * sign;
  tempfrac = ((absraw>>4) % 16) * 625; // Info in 1/10000 of degree
  tempfrac = ((tempfrac) / 1000); // Round to 1 decimal

  return snprintf(tempstring, TEMP_BUFF_MAX, "%d.%1d", tempint, tempfrac);
}

int temp_to_default_buff() {

  return temp_to_buff(tempstring);

}

uint8_t create_response(int num, int accept, char *buffer)
{
  size_t size_temp;
  int size_msgp1, size_msgp2;
  const char *msgp1, *msgp2;
  uint8_t size_msg;

  if (num && accept==REST.type.APPLICATION_XML)
  {
    msgp1 = "<obj href=\"http://myhome/temp\">\n\t<real name=\"temp\" units=\"obix:units/celsius\" val=\"";
    msgp2 = "\"/>\n</obj>\0";
    /* hardcoded length, ugly but faster and necc. for exi-answer */
    size_msgp1 = 83;
    size_msgp2 = 10;
  }
  else if (num && accept==REST.type.APPLICATION_EXI)
  {
    msgp1 = "\x80\x10\x01\x04\x6F\x62\x6A\x01\x01\x00\x02\x14\x68\x74\x74\x70\x3A\x2F\x2F\x6D\x79\x68\x6F\x6D\x65\x2F\x74\x65\x6D\x70\x01\x02\x01\x05\x72\x65\x61\x6C\x01\x01\x00\x08\x06\x74\x65\x6D\x70\x01\x01\x01\x06\x75\x6E\x69\x74\x73\x14\x6F\x62\x69\x78\x3A\x75\x6E\x69\x74\x73\x2F\x63\x65\x6C\x73\x69\x75\x73\x02\x01\x01\x00\x11\x09";
    msgp2 = "\x03\x00\x00\0";
    /* hardcoded length, ugly but faster and necc. for exi-answer */
    size_msgp1 = 81;
    size_msgp2 = 3;
  }
  else
  {
    PRINTF("Unsupported encoding!\n");
    return -1;
  }

  if ((size_temp = temp_to_default_buff()) < 0 )
  {
    PRINTF("Error preparing temperature string!\n");
    return -1;
  }

  /* Some data that has the length up to REST_MAX_CHUNK_SIZE. For more, see the chunk resource. */
  /* we assume null-appended strings behind msgp2 and tempstring */
  size_msg = size_msgp1 + size_msgp2 + size_temp + 1;

  if (size_msg > TEMP_MSG_MAX_SIZE)
  {
    PRINTF("Message too big!\n");
    return -1;
  }

  memcpy(buffer, msgp1, size_msgp1);
  memcpy(buffer + size_msgp1, tempstring, size_temp);
  memcpy(buffer + size_msgp1 + size_temp, msgp2, size_msgp2 + 1);

  return size_msg;
}

/*
 * Resources are defined by the RESOURCE macro.
 * Signature: resource name, the RESTful methods it handles, and its URI path (omitting the leading slash).
 */
PERIODIC_RESOURCE(temp, METHOD_GET, "temp", "title=\"Hello temp: ?len=0..\";rt=\"Text\"", 5*CLOCK_SECOND);

/*
 * A handler function named [resource name]_handler must be implemented for each RESOURCE.
 * A buffer for the response payload is provided through the buffer pointer. Simple resources can ignore
 * preferred_size and offset, but must respect the REST_MAX_CHUNK_SIZE limit for the buffer.
 * If a smaller block size is requested for CoAP, the REST framework automatically splits the data.
 */
void
temp_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  /* we save the message as static variable, so it is retained through multiple calls (chunked resource) */
  static char message[TEMP_MSG_MAX_SIZE];
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

    if ((size_msg = create_response(num, accept[0], message)) <= 0)
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

    memcpy(buffer, message + *offset, length);

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
    memcpy(buffer, message + *offset, length);
    *offset = -1;
  }

  REST.set_header_etag(response, (uint8_t *) &length, 1);
  REST.set_response_payload(response, buffer, length);

}

void temp_periodic_handler(resource_t *r)
{
  static char new_value[TEMP_BUFF_MAX];
  static uint8_t obs_counter = 0;
  size_t size_msg;

  temp_to_buff(new_value);

  if (strcmp(new_value, tempstring) != 0)
  {
    if ((size_msg = create_response(1, REST.type.APPLICATION_XML, tempstring)) <= 0)
    {
      return;
    }
    
    /* Build notification. */
    coap_packet_t notification[1]; /* This way the packet can be treated as pointer as usual. */
    coap_init_message(notification, COAP_TYPE_NON, CONTENT_2_05, 0 );
    coap_set_payload(notification, tempstring, size_msg);

    /* Notify the registered observers with the given message type, observe option, and payload. */
    REST.notify_subscribers(r, obs_counter, notification);
  }
}
#endif

#if REST_RES_METER
PERIODIC_RESOURCE(meter, METHOD_GET, "meter", "title=\"Hello meter: ?len=0..\";rt=\"Text\"", CLOCK_SECOND);
uint8_t random_add, random_remove;
/* buffers */
uint8_t seconds_buff[SEC_BUFF_SIZE];
uint8_t minutes_buff[MIN_BUFF_SIZE];
uint8_t minutesX10_buff[MINX10_BUFF_SIZE];
/* counter for circular buffers */
int seconds_buff_ctr = 0;
int minutes_buff_ctr = 0;
int minutesX10_buff_ctr = 0;

void meter_init()
{
  PRINTF("meter_init: will now initialize\n");
  random_init(0);
  random_add = 0;
  random_remove = random_rand() % 10;

  /* init buffers */
  memset(seconds_buff, 0, SEC_BUFF_SIZE);
  memset(minutes_buff, 0, MIN_BUFF_SIZE);
  memset(minutesX10_buff, 0, MINX10_BUFF_SIZE);
  PRINTF("meter_init: successful\n");
}

void print_meter_buffers()
{
  PRINTF("Position of seconds buffer: %d\n", seconds_buff_ctr);
  PRINTF("Position of minutes buffer: %d\n", minutes_buff_ctr);
  PRINTF("Position of 10 minutes buffer: %d\n", minutesX10_buff_ctr);

  PRINTF("Current seconds value: %d\n", seconds_buff[seconds_buff_ctr]);
  PRINTF("Current minutes value: %d\n", minutes_buff[minutes_buff_ctr]);
  PRINTF("Current minutesX10 value: %d\n", minutesX10_buff[minutesX10_buff_ctr]);
  
  PRINTF("Full seconds buffer:\n");
  int i;
  for (i = 0; i < seconds_buff_ctr; i++)
  {
    PRINTF("[%d] ", seconds_buff[i]);
  }
  PRINTF("\n");

}

#if 0
uint8_t create_meter_response(int num, int accept, char *buffer)
{
  size_t size_temp;
  int size_msgp1, size_msgp2;
  const char *msgp1, *msgp2;
  uint8_t size_msg;

  if (num && accept==REST.type.APPLICATION_XML)
  {
    msgp1 = "see mote output";
    msgp2 = "";
    /* hardcoded length, ugly but faster and necc. for exi-answer */
    size_msgp1 = 0;
    size_msgp2 = 0;
  }
  else if (num && accept==REST.type.APPLICATION_EXI)
  {
    msgp1 = "\x80\x10\x01\x04\x6F\x62\x6A\x01\x01\x00\x02\x14\x68\x74\x74\x70\x3A\x2F\x2F\x6D\x79\x68\x6F\x6D\x65\x2F\x74\x65\x6D\x70\x01\x02\x01\x05\x72\x65\x61\x6C\x01\x01\x00\x08\x06\x74\x65\x6D\x70\x01\x01\x01\x06\x75\x6E\x69\x74\x73\x14\x6F\x62\x69\x78\x3A\x75\x6E\x69\x74\x73\x2F\x63\x65\x6C\x73\x69\x75\x73\x02\x01\x01\x00\x11\x09";
    msgp2 = "\x03\x00\x00\0";
    /* hardcoded length, ugly but faster and necc. for exi-answer */
    size_msgp1 = 81;
    size_msgp2 = 3;
  }
  else
  {
    PRINTF("Unsupported encoding!\n");
    return -1;
  }

  size_temp = 0;

  /* Some data that has the length up to REST_MAX_CHUNK_SIZE. For more, see the chunk resource. */
  /* we assume null-appended strings behind msgp2 and tempstring */
  size_msg = size_msgp1 + size_msgp2 + size_temp + 1;

  if (size_msg > TEMP_MSG_MAX_SIZE)
  {
    PRINTF("Message too big!\n");
    return -1;
  }

  memcpy(buffer, msgp1, size_msgp1);
  memcpy(buffer + size_msgp1, tempstring, size_temp);
  memcpy(buffer + size_msgp1 + size_temp, msgp2, size_msgp2 + 1);

  return size_msg;
}
#endif

void
meter_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  print_meter_buffers();
#if 0
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

    if ((size_msg = create_meter_response(num, accept[0], meter_message)) <= 0)
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
#endif

}

void meter_periodic_handler(resource_t *r)
{
  int8_t value = 0;
  int new_value = 0;
  /* if random_add counter is 0 or less, add another 'consumer' */
  if (random_add-- <= 0)
  {
    value = (random_rand() % 100);
    random_add = random_rand() % 20;
  }
  /* although it seems wrong, we don't want to add and remove in the same step, so we use 'else if'
  and just delay the removal */
  else if (random_remove-- <= 0)
  {
    value = -(random_rand() % 100);
    random_remove = random_rand() % 15;
  }

  new_value = seconds_buff[seconds_buff_ctr];
  if (value != 0)
  {
    new_value = new_value + value;

    new_value = MAX(0, new_value);
    new_value = MIN(255, new_value);
  }

  seconds_buff_ctr = (seconds_buff_ctr + 1 == SEC_BUFF_SIZE ? 0 : seconds_buff_ctr + 1);

  seconds_buff[seconds_buff_ctr] = new_value; 
  
  /* decide which higher order buffers get updated as well */
  if (seconds_buff_ctr % 60 == 0)
  {
    minutes_buff_ctr = (minutes_buff_ctr + 1 == MIN_BUFF_SIZE ? 0 : minutes_buff_ctr + 1);
    minutes_buff[minutes_buff_ctr] = new_value;

    if (minutes_buff_ctr % 10 == 0)
    {
      minutesX10_buff_ctr = (minutesX10_buff_ctr + 1 == MINX10_BUFF_SIZE ? 0 :minutes_buff_ctr + 1);
      minutesX10_buff[minutesX10_buff_ctr++] = new_value;
    }
  }

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

#if REST_RES_TEMP
  rest_activate_periodic_resource(&periodic_resource_temp);
  tmp102_init();
#endif

#if REST_RES_METER
  rest_activate_periodic_resource(&periodic_resource_meter);
  meter_init();
#endif

  /* Define application-specific events here. */
  while(1) {
    PROCESS_WAIT_EVENT();
  } /* while (1) */

  PROCESS_END();
}
