#ifndef ER_SMART_METER_H
#define ER_SMART_METER_H

typedef enum {
  RESSOURCES_METER = 0,
  RESSOURCES_SIZE
} resource;

#define NR_ENCODINGS  51

#define ERR_BLOCKOUTOFSCOPE   -2
#define ERR_WRONGCONTENTTYPE  -3
#define ERR_ALLOC             -4

#define MSG_MAX_SIZE  1024

#endif /*ER_SMART_METER_H*/

