# Spring

> **This is the [eosrio](https://github.com/eosrio) community maintenance fork of [AntelopeIO/spring](https://github.com/AntelopeIO/spring), continuing the stable 1.2.x production line.**
>
> eosrio is a production Antelope/Vaulta node operator. This fork exists because upstream `AntelopeIO/spring` has not shipped a stable release since v1.2.2 (August 2025), while a body of production-correctness fixes (p2p/finality hardening, supply-chain and build work, test de-flaking) sits unreleased. We ship those fixes on the 1.2.x line — the version every production Antelope/Vaulta chain actually runs — as 1.2.3, 1.2.4, … with **zero compatibility breaks**: no consensus/protocol-rule change, no ABI/serialization/wire change, no protocol-feature activation, and no breaking change to the RPC/SHiP response shapes existing consumers depend on.
>
> The **v2.0 line is out of scope** (it was never put into production). All credit for the original Spring/Antelope implementation belongs to the upstream [AntelopeIO](https://github.com/AntelopeIO) and [Vaulta](https://github.com/VaultaFoundation) project. See [ROADMAP.md](./ROADMAP.md) for the complete fork strategy, branch topology, and compatibility guarantees.

1. [Branches](#branches)
2. [Supported Operating Systems](#supported-operating-systems)
3. [Binary Installation](#binary-installation)
4. [Build and Install from Source](#build-and-install-from-source)
5. [Bash Autocomplete](#bash-autocomplete)

Spring is a C++ implementation of the [Antelope](https://github.com/AntelopeIO) protocol with support for Savanna consensus. It contains blockchain node software and supporting tools for developers and node operators.

## Branches

In this fork, the `main` branch is the **1.2.x maintenance line** — the version every production Antelope/Vaulta chain runs, continued with zero compatibility breaks. (It replaces the upstream `main`, which tracked the out-of-scope 2.0 development line; that history is preserved on the `upstream-2.0-main` branch. `release/1.2` @ `v1.2.2` is the backport base.)

**For production, use a release tag — not the tip of `main`.** See the [release page](https://github.com/eosrio/spring/releases) for stable releases and pre-releases. `v1.2.2` is the last fully-stable release; the current development tip is `1.2.3-alpha1`, a testing pre-release.

See [ROADMAP.md](./ROADMAP.md) for the complete fork strategy, branch topology, and compatibility guarantees.

## Supported Operating Systems
We currently support the following operating systems.
- Ubuntu 20.04 Focal
- Ubuntu 22.04 Jammy
- Ubuntu 24.04 Noble
- Ubuntu 26.04

The pinned reproducible build (`.deb` package) is built once and installs and runs on all supported Ubuntu versions.

Other Unix derivatives such as macOS are tended to on a best-effort basis and may not be full featured. If you aren't using Ubuntu, please visit the "[Build Unsupported OS](./docs/00_install/01_build-from-source/00_build-unsupported-os.md)" page to explore your options.

Ubuntu derivatives such as Linux Mint generally work by following the instructions for their Ubuntu base (`cat /etc/upstream-release/lsb-release` to find it), but we make no guarantees.

## Binary Installation
This is the fastest way to get started. From the [latest release](https://github.com/eosrio/spring/releases/latest) page, download a binary for one of our [supported operating systems](#supported-operating-systems), or visit the [release tags](https://github.com/eosrio/spring/releases) page to download a binary for a specific version of Spring.

Once you have an `antelope-spring_*.deb` file downloaded for your version of Ubuntu, you can install it as follows:
```bash
sudo apt-get update
sudo apt-get install -y ~/Downloads/antelope-spring_*.deb
```
Adjust the path as needed; omit `sudo` inside a Docker container.

Finally, verify Spring was installed correctly:
```bash
nodeos --full-version
```
You should see a [semantic version](https://semver.org) string followed by a `git` commit hash with no errors. For example:
```
v1.2.3-alpha1-9026a03c09c9b4f93edca696b5eef259f0ab96b3
```

## Build and Install from Source
You can also build and install Spring from source.

### Prerequisites
You will need to build on a [supported operating system](#supported-operating-systems).

Requirements to build:
- C++20 compiler and standard library
- CMake 3.16+
- LLVM 7 - 11, or LLVM 14+ - for Linux only
  - LLVM 12 and 13 are not supported (transitional opaque-pointer era)
  - Ubuntu 20.04/22.04 use LLVM 11; Ubuntu 24.04/26.04 use LLVM 18 (system `llvm-18-dev`)
- libcurl 7.40.0+
- libzstd (`libzstd-dev`) - required for LLVM 14+ (including LLVM 18 on Ubuntu 24.04/26.04)
- git
- GMP
- Python 3
- python3-numpy
- zlib

Minimum compiler version by Ubuntu release:
- Ubuntu 20.04: GCC 10 (`g++-10`) for C++20 support (GCC 10.2+ is required)
- Ubuntu 22.04: GCC 11 (default)
- Ubuntu 24.04: GCC 13 (default)
- Ubuntu 26.04: GCC 15 (default)

### Step 1 - Clone
Clone the repository with submodules (HTTPS or SSH), then enter it:
```bash
git clone --recursive https://github.com/eosrio/spring.git
# or: git clone --recursive git@github.com:eosrio/spring.git
cd spring
```

### Step 2 - Checkout Release Tag or Branch
Choose which [release](https://github.com/eosrio/spring/releases) or [branch](#branches) you would like to build, then check it out. If you are not sure, use the [latest release](https://github.com/eosrio/spring/releases/latest). For example, if you want to build release 1.2.3 then you would check it out using its tag, `v1.2.3`. In the example below, replace `v0.0.0` with your selected release tag accordingly:
```bash
git fetch --all --tags
git checkout v0.0.0
```

Once you are on the branch or release tag you want to build, make sure everything is up-to-date:
```bash
git pull
git submodule update --init --recursive
```

### Step 3 - Build
Select build instructions below for a [pinned build](#pinned-reproducible-build) (preferred) or an [unpinned build](#unpinned-build).

> ℹ️ **Pinned vs. Unpinned Build** ℹ️  
We have two types of builds for Spring: "pinned" and "unpinned." A pinned build is a reproducible build with the build environment and dependency versions fixed by the development team. In contrast, unpinned builds use the dependency versions provided by the build platform. Unpinned builds tend to be quicker because the pinned build environment must be built from scratch. Pinned builds, in addition to being reproducible, ensure the compiler remains the same between builds of different Spring major versions. Spring requires the compiler version to remain the same, otherwise its state might need to be recovered from a portable snapshot or the chain needs to be replayed.

> ⚠️ **A Warning On Parallel Compilation Jobs (`-j` flag)** ⚠️  
When building C/C++ software, often the build is performed in parallel via a command such as `make -j "$(nproc)"` which uses all available CPU threads. However, be aware that some compilation units (`*.cpp` files) in Spring will consume nearly 4GB of memory. Failures due to memory exhaustion will typically, but not always, manifest as compiler crashes. Using all available CPU threads may also prevent you from doing other things on your computer during compilation. For these reasons, consider reducing this value.

> 🐋 **Docker and `sudo`** 🐋  
If you are in an Ubuntu docker container, omit `sudo` from all commands because you run as `root` by default. Most other docker containers also exclude `sudo`, especially Debian-family containers. If your shell prompt is a hash tag (`#`), omit `sudo`.

#### Pinned Reproducible Build
The pinned reproducible build requires Docker. Make sure you are in the root of the `spring` repo and then run
```bash
DOCKER_BUILDKIT=1 docker build -f tools/reproducible.Dockerfile -o . .
```
This command will take a substantial amount of time because a toolchain is built from scratch. Upon completion, the current directory will contain a built `.deb` and `.tar.zst` (you can change the `-o .` argument to place the output in a different directory). If needing to reduce the number of parallel jobs as warned above, run the command as,
```bash
DOCKER_BUILDKIT=1 docker build --build-arg SPRING_BUILD_JOBS=4 -f tools/reproducible.Dockerfile -o . .
```

#### Unpinned Build
These instructions are for this branch; other release branches may differ, so follow the directions in the branch or release you intend to build.

**Ubuntu 20.04 / 22.04** (these use the system LLVM 11):
```bash
sudo apt-get update
sudo apt-get install -y \
        build-essential \
        cmake \
        git \
        libcurl4-openssl-dev \
        libgmp-dev \
        llvm-11-dev \
        python3-numpy \
        file \
        zlib1g-dev
```
On Ubuntu 20.04, also install `gcc-10`, which has C++20 support:
```bash
sudo apt-get install -y g++-10
```

**Ubuntu 24.04 / 26.04** (these use the system LLVM 18, which additionally requires `libzstd-dev`):
```bash
sudo apt-get update
sudo apt-get install -y \
        build-essential \
        cmake \
        git \
        libcurl4-openssl-dev \
        libgmp-dev \
        llvm-18-dev \
        libzstd-dev \
        python3-numpy \
        file \
        zlib1g-dev
```

To build, make sure you are in the root of the `spring` repo, then run the following commands. Use the configure line that matches your Ubuntu version:
```bash
mkdir -p build
cd build

## on Ubuntu 20.04, specify the gcc-10 compiler and LLVM 11
cmake -DCMAKE_C_COMPILER=gcc-10 -DCMAKE_CXX_COMPILER=g++-10 -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/usr/lib/llvm-11 ..

## on Ubuntu 22.04, the default gcc (version 11) is fine; use LLVM 11
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/usr/lib/llvm-11 ..

## on Ubuntu 24.04, the default gcc (version 13) is fine; use LLVM 18
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/usr/lib/llvm-18 ..

## on Ubuntu 26.04, the default gcc (version 15) is fine; use LLVM 18.
## Ubuntu 26.04 ships CMake 4.x, which requires the policy-minimum flag below.
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/usr/lib/llvm-18 -DCMAKE_POLICY_VERSION_MINIMUM=3.5 ..

make -j "$(nproc)" package
```

Now you can optionally [test](#step-4---test) your build, or [install](#step-5---install) the `antelope-spring_*.deb` binary packages, which will be in the root of your build directory.

### Step 4 - Test
Spring supports the following test suites:

Test Suite | Test Type | [Test Size](https://testing.googleblog.com/2010/12/test-sizes.html) | Notes
---|:---:|:---:|---
[Parallelizable tests](#parallelizable-tests) | Unit tests | Small
[WASM spec tests](#wasm-spec-tests) | Unit tests | Small | Unit tests for our WASM runtime, each short but _very_ CPU-intensive
[Serial tests](#serial-tests) | Component/Integration | Medium
[Long-running tests](#long-running-tests) | Integration | Medium-to-Large | Tests which take an extraordinarily long amount of time to run

When building from source, we recommended running at least the [parallelizable tests](#parallelizable-tests).

#### Parallelizable Tests
This test suite consists of any test that does not require shared resources, such as file descriptors, specific folders, or ports, and can therefore be run concurrently in different threads without side effects (hence, easily parallelized). These are mostly unit tests and [small tests](https://testing.googleblog.com/2010/12/test-sizes.html) which complete in a short amount of time.

You can invoke them by running `ctest` from a terminal in your Spring build directory and specifying the following arguments:
```bash
ctest -j "$(nproc)" -LE _tests
```

#### WASM Spec Tests
The WASM spec tests verify that our WASM execution engine is compliant with the web assembly standard. These are very [small](https://testing.googleblog.com/2010/12/test-sizes.html), very fast unit tests. However, there are over a thousand of them so the suite can take a little time to run. These tests are extremely CPU-intensive.

You can invoke them by running `ctest` from a terminal in your Spring build directory and specifying the following arguments:
```bash
ctest -j "$(nproc)" -L wasm_spec_tests
```
We have observed severe performance issues when multiple virtual machines are running this test suite on the same physical host at the same time, for example in a CICD system. This can be resolved by disabling hyperthreading on the host.

#### Serial Tests
The serial test suite consists of [medium](https://testing.googleblog.com/2010/12/test-sizes.html) component or integration tests that use specific paths, ports, rely on process names, or similar, and cannot be run concurrently with other tests. Serial tests can be sensitive to other software running on the same host and they may `SIGKILL` other `nodeos` processes. These tests take a moderate amount of time to complete, but we recommend running them.

You can invoke them by running `ctest` from a terminal in your Spring build directory and specifying the following arguments:
```bash
ctest -L "nonparallelizable_tests"
```

#### Long-Running Tests
The long-running tests are [medium-to-large](https://testing.googleblog.com/2010/12/test-sizes.html) integration tests that rely on shared resources and take a very long time to run.

You can invoke them by running `ctest` from a terminal in your Spring build directory and specifying the following arguments:
```bash
ctest -L "long_running_tests"
```

### Step 5 - Install
Once you have [built](#step-3---build) Spring and [tested](#step-4---test) your build, you can install Spring on your system. Don't forget to omit `sudo` if you are running in a docker container.

We recommend installing the binary package you just built. Navigate to your Spring build directory in a terminal and run this command:
```bash
sudo apt-get update
sudo apt-get install -y ./antelope-spring_*.deb
```

It is also possible to install using `make` instead:
```bash
sudo make install
```

## Bash Autocomplete
`cleos` and `spring-util` offer a substantial amount of functionality. Consider using bash's autocompletion support which makes it easier to discover all their various options.

For our provided `.deb` packages simply install Ubuntu's `bash-completion` package: `apt-get install bash-completion` (you may need to log out/in after installing).

If building from source install the `build/programs/cleos/bash-completion/completions/cleos` and `build/programs/spring-util/bash-completion/completions/spring-util` files to your bash-completion directory. Refer to [bash-completion's documentation](https://github.com/scop/bash-completion#faq) on the possible install locations.