# syntax=docker/dockerfile:1
# debian:buster on Aug 20 2025 (tag from Jun 12, 2024)
FROM debian@sha256:58ce6f1271ae1c8a2006ff7d3e54e9874d839f573d8009c20154ad0f2fb0a225 AS builder

# If enabling the snapshot repo below, this ought to be after the base image time from above.
# date -u -d @1752331000 = Sat Jul 12 02:36:40 PM UTC 2025
ENV SOURCE_DATE_EPOCH=1752331000

# When the package repo is signed, a message in the payload indicates the time when the repo becomes stale. This protection
#  nominally exists to ensure older versions of the package repo which may contain defective packages aren't served in the far
#  future. But in our case, we want this pinned package repo at any future date. So [check-valid-until=no] to disable this check.
RUN DATETIMESTR=$(date -d @${SOURCE_DATE_EPOCH} +%Y%m%dT%H%M%SZ) && cat <<EOF > /etc/apt/sources.list
deb [check-valid-until=no] pinned://snapshot.debian.org/archive/debian/${DATETIMESTR}/ buster main
deb [check-valid-until=no] pinned://snapshot.debian.org/archive/debian/${DATETIMESTR}/ buster-updates main
deb [check-valid-until=no] pinned://snapshot.debian.org/archive/debian-security/${DATETIMESTR}/ buster/updates main
EOF

# Install the 'pinned' apt method, and define required hashes for the InRelease files of the package repos
COPY tools/pinned.pl /usr/lib/apt/methods/pinned
RUN cat <<EOF > /etc/apt/apt.conf.d/99pinned.conf
Acquire::pinned::InReleaseHashes {
  "buster"           "d2126c57347cfe5ca81d912ddfecc02e9a741c6e100d8d8295b735f979bc1a9d";
  "buster-updates"   "2efadfba571a0c888a8e0175c6f782f7a0afe18dee0e3fbcf1939931639749b8";
  "updates"          "5a9bda70b67ba71088bc7576dd6ee078f75428ea6291ca7b22ac7d79a9ec73e8";
};
EOF

RUN apt-get update && apt-get -y upgrade && DEBIAN_FRONTEND=noninteractive apt-get -y install build-essential \
                                                                                              file \
                                                                                              git \
                                                                                              libcurl4-openssl-dev \
                                                                                              libgmp-dev \
                                                                                              ninja-build \
                                                                                              python3 \
                                                                                              zlib1g-dev \
                                                                                              zstd \
                                                                                              ;

ARG _SPRING_CLANG_VERSION=18.1.8
# #578: OC now builds against modern LLVM. The OC LLVM ("pinllvm", built further down) is pinned to
# the SAME release as the toolchain clang (18.1.8) so a single source tarball serves both builds.
#
# Why two LLVM builds — the toolchain's own LLVM CANNOT be reused for OC (verified empirically by
# pointing OC's find_package(LLVM) at the toolchain LLVM and linking nodeos; it failed):
#   1. libc++ ABI: the toolchain LLVM is compiled with Debian's system GCC + libstdc++ because it is
#      built BEFORE CMAKE_TOOLCHAIN_FILE (below) switches everything to the pinned clang + libc++.
#      Spring/OC link libc++ exclusively, so the toolchain LLVM's libstdc++-flavored static libs do
#      not link into nodeos (undefined std::_Rb_tree_* / std::__throw_system_error). pinllvm is built
#      WITH the pinned clang + libc++ (-stdlib=libc++), which is exactly why OC can link it.
#   2. RTTI: the toolchain LLVM is RTTI-off (LLVM's default — note no -DLLVM_ENABLE_RTTI below in the
#      toolchain build); OC links LLVM into the consensus library and needs RTTI-on (pinllvm sets it).
# Truly eliminating the 2nd compile would require rebuilding the SHARED toolchain LLVM as libc++ +
# RTTI-on, which re-baselines the released reproducible-build hash and hits a libc++ bootstrap
# circularity — high risk for a stability-first fork, so we keep pinllvm. (PIC is NOT a factor: OC
# sets its blob relocation model programmatically in LLVMJIT.cpp, so -DLLVM_ENABLE_PIC=Off on pinllvm
# is only a host-library link detail and does not affect the bytes OC emits.)
ARG _SPRING_LLVM_VERSION=18.1.8
ARG _SPRING_CMAKE_VERSION=3.27.6

