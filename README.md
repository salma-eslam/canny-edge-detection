# canny-edge-detection



# Canny Edge Detection with RISC-V RVV

This project implements a Canny-style edge detection pipeline in C++ with both scalar and RISC-V Vector Extension (RVV) versions. The project supports:

* Scalar host pipeline
* RVV pipeline using `riscv64-unknown-elf-g++`
* Separable Gaussian mixed pipeline
* Host-side GoogleTest tests
* QEMU-side assert/printf RVV tests
* Raw grayscale image input/output
* Benchmarking with 100+ iterations
* VLEN and optimization-level sweeps

---

## 1. Project Structure

```text
canny-edge-detection/
├── src/
│   ├── main.cpp
│   ├── main01.cpp
│   ├── main_separable.cpp
│   ├── gaussian.cpp
│   ├── gaussian_rvv.cpp
│   ├── gaussian_separable.cpp
│   ├── gaussian_separable_rvv.cpp
│   ├── sobel.cpp
│   ├── sobel_rvv.cpp
│   ├── gradient.cpp
│   ├── gradient_rvv.cpp
│   ├── direction.cpp
│   ├── image_io.cpp
│   ├── clock_shim.cpp
│   ├── syscalls.cpp
│   └── benchmark*.cpp
│
├── tests/
│   ├── test_direction.cpp
│   ├── test_gaussian.cpp
│   ├── test_gaussian_separable.cpp
│   ├── test_sobel_gradient.cpp
│   ├── test_gaussian_rvv.cpp
│   ├── test_gaussian_separable_rvv.cpp
│   ├── test_magnitude_rvv.cpp
│   └── sobel_validation_test.cpp
│
├── tools/
│   ├── convert_image.py
│   ├── raw_to_png.py
│   
│
├── input/
├── output/
├── Makefile
└── README.md
```

---

## 2. Required Tools

The project uses two compilers:

```text
HOST_CXX = g++
RV_CXX   = riscv64-unknown-elf-g++
```

The host compiler is used for scalar programs and GoogleTest tests.

The RISC-V compiler is used for RVV programs and QEMU-side tests.

---

## 3. Install Basic Dependencies

On Ubuntu / WSL:

```bash
sudo apt update
sudo apt install -y build-essential git cmake ninja-build python3 python3-pip qemu-user pkg-config
```

Install Python image dependencies:

```bash
pip3 install numpy pillow
```

---

## 4. Build the RISC-V GNU Toolchain

The project requires an RVV-capable RISC-V GNU toolchain built from source.

Clone the toolchain:

```bash
cd ~
git clone https://github.com/riscv-collab/riscv-gnu-toolchain.git
cd riscv-gnu-toolchain
```

Install toolchain build dependencies:

```bash
sudo apt install -y autoconf automake autotools-dev curl python3 libmpc-dev libmpfr-dev libgmp-dev gawk build-essential bison flex texinfo gperf libtool patchutils bc zlib1g-dev libexpat-dev ninja-build
```

Configure and build with RVV enabled:

```bash
./configure --prefix=$HOME/riscv --with-arch=rv64gcv --with-abi=lp64d
make -j$(nproc)
```

Add the toolchain to your PATH:

```bash
echo 'export PATH=$HOME/riscv/bin:$PATH' >> ~/.bashrc
source ~/.bashrc
```

Check that the compiler exists:

```bash
which riscv64-unknown-elf-g++
riscv64-unknown-elf-g++ --version
```

Expected compiler name:

```text
riscv64-unknown-elf-g++
```

---

## 5. Set Up GoogleTest Locally

GoogleTest is used only for host-side tests. It is compiled with native `g++`, not the RISC-V compiler.

Clone and build GoogleTest:

```bash
mkdir -p ~/tools
cd ~/tools
git clone https://github.com/google/googletest.git
cd googletest
cmake -S . -B build -DCMAKE_INSTALL_PREFIX=$HOME/tools/gtest -DBUILD_GMOCK=OFF
cmake --build build -j$(nproc)
cmake --install build
```

