include ../../common.mk

all: config
	$(MAKE)

config:
	mkdir -p $(ROOTDIR)/include/net-snmp/
	cp -avf $(ROOTDIR)/packages/libconfig/net-snmp-config.h $(ROOTDIR)/include/net-snmp/net-snmp-config.h
	ln -Tsf $(ROOTDIR)/packages/cish $(ROOTDIR)/include/cish
	ln -Tsf $(ROOTDIR)/packages/libconfig $(ROOTDIR)/include/libconfig

libconfig_basic: config
	$(MAKE) libconfig_basic
	cp -avf libconfig.* $(ROOTDIR)/lib/

libconfig.so:
	$(MAKE) libconfig.so
	cp -avf libconfig.* $(ROOTDIR)/lib/

libconfigsnmp.so:
	$(MAKE) libconfigsnmp.so

install:
	$(MAKE) install

clean:
	$(MAKE) clean

distclean:
	$(MAKE) clean
