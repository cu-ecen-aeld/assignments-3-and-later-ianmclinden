# 
# Comments:
#  Not a stolen Makefile, just templated a quick one based off
#  my industry experience
#

# ==== Build Config  ========================================================

EXEC ?= writer
CROSS_COMPILE ?=

SRC_DIRS ?= .
INCLUDE_DIRS ?= 
BUILD_DIR ?= ./$(CROSS_COMPILE)build

SRCS := $(shell find $(SRC_DIRS) -name *.c)
OBJS := $(SRCS:%=$(BUILD_DIR)/%.o)

CC ?= gcc

# Be GNU-like
DESTDIR ?= /
PKGCONFDIR ?= /etc/finder-app
BINDIR ?= /usr/bin

INSTALL_BINS := $(BUILD_DIR)/$(EXEC) finder-test.sh finder.sh
INSTALL_CONF := conf/username.txt conf/assignment.txt

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

.PHONY: install install_bins install_conf
install: install_bins install_conf

install_bins: $(INSTALL_BINS)
ifdef DESTDIR
	install -d $(DESTDIR)$(BINDIR)
	for f in $^; do install -m 0755 $$f $(DESTDIR)$(BINDIR)/; done
endif

install_conf: $(INSTALL_CONF)
ifdef DESTDIR
	install -d $(DESTDIR)$(PKGCONFDIR)
	for f in $^; do install -Dm 0644 $$f $(DESTDIR)$(PKGCONFDIR)/$$f; done
endif

# ==== Cleanup ================================================================

.PHONY: clean
clean:
	@$(RM) -rf $(EXEC) $(BUILD_DIR)