Check that it installed correctly:

```bash
ls ~/tools/gtest/include/gtest/gtest.h
find ~/tools/gtest -name "libgtest*.a"
```

Expected output includes:

```text
/home/<user>/tools/gtest/include/gtest/gtest.h
/home/<user>/tools/gtest/lib/libgtest.a
/home/<user>/tools/gtest/lib/libgtest_main.a
```

---

## 6. Clone the Project

```bash
cd ~
git clone <https://github.com/salma-eslam/canny-edge-detection>
cd canny-edge-detection
```

---

## 7. Raw Image Format

The project uses raw grayscale images.

A raw image is exactly:

```text
width × height bytes
```

Each byte is one pixel:

```text
0   = black
255 = white
```

There is no header and no compression.

For a 512×512 image, the raw file size must be:

```text
512 × 512 = 262144 bytes
```

Check raw file size:

```bash
stat -c%s input/conan_512x512.raw
```

Expected:

```text
262144
```

---

## 8. Convert an Image to Raw

Place the source image in the `input/` folder.

Supported source image extensions:

```text
.jpeg
.jpg
.png
.jfif
```

Example:

```text
input/conan.jpeg
```

Convert it to raw:

```bash
make raw IMAGE=conan
```

This creates:

```text
input/conan_512x512.raw
```

For another image:

```bash
make raw IMAGE=f1
make raw IMAGE=ferrari
make raw IMAGE=night
```

If an old-style raw file already exists, such as:

```text
input/conan_512.raw
```

the Makefile can still use it.

---

## 9. Official Assignment Commands

The Makefile includes the required official commands:

```bash
make test
make canny_rv
make run
make clean
```

Meaning:

```text
make test     = compile and run host GoogleTest suite
make canny_rv = cross-compile official RISC-V RVV pipeline
make run      = run official RISC-V RVV pipeline on QEMU
make clean    = remove build files
```

Run the official RVV pipeline:

```bash
make clean
make canny_rv
make run IMAGE=conan OPT=-O2 VLEN=512
```

---

## 10. Main Pipelines

There are three main pipeline targets.

### 10.1 Scalar Host Pipeline

Compile only:

```bash
make main OPT=-O2
```

Run on a real image:

```bash
make run-main IMAGE=conan OPT=-O2
```

This runs natively on the host using `g++`.

---

### 10.2 RVV Pipeline

Compile only:

```bash
make main01 OPT=-O2
```

Run on QEMU:

```bash
make run-main01 IMAGE=conan OPT=-O2 VLEN=512
```

This uses:

```text
riscv64-unknown-elf-g++
qemu-riscv64
```

The RVV pipeline is:

```text
Gaussian RVV
→ Sobel RVV
→ Magnitude L1 RVV
→ Magnitude L2 scalar
→ Direction scalar
```

---

### 10.3 Separable Mixed Pipeline

Compile only:

```bash
make main_separable OPT=-O2
```

Run on QEMU:

```bash
make run-main_separable IMAGE=conan OPT=-O2 VLEN=512
```

This uses separable Gaussian filtering and RVV stages.

---

### 10.4 Run All Three Pipelines

```bash
make run-all IMAGE=conan OPT=-O2 VLEN=512
```

This runs:

```text
scalar host pipeline
RVV pipeline
separable mixed pipeline
```

---

## 11. Output Files

After running a pipeline, output raw files are written to `output/`.

Example output files:

```text
output/conan_512x512_blur.raw
output/conan_512x512_magnitude_l1.raw
output/conan_512x512_magnitude_l2.raw
output/conan_512x512_direction.raw
output/conan_512x512_edges.raw
```

The final edge output is:

```text
output/conan_512x512_edges.raw
```

---

## 12. Convert Raw Outputs to PNG

