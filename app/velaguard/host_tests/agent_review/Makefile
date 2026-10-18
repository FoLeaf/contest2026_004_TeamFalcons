#############################################################################
# app/velaguard/host_tests/agent_review/Makefile
#
# Offline fault-injection tests for the device-side agent tool path:
#   test_runner_budget      tool_shell_runner.c  execution budget and reclaim
#   test_round_isolation    vg_agent_round.c     request correlation, reclaim
#   test_file_policy_extra  tool_files.c         path policy corners
#   test_shell_gate         tool_shell.c         product command gate
#   repro_symlink_write_escape                   F-01 write-path regression
#
# This directory is self-contained on purpose.  host_tests/Makefile is being
# edited by other workstreams, so nothing here is wired into it and no file
# outside this directory is written.
#
#   make -C app/velaguard/host_tests/agent_review test    # green regression set
#   make -C app/velaguard/host_tests/agent_review repro   # F-01 alone
#
# The openvela checkouts (nuttx, apps, packages) are read-only inputs here.
#############################################################################

CC       ?= gcc
CFLAGS   ?= -Wall -Werror
CFLAGS   += -pthread

ROOT      := ../../../../..
AI_AGENT  := $(ROOT)/packages/ai_agent
VG        := ../..
STUBS     := ../stubs
CJSON     := $(ROOT)/apps/netutils/cjson/cJSON/cJSON.c
CJSONINC  := $(ROOT)/apps/netutils/cjson/cJSON

# The ai_agent tools are NuttX sources: they take OK/ERROR from <sys/types.h>
# and pull agent_config.h / agent_compat.h from the ai_agent tree.
AI_CFLAGS := $(CFLAGS) -DOK=0 -DERROR=-1 \
             -I$(AI_AGENT)/tests/stub -I$(AI_AGENT)/include \
             -I$(AI_AGENT)/src -I$(CJSONINC)

# Not named SHELL: make reserves that variable for the recipe interpreter.
RUNNER     := $(AI_AGENT)/src/tools/tool_shell_runner.c
TOOL_SHELL := $(AI_AGENT)/src/tools/tool_shell.c
FILES      := $(AI_AGENT)/src/tools/tool_files.c

.PHONY: test repro clean

test: test_runner_budget test_round_isolation test_file_policy_extra \
      test_shell_gate test_tool_guard_rate repro_symlink_write_escape
	./test_runner_budget
	./test_round_isolation
	./test_file_policy_extra
	./test_shell_gate
	./test_tool_guard_rate
	./repro_symlink_write_escape
	@echo "agent_review: regression set OK"

repro: repro_symlink_write_escape
	./repro_symlink_write_escape

test_runner_budget: test_runner_budget.c $(RUNNER)
	$(CC) $(CFLAGS) -I$(AI_AGENT)/src -o $@ test_runner_budget.c $(RUNNER)

test_round_isolation: test_round_isolation.c $(VG)/vg_agent_round.c
	$(CC) $(CFLAGS) -I$(STUBS) -I$(VG) -I$(AI_AGENT)/include -o $@ \
	  test_round_isolation.c $(VG)/vg_agent_round.c -Wl,--wrap=clock_gettime

test_file_policy_extra: test_file_policy_extra.c $(FILES) $(CJSON)
	$(CC) $(AI_CFLAGS) \
	  '-DCONFIG_EXAMPLES_AI_AGENT_VELA_DATA_DIR="data/agent"' \
	  '-DVG_DATA_ROOT="data/velaguard"' \
	  -o $@ test_file_policy_extra.c $(FILES) $(CJSON) -lm

test_shell_gate: test_shell_gate.c $(TOOL_SHELL) $(RUNNER) $(CJSON)
	$(CC) $(AI_CFLAGS) -DCONFIG_VG_HMI -o $@ \
	  test_shell_gate.c $(RUNNER) $(CJSON) -lm

test_tool_guard_rate: $(AI_AGENT)/tests/test_tool_guard_rate.c \
                      $(AI_AGENT)/src/tools/tool_guard.c $(CJSON)
	$(CC) $(AI_CFLAGS) -o $@ $(AI_AGENT)/tests/test_tool_guard_rate.c \
	  $(CJSON) -Wl,--wrap=time -lm

repro_symlink_write_escape: repro_symlink_write_escape.c $(FILES) $(CJSON)
	$(CC) $(AI_CFLAGS) \
	  '-DCONFIG_EXAMPLES_AI_AGENT_VELA_DATA_DIR="data/agent"' \
	  '-DVG_DATA_ROOT="data/velaguard"' \
	  -o $@ repro_symlink_write_escape.c $(FILES) $(CJSON) -lm

clean:
	rm -f test_runner_budget test_round_isolation test_file_policy_extra \
	      test_shell_gate test_tool_guard_rate repro_symlink_write_escape