# One LLVM source tarball serves BOTH the toolchain clang and the pinllvm/OC build: since #578 the
# two are the same release (18.1.8). The pinllvm RUN below asserts the versions match before reusing
# this download — to diverge them, restore a second tarball+sig pair here for ${_SPRING_LLVM_VERSION}.
ADD https://github.com/llvm/llvm-project/releases/download/llvmorg-${_SPRING_CLANG_VERSION}/llvm-project-${_SPRING_CLANG_VERSION}.src.tar.xz     \
    https://github.com/llvm/llvm-project/releases/download/llvmorg-${_SPRING_CLANG_VERSION}/llvm-project-${_SPRING_CLANG_VERSION}.src.tar.xz.sig \
    https://github.com/Kitware/CMake/releases/download/v${_SPRING_CMAKE_VERSION}/cmake-${_SPRING_CMAKE_VERSION}.tar.gz                           \
    https://github.com/Kitware/CMake/releases/download/v${_SPRING_CMAKE_VERSION}/cmake-${_SPRING_CMAKE_VERSION}-SHA-256.txt                      \
    https://github.com/Kitware/CMake/releases/download/v${_SPRING_CMAKE_VERSION}/cmake-${_SPRING_CMAKE_VERSION}-SHA-256.txt.asc                  \
    /

# CBA23971357C2E6590D9EFD3EC8FEF3A7BFB4EDA - Brad King <brad.king@kitware.com> (cmake)
# 474E22316ABF4785A88C6E8EA2C794A986419D8A - Tom Stellard <tstellar@redhat.com> (llvm)
# D574BD5D1D0E98895E3BF90044F2485E45D59042 - Tobias Hieta <tobias@hieta.se> (llvm)

RUN gpg --keyserver hkps://keyserver.ubuntu.com --recv-keys CBA23971357C2E6590D9EFD3EC8FEF3A7BFB4EDA \
                                                            474E22316ABF4785A88C6E8EA2C794A986419D8A \
                                                            D574BD5D1D0E98895E3BF90044F2485E45D59042

RUN ls *.sig *.asc | xargs -n 1 gpg --verify && \
    sha256sum -c --ignore-missing cmake-*-SHA-256.txt

RUN tar xf cmake-*.tar.gz && \
    cd cmake*[0-9] && \
    echo 'set(CMAKE_USE_OPENSSL OFF CACHE BOOL "" FORCE)' > spring-init.cmake && \
    ./bootstrap --parallel=$(nproc) --init=spring-init.cmake --generator=Ninja && \
    ninja install && \
    rm -rf cmake*

RUN tar xf llvm-project-${_SPRING_CLANG_VERSION}.src.tar.xz && \
    cmake -S llvm-project-${_SPRING_CLANG_VERSION}.src/llvm -B build-toolchain -GNinja -DLLVM_INCLUDE_DOCS=Off -DLLVM_TARGETS_TO_BUILD=host -DCMAKE_BUILD_TYPE=Release \
                                                                                     -DCMAKE_INSTALL_PREFIX=/pinnedtoolchain \
                                                                                     -DCOMPILER_RT_BUILD_SANITIZERS=Off \
                                                                                     -DLIBCXX_HARDENING_MODE=fast \
                                                                                     -DLLVM_ENABLE_PROJECTS='lld;clang;clang-tools-extra' \
                                                                                     -DLLVM_ENABLE_RUNTIMES='compiler-rt;libc;libcxx;libcxxabi;libunwind' && \
    cmake --build build-toolchain -t install && \
    rm -rf build*

