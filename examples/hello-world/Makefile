CONTIKI_PROJECT = hello-world
all: $(CONTIKI_PROJECT)

# variable for Makefile.include
WITH_UIP6=1
# for some platforms
UIP_CONF_IPV6=1
# IPv6 make config disappeared completely
CFLAGS += -DUIP_CONF_IPV6=1

CONTIKI = ../..
CFLAGS += -DPROJECT_CONF_H=\"project-conf.h\"
include $(CONTIKI)/Makefile.include

CFLAGS += -DUIP_CONF_IPV6_RPL=1
