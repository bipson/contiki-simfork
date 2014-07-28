all: er-obix-gpost-server er-obix-server er-obix-group-uc-sender er-obix-requester er-obix-group-mc-sender hop

# Use this target explicitly if requried: er-plugtest-server

#PROJECT_SOURCEFILES += static-routing.c 

#for powertrace
WITH_POWERTRACE=0

# FLOOD, SMRF, MPL
IOTSYS_MC_ENGINE=SMRF

# variable for Makefile.include
WITH_UIP6=1
UIP_CONF_IPV6=1
CFLAGS += -DUIP_CONF_IPV6=1
CFLAGS += -DUIP_CONF_IPV6_RPL=1

CONTIKI=../..
CFLAGS += -DPROJECT_CONF_H=\"project-conf.h\"

# linker optimizations
SMALL=1

APPS += er-coap
APPS += rest-engine

ifeq ($(WITH_POWERTRACE), 1)
CFLAGS += -DWITH_POWERTRACE=1
APPS += powertrace
endif

ifeq ($(IOTSYS_MC_ENGINE), SMRF)
MODULES += core/net/ipv6/multicast
CFLAGS += -DUIP_MCAST6_CONF_ENGINE=UIP_MCAST6_ENGINE_SMRF
CFLAGS += -DIOTSYS_GC_FLOOD=0
endif
ifeq ($(IOTSYS_MC_ENGINE), MPL)
MODULES += core/net/ipv6/multicast
CFLAGS += -DUIP_MCAST6_CONF_ENGINE=UIP_MCAST6_ENGINE_ROLL_TM
CFLAGS += -DIOTSYS_GC_FLOOD=0
endif
ifeq ($(IOTSYS_MC_ENGINE), FLOOD)
CFLAGS += -DUIP_MCAST6_CONF_ENGINE=UIP_MCAST6_ENGINE_NONE
CFLAGS += -DIOTSYS_GC_FLOOD=1
endif

# optional rules to get assembly
#CUSTOM_RULE_C_TO_OBJECTDIR_O = 1
#CUSTOM_RULE_S_TO_OBJECTDIR_O = 1

include $(CONTIKI)/Makefile.include

# optional rules to get assembly
#$(OBJECTDIR)/%.o: asmdir/%.S
#	$(CC) $(CFLAGS) -MMD -c $< -o $@
#	@$(FINALIZE_DEPENDENCY)
#
#asmdir/%.S: %.c
#	$(CC) $(CFLAGS) -MMD -S $< -o $@

# border router rules
$(CONTIKI)/tools/tunslip6:	$(CONTIKI)/tools/tunslip6.c
	(cd $(CONTIKI)/tools && $(MAKE) tunslip6)

connect-router:	$(CONTIKI)/tools/tunslip6
	sudo $(CONTIKI)/tools/tunslip6 aaaa::1/64

connect-router-cooja:	$(CONTIKI)/tools/tunslip6
	sudo $(CONTIKI)/tools/tunslip6 -a 127.0.0.1 aaaa::1/64

tap0up:
	sudo ip address add fdfd::1/64 dev tap0
