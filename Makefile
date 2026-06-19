# =============================================================================
# Canny Edge Detection — Makefile
# =============================================================================
#
# How to explain this to teammates:
#
# This Makefile is a shortcut system for compiling and running the project.
# Instead of typing long compiler commands every time, we define named
# targets.
#
#   make main           - compiles the scalar pipeline (main.cpp)
#   make main01          - compiles the RVV pipeline (main01.cpp)
#   make main_separable  - compiles the separable Gaussian mixed pipeline
#
# The compiler is selected by the target:
# =============================================================================
# Canny Edge Detection - Dual-target Makefile
# =============================================================================
# Official RVV compiler required by the project: riscv64-unknown-elf-g++
# Host/scalar compiler: g++
#
# Main official commands:
#   make test                         # host GoogleTest suite
#   make canny_rv                     # build official RVV pipeline
#   make run IMAGE=conan VLEN=512     # run official RVV pipeline on QEMU
#   make clean                        # remove generated build files
#
# Useful extra commands:
#   make run-main IMAGE=conan
#   make run-main01 IMAGE=conan OPT=-O3 VLEN=512
#   make run-main_separable IMAGE=conan OPT=-O2 VLEN=512
#   make run-benchmark_scenarios_1 IMAGE=conan OPT=-O2 VLEN=512
#   make sweep-vlen IMAGE=conan
#   make sweep-opt IMAGE=conan
#   make png IMAGE=conan
# =============================================================================

SHELL := /bin/bash
.DEFAULT_GOAL := help

# -----------------------------------------------------------------------------
# Directories
# -----------------------------------------------------------------------------
SRC_DIR    := src
TEST_DIR   := tests
TOOLS_DIR  := tools
INPUT_DIR  := input
OUTPUT_DIR := output
BUILD_DIR  := build

HOST_OBJ_DIR     := $(BUILD_DIR)/host/obj
HOST_BIN_DIR     := $(BUILD_DIR)/host/bin
RV_OBJ_DIR       := $(BUILD_DIR)/rv/obj
RV_BIN_DIR       := $(BUILD_DIR)/rv/bin
RV_LINUX_OBJ_DIR := $(BUILD_DIR)/rv_linux/obj
RV_LINUX_BIN_DIR := $(BUILD_DIR)/rv_linux/bin

# -----------------------------------------------------------------------------
# Compilers
# -----------------------------------------------------------------------------
HOST_CXX     := g++
RV_CXX       ?= riscv64-unknown-elf-g++
RV_LINUX_CXX ?= riscv64-linux-gnu-g++

# -----------------------------------------------------------------------------
# Options
# -----------------------------------------------------------------------------
OPT    ?= -O2
VLEN   ?= 512
IMAGE  ?= conan
WIDTH  ?= 512
HEIGHT ?= 512
RAW    ?=

CXXSTD     := -std=c++17
WARN_FLAGS := -Wall -Wextra

HOST_CXXFLAGS     := $(CXXSTD) $(WARN_FLAGS) $(OPT) -I$(SRC_DIR)
RV_CXXFLAGS       := $(CXXSTD) $(WARN_FLAGS) $(OPT) -I$(SRC_DIR) -march=rv64gcv -mabi=lp64d -static
RV_LINUX_CXXFLAGS := $(CXXSTD) $(WARN_FLAGS) $(OPT) -I$(SRC_DIR) -march=rv64gcv -mabi=lp64d -static

QEMU             := qemu-riscv64
QEMU_FLAGS       := -cpu rv64,v=true,vlen=$(VLEN)
QEMU_LINUX_FLAGS := -cpu rv64,v=true,vlen=$(VLEN) -L /usr/riscv64-linux-gnu

# -----------------------------------------------------------------------------
# GoogleTest local installation
# -----------------------------------------------------------------------------
# GoogleTest is used ONLY for host-side tests.
# It is compiled with native g++, not with the RISC-V compiler.
#
# Installed locally at:
#   ~/tools/gtest
#
# Required files:
#   ~/tools/gtest/include/gtest/gtest.h
#   ~/tools/gtest/lib/libgtest.a
#   ~/tools/gtest/lib/libgtest_main.a

