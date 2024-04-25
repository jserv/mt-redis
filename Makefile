# Top level makefile, the real shit is at src/Makefile

default: all

export OPTIMIZATION := -O0
export DEBUG_FLAGS := -g -ggdb

.DEFAULT:
	cd src && $(MAKE) $@

install:
	cd src && $(MAKE) $@

.PHONY: install