Convert output raw files to PNG:

```bash
make png IMAGE=conan
```

Open the output folder in Windows:

```bash
explorer.exe output
```

---

## 13. Copy PNG Results to Windows Downloads

```bash
make copy-results IMAGE=conan
```

If the default Downloads path does not work, pass the Windows path manually:

```bash
make copy-results IMAGE=conan WIN_DOWNLOADS=/mnt/c/Users/<WindowsUser>/Downloads
```

---

## 14. Threshold Final Edge Image

The edge magnitude output can look gray. To make a clean black/white image, use threshold 50.

Run:

```bash
python3 tools/threshold_raw.py output/conan_512x512_edges.raw output/conan_512x512_edges_thr50.raw output/conan_512x512_edges_thr50.png 512 512
```

This means:

```text
pixel >= 50  → 255 white
pixel < 50   → 0 black
```

Open results:

```bash
explorer.exe output
```

---

## 15. Host Tests

Host tests use GoogleTest and are compiled with normal `g++`.

Run all host tests:

```bash
make test
```

Individual host tests:

```bash
make test_direction
make test_gaussian
make test_gaussian_separable
make test_sobel_gradient
```

These test scalar/host functionality.

---

## 16. RVV / QEMU Tests

QEMU-side RVV tests do not use GoogleTest. They use assert/printf-style tests and return codes.

Run all RVV tests:

```bash
make rvv-tests VLEN=512
```

Individual RVV tests:

```bash
make test_gaussian_rvv VLEN=512
make test_gaussian_separable_rvv VLEN=512
make test_magnitude_rvv VLEN=512
make test_sobel_rvv VLEN=512
```

---

## 17. Benchmarks

Benchmarks are used for timing and comparison.

### 17.1 Scalar Benchmark

```bash
make run-benchmark IMAGE=conan OPT=-O2
```

### 17.2 Main Scenario Benchmark

This is the most important benchmark for comparing scalar/RVV scenarios under the same environment.

```bash
make run-benchmark_scenarios_1 IMAGE=conan OPT=-O2 VLEN=512
```

### 17.3 Best Case Benchmark

```bash
make run-benchmark_best_case IMAGE=conan OPT=-O2 VLEN=512
```

Note: `benchmark_best_case` is hybrid/RVV because it links RVV code even though the filename does not include `rvv`.

### 17.4 Mixed Benchmark

```bash
make run-benchmark_mixed IMAGE=conan OPT=-O2 VLEN=512
```

### 17.5 Separable Scalar Benchmark

```bash
make run-benchmark_separable OPT=-O2
```

### 17.6 Gaussian RVV Benchmark

```bash
make run-benchmark_gaussian_rvv OPT=-O2 VLEN=512
```

### 17.7 Magnitude RVV Benchmark

```bash
make run-benchmark_magnitude_rvv OPT=-O2 VLEN=512
```

### 17.8 Separable RVV Benchmark

```bash
make run-benchmark_separable_rvv OPT=-O2 VLEN=512
```

---

## 18. VLEN Sweep

To compare different vector lengths:

```bash
make sweep-vlen IMAGE=conan OPT=-O2
```

This runs:

```text
VLEN=128
VLEN=256
VLEN=512
```

---

## 19. Optimization Sweep

To compare optimization levels:

```bash
make sweep-opt IMAGE=conan VLEN=512
```

This runs:

```text
-O0
-O2
-O3
-Ofast
```

---

## 20. Optional Linux-Target RISC-V Run

The official project toolchain is:

```text
riscv64-unknown-elf-g++
```

However, for timing sanity checks, the Makefile also includes optional Linux-target RISC-V commands if the system has an RVV-capable Linux compiler.

Run optional Linux RVV pipeline:

```bash
make run-main01-linux IMAGE=conan OPT=-O2 VLEN=512
```

Run optional Linux benchmark:

