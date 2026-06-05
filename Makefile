HOST_CXX := g++
RV_CXX   := riscv64-unknown-elf-g++
HOST_CXXFLAGS := -std=c++17 -Wall -Wextra -O2
RV_CXXFLAGS   := -std=c++17 -Wall -Wextra -O2

SRC_DIR       := src
TEST_DIR      := tests
HOST_BUILD    := build/host
RV_BUILD      := build/riscv

SRCS := $(wildcard $(SRC_DIR)/*.cpp)
APP_SRCS := $(filter-out $(SRC_DIR)/main.cpp,$(SRCS))
TEST_SRCS := $(wildcard $(TEST_DIR)/*.cpp)

TEST_BIN := $(HOST_BUILD)/test_runner
RV_BIN   := $(RV_BUILD)/canny_rv

GTEST_LIBS := -lgtest -lgtest_main -pthread

.PHONY: all
all: test canny_rv

.PHONY: test
test: $(TEST_BIN)
	./$(TEST_BIN)

$(TEST_BIN): $(APP_SRCS) $(TEST_SRCS)
	@mkdir -p $(HOST_BUILD)
	$(HOST_CXX) $(HOST_CXXFLAGS) $^ -o $@ $(GTEST_LIBS)

.PHONY: canny_rv
canny_rv: $(RV_BIN)

$(RV_BIN): $(SRCS)
	@mkdir -p $(RV_BUILD)
	$(RV_CXX) $(RV_CXXFLAGS) $^ -o $@

.PHONY: run
run: $(RV_BIN)
	qemu-riscv64 ./$(RV_BIN)

.PHONY: clean
clean:
	rm -rf build
