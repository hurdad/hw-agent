FROM ubuntu:24.04 AS build

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    libhiredis-dev \
    nlohmann-json3-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .

RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_HW_AGENT=OFF \
    -DBUILD_HW_AGENT_MCP=ON \
    -DCMAKE_C_FLAGS_RELEASE='-O2 -D_FORTIFY_SOURCE=2 -fstack-protector-strong -fPIE' \
    -DCMAKE_CXX_FLAGS_RELEASE='-O2 -D_FORTIFY_SOURCE=2 -fstack-protector-strong -fPIE' \
    -DCMAKE_EXE_LINKER_FLAGS='-Wl,-z,relro,-z,now -pie' \
    && cmake --build build --target hw-agent-mcp -j"$(nproc)"

FROM ubuntu:24.04 AS runtime

RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates \
    libhiredis0.14 \
    && rm -rf /var/lib/apt/lists/*

COPY --from=build /src/build/tools/hw-agent-mcp/hw-agent-mcp /usr/local/bin/hw-agent-mcp

ENTRYPOINT ["/usr/local/bin/hw-agent-mcp"]
