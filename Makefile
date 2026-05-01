HELENGINE_CORE_CPP_ROOT ?=

BUILD_DIR := build
TARGET_VPK := $(BUILD_DIR)/helengine_psvita.vpk
CMAKE_ARGS :=

ifneq ($(strip $(HELENGINE_CORE_CPP_ROOT)),)
CMAKE_ARGS += -DHELENGINE_CORE_CPP_ROOT=$(HELENGINE_CORE_CPP_ROOT)
endif

.PHONY: all clean

all: $(TARGET_VPK)

$(BUILD_DIR)/CMakeCache.txt: CMakeLists.txt
	@mkdir -p $(BUILD_DIR)
	cd $(BUILD_DIR) && cmake $(CMAKE_ARGS) ..

$(TARGET_VPK): $(BUILD_DIR)/CMakeCache.txt
	$(MAKE) -C $(BUILD_DIR)

clean:
	@rm -rf $(BUILD_DIR)
