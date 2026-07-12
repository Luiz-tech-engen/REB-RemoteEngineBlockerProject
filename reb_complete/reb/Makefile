CC      = gcc
CFLAGS  = -std=c99 -Wall -Wextra -Werror -pedantic \
          -Wno-unused-parameter \
          -I. \
          -I./include

CORE_SRCS = \
    src/reb_core/event_log.c      \
    src/reb_core/nvm.c            \
    src/reb_core/security_manager.c \
    src/reb_core/panel_auth.c     \
    src/reb_core/sensor_fusion.c  \
    src/reb_core/powertrain_validation.c \
    src/reb_core/actuator_iface.c \
    src/reb_core/starter_control.c\
    src/reb_core/alert_manager.c  \
    src/reb_core/reversal_window.c\
	src/can/can_rx.c              \
	src/can/can_frame.c           \
    src/can/can_ids.c             \
    src/can/can_codec.c           \
    src/can/can_rx_dev.c          \
    src/can/can_tx.c              \
    src/can/can_monitor.c         \
    src/can/can_v3_adapter.c      \
    src/reb_core/fsm.c            \
    reb_main.c

CORE_OBJS = $(CORE_SRCS:.c=.o)

.PHONY: all clean test lib

all: lib test

lib: libreb.a

libreb.a: $(CORE_OBJS)
	ar rcs $@ $^
	@echo ">>> libreb.a created"

TEST_SRCS = tests/test_reb.c
TEST_BIN  = test_reb

test: $(TEST_BIN)
	@echo "\n>>> Running REB test suite..."
	./$(TEST_BIN)

$(TEST_BIN): $(CORE_SRCS) $(TEST_SRCS)
	$(CC) $(CFLAGS) -o $@ $^
	@echo ">>> Test binary compiled: $(TEST_BIN)"

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(CORE_OBJS) libreb.a $(TEST_BIN)
	@echo ">>> Cleanup completed"

misra:
	cppcheck --addon=misra --enable=all --error-exitcode=1 \
	         --suppress=missingIncludeSystem \
	         -I. -I./include \
	         $(CORE_SRCS)
	@echo ">>> MISRA C verification completed (0 violations = PASS)"

help:
	@echo "Available targets:"
	@echo "  all    — compiles lib and runs tests"
	@echo "  lib    — generates libreb.a for integration"
	@echo "  test   — compiles and runs test suite"
	@echo "  misra  — MISRA C verification (requires cppcheck)"
	@echo "  clean  — removes build artifacts"
