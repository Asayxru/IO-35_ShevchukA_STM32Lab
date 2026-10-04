IMAGE ?= stm32-build
HOST_UID := $(shell id -u 2>/dev/null)
HOST_GID := $(shell id -g 2>/dev/null)
ifneq ($(HOST_UID),)
USER_FLAG := --user $(HOST_UID):$(HOST_GID)
endif

.PHONY: docker build clean help

docker:
	docker build -t $(IMAGE) docker/.

build:
	docker run --rm -v "$(PWD):/workspace" -w /workspace \
		$(USER_FLAG) $(IMAGE) make -C firmware all

clean:
	docker run --rm -v "$(PWD):/workspace" -w /workspace \
		$(USER_FLAG) $(IMAGE) make -C firmware clean

help:
	@echo "make docker  - build STM32 toolchain image"
	@echo "make build   - build CubeMX project in ./firmware"
	@echo "make clean   - clean firmware build"
	@echo "Wokwi UART monitor opens inside the VS Code simulator"
