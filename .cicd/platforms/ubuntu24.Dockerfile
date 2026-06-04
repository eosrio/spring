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
                       ninja-build          \
                       python3-numpy        \
                       file                 \
                       zlib1g-dev           \
                       zstd

# Ubuntu 24.04 packages only llvm-14..20, but Spring's EOS VM OC needs LLVM 7-11
# (ORCv1, removed in LLVM 12), so OC is disabled here (eos-vm + eos-vm-jit only)
# until ORCv1->ORCv2 modernization (#578) lands. A full-OC binary that RUNS on
# 24.04 is available via the pinned reproducible build (glibc forward-compat).
ENV SPRING_PLATFORM_HAS_EXTRAS_CMAKE=1
COPY <<-EOF /extras.cmake
set(ENABLE_OC OFF CACHE BOOL "" FORCE)
EOF
