HELENGINE_CORE_CPP_ROOT ?=
HELENGINE_PSVITA_GAME_TITLE ?=

BUILD_DIR := build
TARGET_VPK := $(BUILD_DIR)/helengine_psvita.vpk
CMAKE_ARGS :=

ifneq ($(strip $(HELENGINE_CORE_CPP_ROOT)),)
CMAKE_ARGS += -DHELENGINE_CORE_CPP_ROOT=$(HELENGINE_CORE_CPP_ROOT)
endif

ifneq ($(strip $(HELENGINE_PSVITA_GAME_TITLE)),)
CMAKE_ARGS += "-DHELENGINE_PSVITA_GAME_TITLE=$(HELENGINE_PSVITA_GAME_TITLE)"
endif

.PHONY: all clean test-native

all: $(TARGET_VPK)

test-native:
	arm-vita-eabi-g++ -std=gnu++20 -Wall -Wextra -Werror -Isrc -fsyntax-only builder.tests/native/PsVitaGxmMemoryBlockSizeTests.cpp

$(BUILD_DIR)/CMakeCache.txt: CMakeLists.txt
	@mkdir -p $(BUILD_DIR)
	cd $(BUILD_DIR) && cmake $(CMAKE_ARGS) ..

$(TARGET_VPK): $(BUILD_DIR)/CMakeCache.txt
	$(MAKE) -C $(BUILD_DIR)

clean:
	@rm -rf $(BUILD_DIR)
