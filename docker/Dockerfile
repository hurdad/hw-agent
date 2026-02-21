FROM ubuntu:24.04 AS build

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    git \
    pkg-config \
    libhiredis-dev \
    libyaml-cpp-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .
RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_FLAGS_RELEASE='-O2 -D_FORTIFY_SOURCE=2 -fstack-protector-strong -fPIE' \
    -DCMAKE_CXX_FLAGS_RELEASE='-O2 -D_FORTIFY_SOURCE=2 -fstack-protector-strong -fPIE' \
    -DCMAKE_EXE_LINKER_FLAGS='-Wl,-z,relro,-z,now -pie' \
    && cmake --build build -j"$(nproc)" \
    && ctest --test-dir build --output-on-failure

FROM ubuntu:24.04 AS runtime

RUN apt-get update && apt-get install -y --no-install-recommends \
    libhiredis1.1.0 \
    libyaml-cpp0.8 \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY --from=build /src/build/hw_agent /usr/local/bin/hw_agent
COPY --from=build /src/configs /app/configs

CMD ["/usr/local/bin/hw_agent", "/app/configs/agent.yaml"]