GTEST_PREFIX ?= $(HOME)/tools/gtest
GTEST_CFLAGS := -I$(GTEST_PREFIX)/include
GTEST_CORE_LIBS := -L$(GTEST_PREFIX)/lib -lgtest -pthread
GTEST_MAIN_LIBS := -L$(GTEST_PREFIX)/lib -lgtest -lgtest_main -pthread

# -----------------------------------------------------------------------------
# Image input handling
# -----------------------------------------------------------------------------
# IMAGE=conan can use any of these if present:
#   input/conan_512x512.raw
#   input/conan_512.raw
#   input/conan.raw
# If none exists, Makefile tries to convert:
#   input/conan.jpeg / .jpg / .png / .jfif -> input/conan_512x512.raw
# You may also bypass IMAGE detection by using RAW=input/some_file.raw

RAW_CANONICAL := $(INPUT_DIR)/$(IMAGE)_$(WIDTH)x$(HEIGHT).raw
RAW_LEGACY    := $(INPUT_DIR)/$(IMAGE)_$(WIDTH).raw
RAW_DIRECT    := $(INPUT_DIR)/$(IMAGE).raw

ifeq ($(strip $(RAW)),)
  RAW_EXISTING := $(firstword $(wildcard $(RAW_CANONICAL)) $(wildcard $(RAW_LEGACY)) $(wildcard $(RAW_DIRECT)))
  RAW_INPUT    := $(if $(RAW_EXISTING),$(RAW_EXISTING),$(RAW_CANONICAL))
else
  RAW_INPUT    := $(RAW)
endif

SRC_PHOTO := $(firstword \
  $(wildcard $(INPUT_DIR)/$(IMAGE).jpeg) \
  $(wildcard $(INPUT_DIR)/$(IMAGE).jpg) \
  $(wildcard $(INPUT_DIR)/$(IMAGE).png) \
  $(wildcard $(INPUT_DIR)/$(IMAGE).jfif))

OUT_PREFIX  := $(OUTPUT_DIR)/$(IMAGE)_$(WIDTH)x$(HEIGHT)
OUT_SUFFIXES := blur magnitude_l1 magnitude_l2 direction edges
WIN_DOWNLOADS ?= /mnt/c/Users/User/Downloads

# -----------------------------------------------------------------------------
# Source classification
# -----------------------------------------------------------------------------
# Portable sources can be compiled by host or RISC-V compilers.
LIB_SRCS_PORTABLE := \
  image_io.cpp \
  gaussian.cpp \
  gaussian_separable.cpp \
  sobel.cpp \
  gradient.cpp \
  direction.cpp

# RVV sources include intrinsics and must be compiled by the RISC-V compiler.
LIB_SRCS_RVV_IMPL := \
  gaussian_rvv.cpp \
  gaussian_separable_rvv.cpp \
  sobel_rvv.cpp \
  gradient_rvv.cpp

# Runtime shims are only for official unknown-elf builds.
LIB_SRCS_RV_RUNTIME := \
  clock_shim.cpp \
  syscalls.cpp

LIB_OBJS_HOST := $(patsubst %.cpp,$(HOST_OBJ_DIR)/%.o,$(LIB_SRCS_PORTABLE))
LIB_OBJS_RV   := $(patsubst %.cpp,$(RV_OBJ_DIR)/%.o,$(LIB_SRCS_PORTABLE) $(LIB_SRCS_RVV_IMPL) $(LIB_SRCS_RV_RUNTIME))
LIB_OBJS_RV_LINUX := $(patsubst %.cpp,$(RV_LINUX_OBJ_DIR)/%.o,$(LIB_SRCS_PORTABLE) $(LIB_SRCS_RVV_IMPL))

