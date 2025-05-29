MAKEFLAGS += --no-print-directory
PREFIX ?= /usr
BINDIR ?= $(PREFIX)/bin
MANDIR ?= $(PREFIX)/share/man
PKG_CONFIG ?= pkg-config

MKDIR ?= $(shell pwd)
INC_DIR ?= $(MKDIR)/include
SRC_DIR ?= $(MKDIR)/src
OBJ_DIR ?= $(MKDIR)/src
BIN_DIR ?= $(MKDIR)

SRC ?= ${wildcard ${SRC_DIR}/*.c}
OBJS ?= ${patsubst %.c, $(OBJ_DIR)/%.o, ${notdir ${SRC}}}


TARGET ?= btrf
BIN_TARGET ?= ${BIN_DIR}/${TARGET}

CC ?= gcc
CFLAGS ?= -O2 -g
CFLAGS += -Wall -Wundef -Wstrict-prototypes -Wno-trigraphs -fno-strict-aliasing -fno-common -Werror-implicit-function-declaration
CFLAGS += -I${INC_DIR}

ifeq ($(V),1)
Q=
NQ=true
else
Q=@
NQ=echo
endif

${OBJ_DIR}/%.o: ${SRC_DIR}/%.c
	@$(NQ) ' CC  ' $@
	$(Q)$(CC) $(CFLAGS) -c -o $@ $<

${BIN_TARGET}: $(OBJS)
	@$(NQ) ' CC  ' btrf
	$(Q)$(CC) $(LDFLAGS) $(OBJS) -lpthread -o btrf


clean:
	$(Q)rm -f btrf $(OBJ_DIR)/*.o *~
