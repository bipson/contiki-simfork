#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "contiki.h"
#include "contiki-net.h"
#include "rest.h"

#define REST_RES_METER 1
#define REST_RES_HELLOWORLD 0

#if REST_RES_METER
#include "rest-server-example.h"
#endif /* REST_RES_METER */

#define DEBUG 1
#if DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#define PRINT6ADDR(addr) PRINTF(" %02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x ", ((uint8_t *)addr)[0], ((uint8_t *)addr)[1], ((uint8_t *)addr)[2], ((uint8_t *)addr)[3], ((uint8_t *)addr)[4], ((uint8_t *)addr)[5], ((uint8_t *)addr)[6], ((uint8_t *)addr)[7], ((uint8_t *)addr)[8], ((uint8_t *)addr)[9], ((uint8_t *)addr)[10], ((uint8_t *)addr)[11], ((uint8_t *)addr)[12], ((uint8_t *)addr)[13], ((uint8_t *)addr)[14], ((uint8_t *)addr)[15])
#define PRINTLLADDR(lladdr) PRINTF(" %02x:%02x:%02x:%02x:%02x:%02x ",(lladdr)->addr[0], (lladdr)->addr[1], (lladdr)->addr[2], (lladdr)->addr[3],(lladdr)->addr[4], (lladdr)->addr[5])
#else
#define PRINTF(...)
#define PRINT6ADDR(addr)
#define PRINTLLADDR(addr)
#endif

#if REST_RES_METER
static char *msg[RESOURCES_SIZE][NR_ENCODINGS];
static uint16_t msg_size[RESOURCES_SIZE][NR_ENCODINGS];

/********************/
/* helper functions */
/********************/
void
set_error_response(void* response, int8_t error_code)
{
  char *err_msg;

  switch(error_code)
  {
    case ERR_BLOCKOUTOFSCOPE:
      /* A block error message should not exceed the minimum block size (16). */
      err_msg = "BlockOutOfScope";
      break;
    case ERR_WRONGCONTENTTYPE:
      PRINTF("wrong content-type!\n");
      err_msg = "Supporting content-types application/xml and application/exi";
      break;
    case ERR_ALLOC:
      PRINTF("ERROR while creating message!\n");
      err_msg = "ERROR while creating message :\\";
      break;
    default:
      PRINTF("AHOYHOY?!\n");
      err_msg = "creating message error, this should not happen :\\";
  }

  rest_set_header_content_type(response, TEXT_PLAIN);
  rest_set_response_payload(response, (uint8_t *)err_msg, strlen(err_msg));
  return;
}

int16_t
create_response(const char **message, uint8_t resource, void *request, void *response, int8_t *encoding)
{
  content_type_t accept = 0;

  /* decide upon content-format */
  accept = rest_get_header_accept(request);
  
  if (accept==APPLICATION_XML || accept==APPLICATION_EXI)
  {
    if (accept == APPLICATION_XML)
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
    rest_set_header_content_type(response, *encoding);
    PRINTF("size found: %d\n", msg_size[resource][*encoding]);
    PRINTF("text found: %s\n", msg[resource][*encoding]);
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
RESOURCE(meter, METHOD_GET, "smart-meter");

void
meter_handler(REQUEST* request, RESPONSE* response)
{
  int8_t acc_encoding = -1;
  int8_t message_size = -1;
  const char *meter_message;

  PRINTF("Will create response\n");
  if ((message_size = create_response(&meter_message, RESOURCES_METER, request, response, &acc_encoding)) <= 0)
  {
    PRINTF("Error caught: %d\n", message_size);
    set_error_response(response, message_size);
    return;
  }

  PRINTF("Will send message: %s\n", meter_message);
  PRINTF("Encoding: %d\n", acc_encoding);
  PRINTF("Size: %d\n", message_size);
  rest_set_response_payload(response, (uint8_t*)meter_message, message_size);
  return;
}
#endif /* REST_RES_METER */

#if REST_RES_HELLOWORLD
char temp[100];

/* Resources are defined by RESOURCE macro, signature: resource name, the http methods it handles and its url*/
RESOURCE(helloworld, METHOD_GET, "helloworld");

/* For each resource defined, there corresponds an handler method which should be defined too.
 * Name of the handler method should be [resource name]_handler
 * */
void
helloworld_handler(REQUEST* request, RESPONSE* response)
{
  sprintf(temp,"Hello World!\n");

  rest_set_header_content_type(response, TEXT_PLAIN);
  rest_set_response_payload(response, (uint8_t*)temp, strlen(temp));
}
#endif /* REST_RES_HELLOWORLD */

RESOURCE(discover, METHOD_GET, ".well-known/core");
void
discover_handler(REQUEST* request, RESPONSE* response)
{
  char temp[100];
  int index = 0;

#if REST_RES_METER
  index += sprintf(temp + index, "%s,", "</meter>;n=\"SmartMeter\"");
#endif /* REST_RES_METER */
#if REST_RES_HELLOWORLD
  index += sprintf(temp + index, "%s,", "</helloworld>;n=\"HelloWorld\"");
#endif /* REST_RES_HELLOWORLD */

  rest_set_response_payload(response, (uint8_t*)temp, strlen(temp));
  rest_set_header_content_type(response, APPLICATION_LINK_FORMAT);
}

PROCESS(rest_server_example, "Rest Server Example");
AUTOSTART_PROCESSES(&rest_server_example);

PROCESS_THREAD(rest_server_example, ev, data)
{
  PROCESS_BEGIN();

#ifdef WITH_COAP
  PRINTF("COAP Server\n");
#else
  PRINTF("HTTP Server\n");
#endif

  rest_init();

#if REST_RES_METER
  rest_activate_resource(&resource_meter);
  msg[RESOURCES_METER][ENCODING_XML] = "value xml\0";
  msg_size[RESOURCES_METER][ENCODING_XML] = 9;
#endif /* REST_RES_METER */
  
#if REST_RES_HELLOWORLD
  rest_activate_resource(&resource_helloworld);
#endif /* REST_RES_HELLOWORLD */

  rest_activate_resource(&resource_discover);

  PROCESS_END();
}
