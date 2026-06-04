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
# NOTE: llvm-11-dev is intentionally omitted — Ubuntu 24.04 packages only
# llvm-14..20, and Spring's EOS VM OC needs LLVM 7-11 (ORCv1, removed in 12).
# Native builds on this platform must therefore configure with -DENABLE_OC=OFF
# (eos-vm + eos-vm-jit runtimes; no OC tier-up) until ORCv1->ORCv2 (#578) lands.
# A full-OC binary that RUNS on 24.04 is available via the pinned reproducible build.