```bash
make run-benchmark_scenarios_1-linux IMAGE=conan OPT=-O2 VLEN=512
```

This uses:

```text
riscv64-linux-gnu-g++
qemu-riscv64 -L /usr/riscv64-linux-gnu
```

Important: this is optional and only works if `riscv64-linux-gnu-g++` supports RVV intrinsics on the machine.

---

## 21. How to Interpret Timing

Do not directly compare:

```text
host scalar main time
vs
QEMU RVV main01 time
```

This is not fair because:

```text
main runs natively on the laptop/WSL
main01 runs through QEMU emulation
```

For fair comparison, compare scalar and RVV kernels inside the same benchmark environment.

Use:

```bash
make run-benchmark_scenarios_1 IMAGE=conan OPT=-O2 VLEN=512
```

The benchmark runs kernels multiple times, usually 100+ iterations, and reports average time.

---

## 22. Why QEMU Timing Is Limited

QEMU is not cycle-accurate. It does not simulate a real RISC-V vector microarchitecture.

Therefore:

```text
absolute timing numbers are not real hardware performance
relative comparisons are more useful

---

## 23. What to Say If RVV Is Slower

If RVV appears slower, this does not necessarily mean RVV would be slower on real hardware.

```text
The RVV result is slower under QEMU because QEMU emulates vector instructions in software and does not model real hardware speedups. Therefore, absolute timing is not representative of a real RISC-V vector processor. We use QEMU timing only for relative comparison under the same environment, and we focus on correctness, instruction-level RVV implementation, and trends across optimization levels and VLEN values.
```

---

## 24. Full Demo Checklist

Before presenting, run:

```bash
make clean
make test
make rvv-tests VLEN=512
make raw IMAGE=conan
make run-main IMAGE=conan OPT=-O2
make run-main01 IMAGE=conan OPT=-O2 VLEN=512
make run-main_separable IMAGE=conan OPT=-O2 VLEN=512
make png IMAGE=conan
```

Open outputs:

```bash
explorer.exe output
```

Run main benchmark:

```bash
make run-benchmark_scenarios_1 IMAGE=conan OPT=-O2 VLEN=512
```

Run sweeps:

```bash
make sweep-vlen IMAGE=conan OPT=-O2
make sweep-opt IMAGE=conan VLEN=512
```

---

## 25. Troubleshooting

### Problem: `gtest/gtest.h: No such file or directory`

GoogleTest is not found.

Check:

```bash
ls ~/tools/gtest/include/gtest/gtest.h
```

If missing, rebuild GoogleTest.

---

### Problem: `cannot find -lgtest`

GoogleTest library path is not found.

Check:

```bash
find ~/tools/gtest -name "libgtest*.a"
```

Expected:

```text
~/tools/gtest/lib/libgtest.a
~/tools/gtest/lib/libgtest_main.a
```

---

### Problem: `Cannot open file: input/conan_512x512.raw`

The raw file does not exist or the name is wrong.

Run:

```bash
ls input
make raw IMAGE=conan

Or copy old-style raw file:
```bash
cp input/conan_512.raw input/conan_512x512.raw

---

### Problem: RVV binary runs but timing is huge

This is expected under QEMU and `unknown-elf` timing shim. Use benchmark averages and explain QEMU limitations.


## 26. Most Important Commands
make clean
make test
make rvv-tests VLEN=512
make raw IMAGE=conan
make run IMAGE=conan OPT=-O2 VLEN=512
make run-main IMAGE=conan OPT=-O2
make run-main01 IMAGE=conan OPT=-O2 VLEN=512
make run-main_separable IMAGE=conan OPT=-O2 VLEN=512
make run-benchmark_scenarios_1 IMAGE=conan OPT=-O2 VLEN=512
make sweep-vlen IMAGE=conan OPT=-O2
make sweep-opt IMAGE=conan VLEN=512
make png IMAGE=conan
```
