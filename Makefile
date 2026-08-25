CMAKE ?= cmake
CTEST ?= ctest
JOBS ?= $(shell nproc 2>/dev/null || echo 2)

DEBUG_BUILD ?= build-debug
RELEASE_BUILD ?= build-release
PDF_BUILD ?= build-debug-pdf
INSTALL_PREFIX ?= $(CURDIR)/install

.PHONY: help refresh debug test release install pdf-test clean

help:
	@printf '%s\n' \
		'make refresh   Initialize pinned Git submodules' \
		'make debug     Configure and build with debug checks and tests' \
		'make test      Build debug and run the test suite' \
		'make release   Configure and build an optimized release' \
		'make install   Install the release into INSTALL_PREFIX' \
		'make pdf-test  Build and test with optional PDFium enabled' \
		'make clean     Remove Makefile-managed build/install trees'

refresh:
	git submodule sync --recursive
	git submodule update --init --recursive

debug:
	$(CMAKE) -S . -B $(DEBUG_BUILD) \
		-DCMAKE_BUILD_TYPE=Debug \
		-DTAHOMA_VISION_BUILD_TESTS=ON \
		-DTAHOMA_VISION_INSTALL=OFF
	$(CMAKE) --build $(DEBUG_BUILD) --parallel $(JOBS)

test: debug
	$(CTEST) --test-dir $(DEBUG_BUILD) --output-on-failure

release:
	$(CMAKE) -S . -B $(RELEASE_BUILD) \
		-DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_INSTALL_PREFIX=$(INSTALL_PREFIX) \
		-DTAHOMA_VISION_BUILD_TESTS=OFF \
		-DTAHOMA_VISION_INSTALL=ON
	$(CMAKE) --build $(RELEASE_BUILD) --parallel $(JOBS)

install: release
	$(CMAKE) --install $(RELEASE_BUILD)

pdf-test:
	$(CMAKE) -S . -B $(PDF_BUILD) \
		-DCMAKE_BUILD_TYPE=Debug \
		-DTAHOMA_VISION_PDF=ON \
		-DTAHOMA_VISION_BUILD_TESTS=ON \
		-DTAHOMA_VISION_INSTALL=OFF
	$(CMAKE) --build $(PDF_BUILD) --parallel $(JOBS)
	$(CTEST) --test-dir $(PDF_BUILD) --output-on-failure

clean:
	$(CMAKE) -E rm -rf \
		$(DEBUG_BUILD) $(RELEASE_BUILD) $(PDF_BUILD) $(INSTALL_PREFIX)
