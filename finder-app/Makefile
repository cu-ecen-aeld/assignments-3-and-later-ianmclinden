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

CC = $(CROSS_COMPILE)gcc

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
	$(CC) -o $@ $(OBJS) $(CFLAGS) $(LDFLAGS)

$(BUILD_DIR)/%.c.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# ==== Cleanup ================================================================

.PHONY: clean
clean:
	@$(RM) -rf $(EXEC) $(BUILD_DIR)