COPY <<-"EOF" /pinnedtoolchain/pinnedtoolchain.cmake
   set(CMAKE_C_COMPILER ${CMAKE_CURRENT_LIST_DIR}/bin/clang)
   set(CMAKE_CXX_COMPILER ${CMAKE_CURRENT_LIST_DIR}/bin/clang++)

   set(CMAKE_CXX_STANDARD_INCLUDE_DIRECTORIES ${CMAKE_CURRENT_LIST_DIR}/include/c++/v1 ${CMAKE_CURRENT_LIST_DIR}/include/x86_64-unknown-linux-gnu/c++/v1 /usr/local/include /usr/include)

   set(CMAKE_C_FLAGS_INIT "-D_FORTIFY_SOURCE=2 -fstack-protector-strong -fpie -pthread")
   set(CMAKE_CXX_FLAGS_INIT "-nostdinc++ -D_FORTIFY_SOURCE=2 -fstack-protector-strong -fpie -pthread")

   set(CMAKE_EXE_LINKER_FLAGS_INIT "-stdlib=libc++ -nostdlib++ -pie -pthread -Wl,-z,relro,-z,now")
   set(CMAKE_SHARED_LINKER_FLAGS_INIT "-stdlib=libc++ -nostdlib++")
   set(CMAKE_MODULE_LINKER_FLAGS_INIT "-stdlib=libc++ -nostdlib++")

   set(CMAKE_CXX_STANDARD_LIBRARIES "${CMAKE_CURRENT_LIST_DIR}/lib/x86_64-unknown-linux-gnu/libc++.a ${CMAKE_CURRENT_LIST_DIR}/lib/x86_64-unknown-linux-gnu/libc++abi.a")

   set(CMAKE_SYSTEM_PREFIX_PATH "${CMAKE_CURRENT_LIST_DIR}/pinllvm")
EOF
ENV CMAKE_TOOLCHAIN_FILE=/pinnedtoolchain/pinnedtoolchain.cmake

# pinllvm reuses the single LLVM tarball downloaded above, so its version must equal the toolchain's.
RUN test "${_SPRING_CLANG_VERSION}" = "${_SPRING_LLVM_VERSION}" || { echo "ERROR: _SPRING_LLVM_VERSION (${_SPRING_LLVM_VERSION}) != _SPRING_CLANG_VERSION (${_SPRING_CLANG_VERSION}); restore the second LLVM tarball+sig in the ADD above to build OC against a different LLVM than the toolchain." >&2; exit 1; } && \
    tar xf llvm-project-${_SPRING_LLVM_VERSION}.src.tar.xz && \
    cmake -S llvm-project-${_SPRING_LLVM_VERSION}.src/llvm -B build-pinllvm -GNinja -DCMAKE_BUILD_TYPE=Release -DLLVM_TARGETS_TO_BUILD=host -DLLVM_BUILD_TOOLS=Off \
                                                                                  -DLLVM_ENABLE_RTTI=On -DLLVM_ENABLE_TERMINFO=Off -DLLVM_ENABLE_PIC=Off -DLLVM_ENABLE_ZSTD=Off \
                                                                                  -DCMAKE_INSTALL_PREFIX=/pinnedtoolchain/pinllvm && \
    cmake --build build-pinllvm -t install && \
    rm -rf build* llvm*

FROM builder AS build

ARG SPRING_BUILD_JOBS
ARG SPRING_CONSENSUS_PROFILE=vanilla

# Yuck: This places the source at the same location as spring's CI (build.yaml, build_base.yaml). Unfortunately this location only matches
#       when build.yaml etc are being run from a repository named spring.
COPY / /__w/spring/spring
RUN cmake -S /__w/spring/spring -B build -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=Release -GNinja \
          -DSPRING_CONSENSUS_PROFILE="${SPRING_CONSENSUS_PROFILE}" && \
    cmake --build build -t package -- ${SPRING_BUILD_JOBS:+-j$SPRING_BUILD_JOBS} && \
    /__w/spring/spring/tools/tweak-deb.sh build/antelope-spring*_*.deb

FROM scratch AS exporter
COPY --from=build /build/*.deb /build/*.tar.* /
