IMAGE ?= stm32-build
CAPTURE_MODE ?= 0
HOST_UID := $(shell id -u 2>/dev/null)
HOST_GID := $(shell id -g 2>/dev/null)
ifneq ($(HOST_UID),)
USER_FLAG := --user $(HOST_UID):$(HOST_GID)
endif

.PHONY: docker build clean test coverage check-architecture help

docker:
	docker build -t $(IMAGE) docker/.

build:
	docker run --rm -v "$(PWD):/workspace" -w /workspace \
		$(USER_FLAG) $(IMAGE) make -C firmware CAPTURE_MODE=$(CAPTURE_MODE) all

clean:
	docker run --rm -v "$(PWD):/workspace" -w /workspace \
		$(USER_FLAG) $(IMAGE) make -C firmware clean

test:
	docker run --rm -v "$(PWD):/workspace" -w /workspace \
		$(USER_FLAG) $(IMAGE) make -C tests test

coverage:
	docker run --rm -v "$(PWD):/workspace" -w /workspace \
		$(USER_FLAG) $(IMAGE) make -C tests coverage

check-architecture:
	docker run --rm -v "$(PWD):/workspace" -w /workspace \
		$(USER_FLAG) $(IMAGE) python3 scripts/check_architecture.py

help:
	@echo "make docker  - build STM32 toolchain image"
	@echo "make build   - build CubeMX project in ./firmware"
	@echo "make clean   - clean firmware build"
	@echo "make test    - run HAL-independent host tests"
	@echo "make coverage - generate host-test coverage report"
	@echo "make check-architecture - enforce layer dependency rules"
	@echo "Wokwi UART monitor opens inside the VS Code simulator"
