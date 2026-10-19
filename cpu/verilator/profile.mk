# Called by ucc/profile with the source directory as the working directory.
ROOT := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/../..)
PROFILE_MAKEFILE := $(lastword $(MAKEFILE_LIST))
UCC := $(ROOT)/ucc/cc
UAS := $(ROOT)/uas/as
BASE := .profile/$(basename $(SOURCE))
ASM := $(BASE).s
DEPS := $(BASE).d
PMEM := $(BASE).pmem.hex
DMEM := $(BASE).dmem.hex
MAP := $(BASE).map

# Escape spaces in Make target/prerequisite lists. Recipes use shell quotes.
empty :=
space := $(empty) $(empty)
escape = $(subst $(space),\$(space),$(1))

.DEFAULT_GOAL := all
.DELETE_ON_ERROR:
.PHONY: all
all: $(call escape,$(PMEM)) $(call escape,$(DMEM)) $(call escape,$(MAP))

$(call escape,$(ASM)) $(call escape,$(DEPS)) &: $(call escape,$(SOURCE)) $(call escape,$(UCC)) $(call escape,$(PROFILE_MAKEFILE))
	mkdir -p .profile
	$(CC) -MM -MP -MQ "$(ASM)" -MF "$(DEPS)" "$(SOURCE)"
	"$(UCC)" -o "$(ASM)" "$(SOURCE)"

# Grouped targets: one assembler invocation produces all three outputs.
$(call escape,$(PMEM)) $(call escape,$(DMEM)) $(call escape,$(MAP)) &: $(call escape,$(ASM)) $(call escape,$(UAS)) $(call escape,$(PROFILE_MAKEFILE))
	"$(UAS)" --pmem "$(PMEM)" --dmem "$(DMEM)" --map "$(MAP)" "$(ASM)"

-include $(call escape,$(DEPS))
