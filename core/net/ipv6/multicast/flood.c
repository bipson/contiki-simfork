/*
 * Copyright (c) 2010, Institute of Computer Aided Automation, TU Vienna
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
 *         This file implements a (very) simple multicast flooding algorithm
 *
 * \author
 *         Markus Jung - <mjung@auto.tuwien.ac.at>
 *         Philipp Raich - <praich@auto.tuwien.ac.at>
 */

/* TODO this is largely (shamelessly) copied from the SMRF code, check if
 * there is anything left to do (copyright)
 */

#include "contiki.h"
#include "contiki-net.h"
#include "net/ipv6/multicast/uip-mcast6.h"
#include "net/ipv6/multicast/uip-mcast6-route.h"
#include "net/ipv6/multicast/uip-mcast6-stats.h"
#include "net/ipv6/multicast/smrf.h"
#include "net/rpl/rpl.h"
#include "net/netstack.h"
#include <string.h>

#define DEBUG DEBUG_NONE
#include "net/ip/uip-debug.h"

#if UIP_CONF_IPV6
/*---------------------------------------------------------------------------*/
/* Macros */
/*---------------------------------------------------------------------------*/
/* CCI */
#define SMRF_FWD_DELAY()  NETSTACK_RDC.channel_check_interval()
/* Number of slots in the next 500ms */
#define SMRF_INTERVAL_COUNT  ((CLOCK_SECOND >> 2) / fwd_delay)
/*---------------------------------------------------------------------------*/
/* Internal Data */
/*---------------------------------------------------------------------------*/
static struct ctimer mcast_periodic;
static uint8_t mcast_len;
static uip_buf_t mcast_buf;
static uint8_t fwd_delay;
static uint8_t fwd_spread;
/*---------------------------------------------------------------------------*/
/* uIPv6 Pointers */
/*---------------------------------------------------------------------------*/
#define UIP_IP_BUF        ((struct uip_ip_hdr *)&uip_buf[UIP_LLH_LEN])
/*---------------------------------------------------------------------------*/
static void
mcast_fwd(void *p)
{
  memcpy(uip_buf, &mcast_buf, mcast_len);
  uip_len = mcast_len;
  UIP_IP_BUF->ttl--;
  tcpip_output(NULL);
  uip_len = 0;
}
/*---------------------------------------------------------------------------*/
static uint8_t
in()
{
  /* if packet not seen yet */
  if (&mcast_buf) {
    /* forward it */
    UIP_MCAST6_STATS_ADD(mcast_fwd);
    /* TODO use a random delay */
    fwd_delay = (random_rand() % MAX_DELAY) ;

    memcpy(&mcast_buf, uip_buf, uip_len);
    mcast_len = uip_len;
    ctimer_set(&mcast_periodic, fwd_delay, mcast_fwd, NULL);

    /* Done with this packet unless we are a member of the mcast group */
    if(!uip_ds6_is_my_maddr(&UIP_IP_BUF->destipaddr)) {
      PRINTF("FLOOD: Not a group member. No further processing\n");
      return UIP_MCAST6_DROP;
    } else {
      /* process and send upwards */
      PRINTF("FLOOD: Ours. Deliver to upper layers\n");
      UIP_MCAST6_STATS_ADD(mcast_in_ours);
      return UIP_MCAST6_ACCEPT;
    }

  /* else duplicate, thus drop */
  } else {
    UIP_MCAST6_STATS_ADD(mcast_dropped);
    return UIP_MCAST6_DROP;
  }
}
/*---------------------------------------------------------------------------*/
static void
init()
{
  UIP_MCAST6_STATS_INIT(NULL);

  uip_mcast6_route_init();
}
/*---------------------------------------------------------------------------*/
static void
out()
{
  return;
}
/*---------------------------------------------------------------------------*/
const struct uip_mcast6_driver flood_driver = {
  "FLOOD",
  init,
  out,
  in,
};
/*---------------------------------------------------------------------------*/

#endif /* UIP_CONF_IPV6 */
