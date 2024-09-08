# ==== Build Config  ========================================================

# Only incuding in case a literal GNU Makefile is required, otherwise
# see CMakeLists.txt

EXEC ?= aesdsocket
CROSS_COMPILE ?=

SRC_DIRS ?= .
INCLUDE_DIRS ?= 
BUILD_DIR ?= ./$(CROSS_COMPILE)build

SRCS := $(shell find $(SRC_DIRS) -name *.c)
OBJS := $(SRCS:%=$(BUILD_DIR)/%.o)

CC ?= gcc
CFLAGS ?= -Wall -Werror -Wextra -Wcast-align -Wcast-qual -Winit-self \
		  -Wlogical-op -Wshadow -Wsign-conversion -Wswitch-default -Wundef \
		  -Wunused -pedantic
LDFLAGS += -lrt -lpthread

# Be GNU-like
DESTDIR ?= /
BINDIR ?= /usr/bin
INITDIR ?= /etc/init.d

INSTALL_BINS := $(BUILD_DIR)/$(EXEC)
INSTALL_INIT := aesdsocket-start-stop
INIT_PRIO := S99

# ==== Build Profiles =========================================================

.PHONY: all clean

all: release

release: CFLAGS += -O2
release: $(EXEC)

debug: CFLAGS += -Og -g -DDEBUG
debug: $(EXEC)

# ==== Build Chain ============================================================

$(EXEC): $(BUILD_DIR)/$(EXEC)
	@ln -sf $(BUILD_DIR)/$(EXEC) $(EXEC)

$(BUILD_DIR)/$(EXEC): $(OBJS)
	$(CROSS_COMPILE)$(CC) -o $@ $(OBJS) $(CFLAGS) $(LDFLAGS)

$(BUILD_DIR)/%.c.o: %.c
	@mkdir -p $(dir $@)
	$(CROSS_COMPILE)$(CC) $(CFLAGS) -c $< -o $@

# ==== Install ================================================================

.PHONY: install install_bins install_init
install: install_bins install_init

install_bins: $(INSTALL_BINS)
ifdef DESTDIR
	install -d $(DESTDIR)$(BINDIR)
	for f in $^; do install -m 0755 $$f $(DESTDIR)$(BINDIR)/; done
endif

install_init: $(INSTALL_INIT)
ifdef DESTDIR
	install -d $(DESTDIR)$(INITDIR)
	install -m 0755 $(INSTALL_INIT) $(DESTDIR)$(INITDIR)/$(INIT_PRIO)$(EXEC)
endif

# ==== Cleanup ================================================================

.PHONY: clean
clean:
	@$(RM) -rf $(EXEC) $(BUILD_DIR)