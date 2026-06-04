# syntax=docker/dockerfile:1
FROM ubuntu:24.04
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

# #578: EOS VM OC is now built against Ubuntu 24.04's system LLVM 18 (llvm-18-dev).
# (Before #578, 24.04 had no llvm-11 package so OC had to be disabled here.)
# libzstd-dev satisfies LLVM 18's zstd::libzstd_shared imported target.
# ENABLE_OC defaults ON; we only point find_package(LLVM) at llvm-18 here.
ENV SPRING_PLATFORM_HAS_EXTRAS_CMAKE=1
COPY <<-EOF /extras.cmake
set(LLVM_DIR "/usr/lib/llvm-18/lib/cmake/llvm" CACHE STRING "")
EOF
