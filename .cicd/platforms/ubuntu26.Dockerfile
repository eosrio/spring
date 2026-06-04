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
                       ninja-build          \
                       python3-numpy        \
                       file                 \
                       zlib1g-dev           \
                       zstd

# Ubuntu 26.04 packages only llvm-17..22, but Spring's EOS VM OC needs LLVM 7-11
# (ORCv1, removed in LLVM 12), so OC is disabled here until ORCv1->ORCv2 (#578).
# Also: 26.04 ships cmake 4.2, which removed compatibility with the pre-3.5
# cmake_minimum_required used by the vendored boost submodule, so we pin the
# policy minimum. A full-OC binary that RUNS on 26.04 comes from the pinned build.
ENV SPRING_PLATFORM_HAS_EXTRAS_CMAKE=1
COPY <<-EOF /extras.cmake
set(ENABLE_OC OFF CACHE BOOL "" FORCE)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "" FORCE)
EOF
