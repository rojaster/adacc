# This file is part of SymCC.
#
# SymCC is free software: you can redistribute it and/or modify it under the
# terms of the GNU General Public License as published by the Free Software
# Foundation, either version 3 of the License, or (at your option) any later
# version.
#
# SymCC is distributed in the hope that it will be useful, but WITHOUT ANY
# WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
# A PARTICULAR PURPOSE. See the GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along with
# SymCC. If not, see <https://www.gnu.org/licenses/>.

#
# The build stage
#
FROM ubuntu:22.04 AS builder

# Install dependencies
RUN apt-get update \
    && DEBIAN_FRONTEND=noninteractive apt-get install -y \
    clang-12 \
    cmake \
    g++ \
    git \
    libz3-dev \
    curl \
    llvm-12-dev \
    llvm-12-tools \
    ninja-build \
    python3 \
    python3-pip \
    zlib1g-dev \
    && rm -rf /var/lib/apt/lists/* \
    && pip3 install lit \
    && ln -s /usr/bin/llvm-config-12 /usr/bin/llvm-config \
    && ln -s /usr/bin/clang-12 /usr/bin/clang \
    && ln -s /usr/bin/clang++-12 /usr/bin/clang++ \
    && ln -s /usr/bin/FileCheck-12 /usr/bin/FileCheck

# Install Rust from rustup
RUN curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y

# Build AFL.
RUN git clone https://github.com/AFLplusplus/AFLplusplus.git -b stable afl \
    && cd afl \
    && make

# Download the LLVM sources already so that we don't need to get them again when
# SymCC changes
RUN git clone -b llvmorg-12.0.1 --depth 1 https://github.com/llvm/llvm-project.git /llvm_source

# Build a version of SymCC with the simple backend to compile libc++
COPY . /adacc_source

WORKDIR /adacc_build_simple
RUN cmake -G Ninja \
        -DQSYM_BACKEND=OFF \
        -DCMAKE_BUILD_TYPE=RelWithDebInfo \
        -DZ3_TRUST_SYSTEM_VERSION=on \
        /adacc_source \
        && cmake --build .

WORKDIR /adacc_build
RUN cmake -G Ninja \
        -DQSYM_BACKEND=ON \
        -DCMAKE_BUILD_TYPE=RelWithDebInfo \
        -DZ3_TRUST_SYSTEM_VERSION=on \
        /adacc_source \
    && cmake --build . \
    && /root/.cargo/bin/cargo install --path /adacc_source/util/symcc_fuzzing_helper

# Build libc++ with SymCC using the simple backend
WORKDIR /libcxx_adacc
RUN export SYMCC_REGULAR_LIBCXX=yes SYMCC_NO_SYMBOLIC_INPUT=yes \
    && mkdir /libcxx_adacc_build \
    && cd /libcxx_adacc_build \
    && cmake -G Ninja /llvm_source/llvm \
         -DLLVM_ENABLE_PROJECTS="libcxx;libcxxabi" \
         -DLLVM_TARGETS_TO_BUILD="X86" \
         -DLLVM_DISTRIBUTION_COMPONENTS="cxx;cxxabi;cxx-headers" \
         -DCMAKE_BUILD_TYPE=Release \
         -DCMAKE_INSTALL_PREFIX=/libcxx_adacc_install \
         -DCMAKE_C_COMPILER=/adacc_build_simple/symcc \
         -DCMAKE_CXX_COMPILER=/adacc_build_simple/sym++ \
    && ninja distribution \
    && ninja install-distribution

#
# The final image
#
FROM ubuntu:22.04

RUN apt-get update \
    && DEBIAN_FRONTEND=noninteractive apt-get install -y \
        build-essential \
        clang-12 \
        g++ \
        libllvm12 \
        zlib1g \
        sudo \
    && rm -rf /var/lib/apt/lists/*

COPY --from=builder /adacc_build /adacc_build
COPY --from=builder /root/.cargo/bin/symcc_fuzzing_helper /adacc_build/
COPY util/pure_concolic_execution.sh /adacc_build/
COPY --from=builder /libcxx_adacc_install /libcxx_adacc_install
COPY --from=builder /afl /afl

ENV PATH /adacc_build:$PATH
ENV AFL_PATH /afl
ENV AFL_CC clang-12
ENV AFL_CXX clang++-12
ENV SYMCC_LIBCXX_PATH=/libcxx_adacc_install

COPY sample.cpp /home/ubuntu/
RUN mkdir /tmp/output
