# syntax=docker/dockerfile:1

# Ubuntu + g++-11 + Ninja Multi-Config + Conan from a local venv.
FROM ubuntu:22.04 AS builder
LABEL authors="raaveinm"

ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential g++-11 gcc-11 cmake ninja-build \
    python3 python3-venv python3-pip \
    pkg-config git ca-certificates libssl-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# conan_conf.cmake looks for the conan executable at <source-dir>/.venv/bin/conan.
# conan_conf.cmake passes --profile:host=default, so a default profile has to
# exist before the cmake configure step invokes conan install.
RUN python3 -m venv .venv \
    && .venv/bin/pip install --no-cache-dir conan \
    && .venv/bin/conan profile detect

COPY conanfile.txt conan_conf.cmake CMakeLists.txt ./
COPY main.cpp ./main.cpp
COPY src ./src
COPY static ./static

# First-time configure can't use the conan-release preset
RUN cmake -S . -B build -G "Ninja Multi-Config" \
    -DCMAKE_C_COMPILER=/usr/bin/gcc-11 \
    -DCMAKE_CXX_COMPILER=/usr/bin/g++-11 \
    -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build --config Release

# Runtime image
FROM ubuntu:22.04 AS runtime

RUN apt-get update && apt-get install -y --no-install-recommends \
    libssl3 libpq5 ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY --from=builder /app/build/Release/PickUsAllBackend ./build/Release/PickUsAllBackend
COPY --from=builder /app/static ./static

ENV PICASSO_BIND=0.0.0.0 \
    PICASSO_PORT=8000

EXPOSE 8000

ENTRYPOINT ["./build/Release/PickUsAllBackend"]
