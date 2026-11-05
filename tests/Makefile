CC ?= gcc
BUILD_DIR := build
COMMON_FLAGS := -std=c11 -Wall -Wextra -Werror -pedantic
INCLUDES := -I../firmware/Devices -I../firmware/Platform -I../firmware/App

ifeq ($(COVERAGE),1)
COMMON_FLAGS += --coverage -O0
endif

TESTS := \
$(BUILD_DIR)/test_bmp180_compensation \
$(BUILD_DIR)/test_bmp180_errors \
$(BUILD_DIR)/test_app

.PHONY: test coverage clean

test: $(TESTS)
	@for test_binary in $(TESTS); do ./$$test_binary; done

$(BUILD_DIR)/test_bmp180_compensation: test_bmp180_compensation.c ../firmware/Devices/bmp180.c | $(BUILD_DIR)
	$(CC) $(COMMON_FLAGS) $(INCLUDES) $^ -o $@

$(BUILD_DIR)/test_bmp180_errors: test_bmp180_errors.c ../firmware/Devices/bmp180.c | $(BUILD_DIR)
	$(CC) $(COMMON_FLAGS) $(INCLUDES) $^ -o $@

$(BUILD_DIR)/test_app: test_app.c ../firmware/App/app.c ../firmware/Devices/bmp180.c | $(BUILD_DIR)
	$(CC) $(COMMON_FLAGS) $(INCLUDES) $^ -o $@

coverage: clean
	$(MAKE) COVERAGE=1 test
	lcov --capture --directory $(BUILD_DIR) --output-file $(BUILD_DIR)/coverage.info
	lcov --remove $(BUILD_DIR)/coverage.info '*/tests/*' --output-file $(BUILD_DIR)/coverage.info
	genhtml $(BUILD_DIR)/coverage.info --output-directory $(BUILD_DIR)/coverage-html

$(BUILD_DIR):
	mkdir -p $@

clean:
	rm -rf $(BUILD_DIR)