# -----------------------------------------------------------------------------
# Build object rules
# -----------------------------------------------------------------------------
$(HOST_OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(HOST_OBJ_DIR)
	$(HOST_CXX) $(HOST_CXXFLAGS) -c $< -o $@

$(HOST_OBJ_DIR)/%.o: $(TEST_DIR)/%.cpp
	@mkdir -p $(HOST_OBJ_DIR)
	$(HOST_CXX) $(HOST_CXXFLAGS) $(GTEST_CFLAGS) -c $< -o $@

$(RV_OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(RV_OBJ_DIR)
	@which $(RV_CXX) >/dev/null 2>&1 || (echo "ERROR: $(RV_CXX) not found on PATH."; exit 1)
	$(RV_CXX) $(RV_CXXFLAGS) -c $< -o $@

$(RV_OBJ_DIR)/%.o: $(TEST_DIR)/%.cpp
	@mkdir -p $(RV_OBJ_DIR)
	@which $(RV_CXX) >/dev/null 2>&1 || (echo "ERROR: $(RV_CXX) not found on PATH."; exit 1)
	$(RV_CXX) $(RV_CXXFLAGS) -c $< -o $@

$(RV_LINUX_OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(RV_LINUX_OBJ_DIR)
	@which $(RV_LINUX_CXX) >/dev/null 2>&1 || (echo "ERROR: $(RV_LINUX_CXX) not found on PATH."; exit 1)
	$(RV_LINUX_CXX) $(RV_LINUX_CXXFLAGS) -c $< -o $@

# -----------------------------------------------------------------------------
# Image conversion rule
# -----------------------------------------------------------------------------
$(RAW_CANONICAL):
	@test -n "$(IMAGE)" || (echo "ERROR: set IMAGE=conan or RAW=input/file.raw"; exit 1)
	@test -n "$(SRC_PHOTO)" || (echo "ERROR: no raw file found and no $(INPUT_DIR)/$(IMAGE).{jpeg,jpg,png,jfif} found"; exit 1)
	@mkdir -p $(INPUT_DIR)
	python3 $(TOOLS_DIR)/convert_image.py $(SRC_PHOTO) $@ $(WIDTH) $(HEIGHT)

.PHONY: raw
raw: $(RAW_INPUT)
	@echo "Using raw input: $(RAW_INPUT)"

# =============================================================================
# Official assignment targets
# =============================================================================
.PHONY: all test canny_rv run clean
all: test canny_rv

canny_rv: main01

run: run-main01

clean:
	rm -rf $(BUILD_DIR)

# =============================================================================
# Main pipelines
# =============================================================================
MAIN_BIN := $(HOST_BIN_DIR)/main
MAIN01_BIN := $(RV_BIN_DIR)/main01
MAIN_SEP_BIN := $(RV_BIN_DIR)/main_separable
MAIN01_LINUX_BIN := $(RV_LINUX_BIN_DIR)/main01_linux

.PHONY: main main01 main_separable main01_linux
main: $(MAIN_BIN)
main01: $(MAIN01_BIN)
main_separable: $(MAIN_SEP_BIN)
main01_linux: $(MAIN01_LINUX_BIN)

$(MAIN_BIN): $(LIB_OBJS_HOST) $(HOST_OBJ_DIR)/main.o
	@mkdir -p $(HOST_BIN_DIR)
	$(HOST_CXX) $(HOST_CXXFLAGS) $^ -o $@

$(MAIN01_BIN): $(LIB_OBJS_RV) $(RV_OBJ_DIR)/main01.o
	@mkdir -p $(RV_BIN_DIR)
	$(RV_CXX) $(RV_CXXFLAGS) $^ -o $@

$(MAIN_SEP_BIN): $(LIB_OBJS_RV) $(RV_OBJ_DIR)/main_separable.o
	@mkdir -p $(RV_BIN_DIR)
	$(RV_CXX) $(RV_CXXFLAGS) $^ -o $@

$(MAIN01_LINUX_BIN): $(LIB_OBJS_RV_LINUX) $(RV_LINUX_OBJ_DIR)/main01.o
	@mkdir -p $(RV_LINUX_BIN_DIR)
	$(RV_LINUX_CXX) $(RV_LINUX_CXXFLAGS) $^ -o $@

.PHONY: run-main run-main01 run-main_separable run-main01-linux run-all
run-main: main $(RAW_INPUT)
	@test -f "$(RAW_INPUT)" || (echo "ERROR: raw input not found: $(RAW_INPUT)"; exit 1)
	@mkdir -p $(OUTPUT_DIR)
	$(MAIN_BIN) $(RAW_INPUT) $(WIDTH) $(HEIGHT) $(OUT_PREFIX)

run-main01: main01 $(RAW_INPUT)
	@test -f "$(RAW_INPUT)" || (echo "ERROR: raw input not found: $(RAW_INPUT)"; exit 1)
	@mkdir -p $(OUTPUT_DIR)
	$(QEMU) $(QEMU_FLAGS) $(MAIN01_BIN) $(RAW_INPUT) $(WIDTH) $(HEIGHT) $(OUT_PREFIX)

run-main_separable: main_separable $(RAW_INPUT)
	@test -f "$(RAW_INPUT)" || (echo "ERROR: raw input not found: $(RAW_INPUT)"; exit 1)
	@mkdir -p $(OUTPUT_DIR)
	$(QEMU) $(QEMU_FLAGS) $(MAIN_SEP_BIN) $(RAW_INPUT) $(WIDTH) $(HEIGHT) $(OUT_PREFIX)

# Optional Linux-target RISC-V run. This is not the official toolchain target.
run-main01-linux: main01_linux $(RAW_INPUT)
	@test -f "$(RAW_INPUT)" || (echo "ERROR: raw input not found: $(RAW_INPUT)"; exit 1)
	@mkdir -p $(OUTPUT_DIR)
	$(QEMU) $(QEMU_LINUX_FLAGS) $(MAIN01_LINUX_BIN) $(RAW_INPUT) $(WIDTH) $(HEIGHT) $(OUT_PREFIX)_linux

run-all: run-main run-main01 run-main_separable

# =============================================================================
# Benchmarks
# =============================================================================
BENCHMARK_BIN := $(HOST_BIN_DIR)/benchmark
BENCH_SEP_BIN := $(HOST_BIN_DIR)/benchmark_separable
BENCH_BEST_BIN := $(RV_BIN_DIR)/benchmark_best_case
BENCH_GAUSS_RVV_BIN := $(RV_BIN_DIR)/benchmark_gaussian_rvv
BENCH_MAG_RVV_BIN := $(RV_BIN_DIR)/benchmark_magnitude_rvv
BENCH_MIXED_BIN := $(RV_BIN_DIR)/benchmark_mixed
BENCH_SCENARIOS_BIN := $(RV_BIN_DIR)/benchmark_scenarios_1
BENCH_SEP_RVV_BIN := $(RV_BIN_DIR)/benchmark_separable_rvv
BENCH_SCENARIOS_LINUX_BIN := $(RV_LINUX_BIN_DIR)/benchmark_scenarios_1_linux

.PHONY: benchmark benchmark_separable benchmark_best_case benchmark_gaussian_rvv benchmark_magnitude_rvv benchmark_mixed benchmark_scenarios_1 benchmark_separable_rvv benchmark_scenarios_1_linux
benchmark: $(BENCHMARK_BIN)
benchmark_separable: $(BENCH_SEP_BIN)
benchmark_best_case: $(BENCH_BEST_BIN)
benchmark_gaussian_rvv: $(BENCH_GAUSS_RVV_BIN)
benchmark_magnitude_rvv: $(BENCH_MAG_RVV_BIN)
benchmark_mixed: $(BENCH_MIXED_BIN)
benchmark_scenarios_1: $(BENCH_SCENARIOS_BIN)
benchmark_separable_rvv: $(BENCH_SEP_RVV_BIN)
benchmark_scenarios_1_linux: $(BENCH_SCENARIOS_LINUX_BIN)

$(BENCHMARK_BIN): $(LIB_OBJS_HOST) $(HOST_OBJ_DIR)/benchmark.o
	@mkdir -p $(HOST_BIN_DIR)
	$(HOST_CXX) $(HOST_CXXFLAGS) $^ -o $@

$(BENCH_SEP_BIN): $(HOST_OBJ_DIR)/gaussian.o $(HOST_OBJ_DIR)/gaussian_separable.o $(HOST_OBJ_DIR)/benchmark_separable.o
	@mkdir -p $(HOST_BIN_DIR)
	$(HOST_CXX) $(HOST_CXXFLAGS) $^ -o $@

$(BENCH_BEST_BIN): $(LIB_OBJS_RV) $(RV_OBJ_DIR)/benchmark_best_case.o
	@mkdir -p $(RV_BIN_DIR)
	$(RV_CXX) $(RV_CXXFLAGS) $^ -o $@

$(BENCH_GAUSS_RVV_BIN): $(LIB_OBJS_RV) $(RV_OBJ_DIR)/benchmark_gaussian_rvv.o
	@mkdir -p $(RV_BIN_DIR)
	$(RV_CXX) $(RV_CXXFLAGS) $^ -o $@

$(BENCH_MAG_RVV_BIN): $(LIB_OBJS_RV) $(RV_OBJ_DIR)/benchmark_magnitude_rvv.o
	@mkdir -p $(RV_BIN_DIR)
	$(RV_CXX) $(RV_CXXFLAGS) $^ -o $@

$(BENCH_MIXED_BIN): $(LIB_OBJS_RV) $(RV_OBJ_DIR)/benchmark_mixed.o
	@mkdir -p $(RV_BIN_DIR)
	$(RV_CXX) $(RV_CXXFLAGS) $^ -o $@

$(BENCH_SCENARIOS_BIN): $(LIB_OBJS_RV) $(RV_OBJ_DIR)/benchmark_scenarios_1.o
	@mkdir -p $(RV_BIN_DIR)
	$(RV_CXX) $(RV_CXXFLAGS) $^ -o $@

$(BENCH_SEP_RVV_BIN): $(LIB_OBJS_RV) $(RV_OBJ_DIR)/benchmark_separable_rvv.o
	@mkdir -p $(RV_BIN_DIR)
	$(RV_CXX) $(RV_CXXFLAGS) $^ -o $@

$(BENCH_SCENARIOS_LINUX_BIN): $(LIB_OBJS_RV_LINUX) $(RV_LINUX_OBJ_DIR)/benchmark_scenarios_1.o
	@mkdir -p $(RV_LINUX_BIN_DIR)
	$(RV_LINUX_CXX) $(RV_LINUX_CXXFLAGS) $^ -o $@

.PHONY: run-benchmark run-benchmark_separable run-benchmark_best_case run-benchmark_gaussian_rvv run-benchmark_magnitude_rvv run-benchmark_mixed run-benchmark_scenarios_1 run-benchmark_separable_rvv run-benchmark_scenarios_1-linux
run-benchmark: benchmark $(RAW_INPUT)
	@test -f "$(RAW_INPUT)" || (echo "ERROR: raw input not found: $(RAW_INPUT)"; exit 1)
	$(BENCHMARK_BIN) $(RAW_INPUT) $(WIDTH) $(HEIGHT)

run-benchmark_separable: benchmark_separable
	$(BENCH_SEP_BIN)

run-benchmark_best_case: benchmark_best_case $(RAW_INPUT)
	@test -f "$(RAW_INPUT)" || (echo "ERROR: raw input not found: $(RAW_INPUT)"; exit 1)
	$(QEMU) $(QEMU_FLAGS) $(BENCH_BEST_BIN) $(RAW_INPUT) $(WIDTH) $(HEIGHT)

run-benchmark_gaussian_rvv: benchmark_gaussian_rvv
	$(QEMU) $(QEMU_FLAGS) $(BENCH_GAUSS_RVV_BIN)

run-benchmark_magnitude_rvv: benchmark_magnitude_rvv
	$(QEMU) $(QEMU_FLAGS) $(BENCH_MAG_RVV_BIN)

run-benchmark_mixed: benchmark_mixed $(RAW_INPUT)
	@test -f "$(RAW_INPUT)" || (echo "ERROR: raw input not found: $(RAW_INPUT)"; exit 1)
	$(QEMU) $(QEMU_FLAGS) $(BENCH_MIXED_BIN) $(RAW_INPUT) $(WIDTH) $(HEIGHT)

run-benchmark_scenarios_1: benchmark_scenarios_1 $(RAW_INPUT)
	@test -f "$(RAW_INPUT)" || (echo "ERROR: raw input not found: $(RAW_INPUT)"; exit 1)
	$(QEMU) $(QEMU_FLAGS) $(BENCH_SCENARIOS_BIN) $(RAW_INPUT) $(WIDTH) $(HEIGHT)

run-benchmark_separable_rvv: benchmark_separable_rvv
	$(QEMU) $(QEMU_FLAGS) $(BENCH_SEP_RVV_BIN)

run-benchmark_scenarios_1-linux: benchmark_scenarios_1_linux $(RAW_INPUT)
	@test -f "$(RAW_INPUT)" || (echo "ERROR: raw input not found: $(RAW_INPUT)"; exit 1)
	$(QEMU) $(QEMU_LINUX_FLAGS) $(BENCH_SCENARIOS_LINUX_BIN) $(RAW_INPUT) $(WIDTH) $(HEIGHT)

# =============================================================================
# Tests
# =============================================================================
HOST_TEST_DIRECTION_BIN := $(HOST_BIN_DIR)/test_direction
HOST_TEST_GAUSSIAN_BIN := $(HOST_BIN_DIR)/test_gaussian
HOST_TEST_GAUSSIAN_SEP_BIN := $(HOST_BIN_DIR)/test_gaussian_separable
HOST_TEST_SOBEL_GRAD_BIN := $(HOST_BIN_DIR)/test_sobel_gradient

RV_TEST_GAUSSIAN_BIN := $(RV_BIN_DIR)/test_gaussian_rvv
RV_TEST_GAUSSIAN_SEP_BIN := $(RV_BIN_DIR)/test_gaussian_separable_rvv
RV_TEST_MAG_BIN := $(RV_BIN_DIR)/test_magnitude_rvv
RV_TEST_SOBEL_BIN := $(RV_BIN_DIR)/test_sobel_rvv

.PHONY: test_direction test_gaussian test_gaussian_separable test_sobel_gradient
test_direction: $(HOST_TEST_DIRECTION_BIN)
	$(HOST_TEST_DIRECTION_BIN)

test_gaussian: $(HOST_TEST_GAUSSIAN_BIN)
	$(HOST_TEST_GAUSSIAN_BIN)

test_gaussian_separable: $(HOST_TEST_GAUSSIAN_SEP_BIN)
	$(HOST_TEST_GAUSSIAN_SEP_BIN)

test_sobel_gradient: $(HOST_TEST_SOBEL_GRAD_BIN)
	$(HOST_TEST_SOBEL_GRAD_BIN)

$(HOST_TEST_DIRECTION_BIN): $(HOST_OBJ_DIR)/test_direction.o $(HOST_OBJ_DIR)/direction.o
	@mkdir -p $(HOST_BIN_DIR)
	$(HOST_CXX) $(HOST_CXXFLAGS) $^ -o $@ $(GTEST_MAIN_LIBS)

$(HOST_TEST_GAUSSIAN_BIN): $(HOST_OBJ_DIR)/test_gaussian.o $(HOST_OBJ_DIR)/gaussian.o $(HOST_OBJ_DIR)/image_io.o
	@mkdir -p $(HOST_BIN_DIR)
	$(HOST_CXX) $(HOST_CXXFLAGS) $^ -o $@ $(GTEST_CORE_LIBS)

$(HOST_TEST_GAUSSIAN_SEP_BIN): $(HOST_OBJ_DIR)/test_gaussian_separable.o $(HOST_OBJ_DIR)/gaussian_separable.o $(HOST_OBJ_DIR)/image_io.o
	@mkdir -p $(HOST_BIN_DIR)
	$(HOST_CXX) $(HOST_CXXFLAGS) $^ -o $@ $(GTEST_CORE_LIBS)

$(HOST_TEST_SOBEL_GRAD_BIN): $(HOST_OBJ_DIR)/test_sobel_gradient.o $(HOST_OBJ_DIR)/sobel.o $(HOST_OBJ_DIR)/gradient.o
	@mkdir -p $(HOST_BIN_DIR)
	$(HOST_CXX) $(HOST_CXXFLAGS) $^ -o $@ $(GTEST_CORE_LIBS)

# Official make test: native host GoogleTest only.
test: test_direction test_gaussian test_gaussian_separable test_sobel_gradient
	@echo "Host GoogleTest suite completed."

.PHONY: test_gaussian_rvv test_gaussian_separable_rvv test_magnitude_rvv test_sobel_rvv rvv-tests
test_gaussian_rvv: $(RV_TEST_GAUSSIAN_BIN)
	$(QEMU) $(QEMU_FLAGS) $(RV_TEST_GAUSSIAN_BIN)

test_gaussian_separable_rvv: $(RV_TEST_GAUSSIAN_SEP_BIN)
	$(QEMU) $(QEMU_FLAGS) $(RV_TEST_GAUSSIAN_SEP_BIN)

test_magnitude_rvv: $(RV_TEST_MAG_BIN)
	$(QEMU) $(QEMU_FLAGS) $(RV_TEST_MAG_BIN)

test_sobel_rvv: $(RV_TEST_SOBEL_BIN)
	$(QEMU) $(QEMU_FLAGS) $(RV_TEST_SOBEL_BIN)

rvv-tests: test_gaussian_rvv test_gaussian_separable_rvv test_magnitude_rvv test_sobel_rvv
	@echo "RVV tests completed."

$(RV_TEST_GAUSSIAN_BIN): $(LIB_OBJS_RV) $(RV_OBJ_DIR)/test_gaussian_rvv.o
	@mkdir -p $(RV_BIN_DIR)
	$(RV_CXX) $(RV_CXXFLAGS) $^ -o $@

$(RV_TEST_GAUSSIAN_SEP_BIN): $(LIB_OBJS_RV) $(RV_OBJ_DIR)/test_gaussian_separable_rvv.o
	@mkdir -p $(RV_BIN_DIR)
	$(RV_CXX) $(RV_CXXFLAGS) $^ -o $@

$(RV_TEST_MAG_BIN): $(LIB_OBJS_RV) $(RV_OBJ_DIR)/test_magnitude_rvv.o
	@mkdir -p $(RV_BIN_DIR)
	$(RV_CXX) $(RV_CXXFLAGS) $^ -o $@

$(RV_TEST_SOBEL_BIN): $(LIB_OBJS_RV) $(RV_OBJ_DIR)/sobel_validation_test.o
	@mkdir -p $(RV_BIN_DIR)
	$(RV_CXX) $(RV_CXXFLAGS) $^ -o $@

# =============================================================================
# Sweeps
# =============================================================================
.PHONY: sweep-vlen sweep-opt
sweep-vlen: benchmark_scenarios_1 $(RAW_INPUT)
	@test -f "$(RAW_INPUT)" || (echo "ERROR: raw input not found: $(RAW_INPUT)"; exit 1)
	@for v in 128 256 512; do \
	  echo ""; \
	  echo "=== VLEN=$$v ==="; \
	  $(QEMU) -cpu rv64,v=true,vlen=$$v $(BENCH_SCENARIOS_BIN) $(RAW_INPUT) $(WIDTH) $(HEIGHT); \
	done

sweep-opt:
	@test -f "$(RAW_INPUT)" || (echo "ERROR: raw input not found: $(RAW_INPUT)"; exit 1)
	@for o in -O0 -O2 -O3 -Ofast; do \
	  echo ""; \
	  echo "=== OPT=$$o ==="; \
	  $(MAKE) --no-print-directory clean; \
	  $(MAKE) --no-print-directory run-benchmark_scenarios_1 IMAGE=$(IMAGE) RAW=$(RAW_INPUT) WIDTH=$(WIDTH) HEIGHT=$(HEIGHT) OPT=$$o VLEN=$(VLEN); \
	done

# =============================================================================
# Output helpers
# =============================================================================
.PHONY: png copy-results clean-output
png:
	@test -n "$(IMAGE)" || (echo "ERROR: set IMAGE=conan"; exit 1)
	@for s in $(OUT_SUFFIXES); do \
	  raw="$(OUT_PREFIX)_$$s.raw"; \
	  png="$(OUT_PREFIX)_$$s.png"; \
	  if [ -f "$$raw" ]; then \
	    python3 $(TOOLS_DIR)/raw_to_png.py "$$raw" "$$png" $(WIDTH) $(HEIGHT); \
	  else \
	    echo "skip (not found): $$raw"; \
	  fi; \
	done

copy-results: png
	@test -d "$(WIN_DOWNLOADS)" || (echo "ERROR: $(WIN_DOWNLOADS) not found/accessible"; exit 1)
	@if ls $(OUT_PREFIX)_*.png >/dev/null 2>&1; then \
	  cp $(OUT_PREFIX)_*.png $(WIN_DOWNLOADS)/; \
	  echo "Copied PNGs to $(WIN_DOWNLOADS)"; \
	else \
	  echo "ERROR: no PNG files found for $(OUT_PREFIX)"; \
	  exit 1; \
	fi

clean-output:
	rm -rf $(OUTPUT_DIR)

# =============================================================================
# Help
# =============================================================================
.PHONY: help
help:
	@echo "Official assignment commands:"
	@echo "  make test                                      - host GoogleTest suite"
	@echo "  make canny_rv                                  - build official RVV pipeline"
	@echo "  make run IMAGE=conan OPT=-O2 VLEN=512          - run official RVV pipeline"
	@echo "  make clean                                     - remove build files"
	@echo ""
	@echo "Main pipelines:"
	@echo "  make run-main IMAGE=conan OPT=-O2              - scalar host pipeline"
	@echo "  make run-main01 IMAGE=conan OPT=-O2 VLEN=512   - official RVV pipeline"
	@echo "  make run-main_separable IMAGE=conan VLEN=512   - separable mixed RVV pipeline"
	@echo "  make run-all IMAGE=conan VLEN=512              - run all three"
	@echo ""
	@echo "Benchmarks:"
	@echo "  make run-benchmark IMAGE=conan                 - scalar benchmark"
	@echo "  make run-benchmark_scenarios_1 IMAGE=conan     - main scalar/RVV comparison"
	@echo "  make run-benchmark_gaussian_rvv VLEN=512       - synthetic Gaussian RVV benchmark"
	@echo "  make run-benchmark_magnitude_rvv VLEN=512      - synthetic magnitude RVV benchmark"
	@echo "  make run-benchmark_separable_rvv VLEN=512      - synthetic separable RVV benchmark"
	@echo ""
	@echo "Sweeps:"
	@echo "  make sweep-vlen IMAGE=conan OPT=-O2"
	@echo "  make sweep-opt IMAGE=conan VLEN=512"
	@echo ""
	@echo "Output tools:"
	@echo "  make png IMAGE=conan"
	@echo "  make copy-results IMAGE=conan"
	@echo ""
	@echo "Optional Linux-target RISC-V run, only if riscv64-linux-gnu-g++ supports RVV:"
	@echo "  make run-main01-linux IMAGE=conan OPT=-O2 VLEN=512"

