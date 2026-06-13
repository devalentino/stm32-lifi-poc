CLANG_FORMAT ?= clang-format
CLANG_FORMAT_MAJOR := 22
CMAKE ?= cmake
CTEST ?= ctest

BUILD_PRESET ?= Debug
TEST_BUILD_DIR ?= build-test

FORMAT_DIRS := Sources Tests
FORMAT_EXTENSIONS := -name '*.c' -o -name '*.h'
FORMAT_EXCLUDES := \
	! -path 'Sources/stm32f4xx_hal_conf.h' \
	! -path 'Sources/system_stm32f4xx.c' \
	! -path 'Sources/main.c' \
	! -path 'Sources/syscalls.c' \
	! -path 'Sources/sysmem.c' \
	! -path 'Tests/fake/stm32f4xx_hal.h'
FORMAT_FILES := $(shell find $(FORMAT_DIRS) -type f \( $(FORMAT_EXTENSIONS) \) $(FORMAT_EXCLUDES) | sort)

.PHONY: help check-format-tool format format-check build test ci clean-test

help:
	@echo "Available targets:"
	@echo "  make format        Format project C sources and headers"
	@echo "  make format-check  Validate formatting for CI"
	@echo "  make build         Configure and build firmware preset ($(BUILD_PRESET))"
	@echo "  make test          Configure, build, and run host-side tests"
	@echo "  make ci            Run formatting, firmware build, and host tests"

check-format-tool:
	@version="$$($(CLANG_FORMAT) --version)"; \
	echo "Using $$version"; \
	echo "$$version" | grep -Eq 'version $(CLANG_FORMAT_MAJOR)\.' || \
		( echo "Expected clang-format $(CLANG_FORMAT_MAJOR).x"; exit 1 )

format: check-format-tool
	$(CLANG_FORMAT) -i $(FORMAT_FILES)

format-check: check-format-tool
	$(CLANG_FORMAT) --dry-run --Werror $(FORMAT_FILES)

build:
	$(CMAKE) --preset $(BUILD_PRESET)
	$(CMAKE) --build --preset $(BUILD_PRESET)

test:
	$(CMAKE) -S . -B $(TEST_BUILD_DIR) -G Ninja -DLIFI_BUILD_TESTS=ON
	$(CMAKE) --build $(TEST_BUILD_DIR)
	$(CTEST) --test-dir $(TEST_BUILD_DIR) --output-on-failure

ci: format-check build test

clean-test:
	$(CMAKE) -E rm -rf $(TEST_BUILD_DIR)
