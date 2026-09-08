PREFIX ?= $(HOME)/.local
BUILD_DIR ?= build
JOBS ?= $(shell nproc 2>/dev/null || echo 4)

.PHONY: all build install uninstall start clean

all: build

build:
	cmake -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=$(PREFIX)
	cmake --build $(BUILD_DIR) -j$(JOBS)

install: build
	cmake --install $(BUILD_DIR)

uninstall:
	./install.sh --uninstall

start: install
	./install.sh --start

clean:
	rm -rf $(BUILD_DIR)
