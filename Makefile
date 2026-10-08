HELENGINE_CORE_CPP_ROOT ?=
HELENGINE_PSVITA_GAME_TITLE ?=

# Keep the CMake build tree in the project/profile native cache. The outer
# builder stages VPK and runtime content into a fresh invocation directory.
NATIVE_OBJECT_CACHE_ROOT ?= build
BUILD_DIR ?= $(NATIVE_OBJECT_CACHE_ROOT)
PACKAGE_DIR ?= $(BUILD_DIR)
SOURCE_DIR ?= .
TARGET_VPK := $(PACKAGE_DIR)/helengine_psvita.vpk
CMAKE_ARGS :=

ifneq ($(strip $(HELENGINE_CORE_CPP_ROOT)),)
CMAKE_ARGS += -DHELENGINE_CORE_CPP_ROOT=$(HELENGINE_CORE_CPP_ROOT)
endif

ifneq ($(strip $(HELENGINE_PSVITA_GAME_TITLE)),)
CMAKE_ARGS += "-DHELENGINE_PSVITA_GAME_TITLE=$(HELENGINE_PSVITA_GAME_TITLE)"
endif

.PHONY: all clean test-native FORCE

all: $(BUILD_DIR)/CMakeCache.txt
	@mkdir -p $(PACKAGE_DIR)
	@rm -f $(BUILD_DIR)/helengine_psvita.vpk
	$(MAKE) -C $(BUILD_DIR)
ifneq ($(abspath $(PACKAGE_DIR)),$(abspath $(BUILD_DIR)))
	@cp $(BUILD_DIR)/helengine_psvita.vpk $(TARGET_VPK)
endif

test-native:
	arm-vita-eabi-g++ -std=gnu++20 -Wall -Wextra -Werror -Isrc -fsyntax-only builder.tests/native/PsVitaGxmMemoryBlockSizeTests.cpp

$(BUILD_DIR)/CMakeCache.txt: CMakeLists.txt FORCE
	@mkdir -p $(BUILD_DIR)
	cd $(BUILD_DIR) && cmake $(CMAKE_ARGS) $(abspath $(SOURCE_DIR))

clean:
	@rm -rf $(BUILD_DIR)
	@rm -f $(TARGET_VPK)
