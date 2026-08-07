PYTHON ?= python

.PHONY: all help list check build-all build-active build-32k build-internal-flash

all: help

help:
	@echo "Hardware test commands:"
	@echo "  make -C hw_tests list                 List cataloged firmware tests"
	@echo "  make -C hw_tests check                Validate catalogs and boundaries"
	@echo "  make -C hw_tests build-active         Build active firmware tests"
	@echo "  make -C hw_tests build-32k            Build preserved 32K feasibility tests"
	@echo "  make -C hw_tests build-all            Build all targets and compatibility aliases"

list:
	@$(PYTHON) ../tools/check_test_catalog.py --list-hardware

check:
	@$(PYTHON) ../tools/check_test_catalog.py
	@$(PYTHON) ../tools/check_hw_tests.py

build-active:
	@$(PYTHON) ../tools/build_hw_test_matrix.py --group active --force --keep-going

build-32k:
	@$(PYTHON) ../tools/build_hw_test_matrix.py --group feasibility --force --keep-going

build-all:
	@$(PYTHON) ../tools/build_hw_test_matrix.py --group all --force --include-aliases --keep-going

build-internal-flash:
	@$(MAKE) -C h417/v3f/internal_flash_cdc
