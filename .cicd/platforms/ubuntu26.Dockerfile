# syntax=docker/dockerfile:1
FROM ubuntu:26.04
ENV TZ="America/New_York"
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get upgrade -y && \
    apt-get install -y build-essential      \
                       cmake                \
                       git                  \
                       jq                   \
                       libcurl4-openssl-dev \
                       libgmp-dev           \
                       libzstd-dev          \
                       llvm-18-dev          \
                       ninja-build          \
                       python3-numpy        \
                       file                 \
                       zlib1g-dev           \
                       zstd

# #578: EOS VM OC is now built against Ubuntu 26.04's system LLVM 18 (llvm-18-dev).
# libzstd-dev satisfies LLVM 18's zstd::libzstd_shared imported target.
# 26.04 ships cmake 4.2, which dropped compat with the vendored boost's pre-3.5
# cmake_minimum_required, so we pin the policy minimum. ENABLE_OC defaults ON.
ENV SPRING_PLATFORM_HAS_EXTRAS_CMAKE=1
COPY <<-EOF /extras.cmake
set(LLVM_DIR "/usr/lib/llvm-18/lib/cmake/llvm" CACHE STRING "")
set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "" FORCE)
EOF
