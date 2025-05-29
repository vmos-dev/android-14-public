MAKEFLAGS += --no-print-directory

PREFIX ?= /usr
BINDIR ?= $(PREFIX)/bin
MANDIR ?= $(PREFIX)/share/man
PKG_CONFIG ?= pkg-config

ROAM_OFFLOAD ?= 0

MKDIR ?= mkdir -p
CC ?= "gcc"

CFLAGS ?= -O2 -g
CFLAGS += -Wall -Wundef -Wstrict-prototypes -Wno-trigraphs -fno-strict-aliasing -fno-common -Werror-implicit-function-declaration

OBJS = besphy_rf.o cmd.o event.o
ALL = besphy


ifeq ($(ROAM_OFFLOAD),1)
CFLAGS += -DROAM_OFFLOAD
endif

NL1FOUND := $(shell $(PKG_CONFIG) --atleast-version=1 libnl-1 && echo Y)
NL2FOUND := $(shell $(PKG_CONFIG) --atleast-version=2 libnl-2.0 && echo Y)
NL3FOUND := $(shell $(PKG_CONFIG) --atleast-version=3 libnl-3.0 && echo Y)

ifeq ($(NL1FOUND),Y)
NLLIBNAME = libnl-1
endif

ifeq ($(NL2FOUND),Y)
CFLAGS += -DCONFIG_LIBNL20
LIBS += -lnl-genl
NLLIBNAME = libnl-2.0
endif

ifeq ($(NL3FOUND),Y)
CFLAGS += -DCONFIG_LIBNL30
#LIBS += -lnl-genl
LIBS += -lnl-genl-3
NLLIBNAME = libnl-3.0
endif  

ifeq ($(NLLIBNAME),)
$(error Cannot find development files for any supported version of libnl)
endif

$(info "libname is $(NLLIBNAME)")


LIBS += $(shell pkg-config --libs $(NLLIBNAME))
CFLAGS += $(shell pkg-config --cflags $(NLLIBNAME))



ifeq ($(V),1)
Q=
NQ=true
else
Q=@
NQ=echo
endif

all: version_check $(ALL)

version_check:
ifeq ($(NL2FOUND),Y)
else
ifeq ($(NL1FOUND),Y)
else
ifeq ($(NL3FOUND),Y)
else
	$(error No libnl found)
endif
endif
endif


$(info "LIBS is $(LIBS)")

%.o: %.c nl80211.h
	@$(NQ) ' CC  ' $@
	$(Q)$(CC) $(CFLAGS) -c -o $@ $<

besphy:	$(OBJS)
	@$(NQ) ' CC  ' besphy
	$(Q)$(CC) $(LDFLAGS) $(OBJS) $(LIBS) -o besphy
#	$(Q)$(CC) $(LDFLAGS) $(OBJS) -lnl-genl-3 -lnl-3 -o besphy

check:
	$(Q)$(MAKE) all CC="REAL_CC=$(CC) CHECK=\"sparse -Wall\" cgcc"

clean:
	$(Q)rm -f besphy *.o *~
