# syntax=docker/dockerfile:1

FROM ubuntu:24.04 AS build
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
        ca-certificates cmake g++ make git curl unzip \
        linux-tools-generic linux-tools-common \
        python3 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY CMakeLists.txt ./
COPY include ./include
COPY src ./src
COPY tests ./tests
COPY scripts ./scripts
COPY docs ./docs
COPY prometheus ./prometheus

RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DLPT_BUILD_TESTS=ON \
    && cmake --build build -j \
    && ctest --test-dir build --output-on-failure

FROM ubuntu:24.04 AS runtime
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
        ca-certificates curl linux-tools-generic linux-tools-common \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /opt/lpt
COPY --from=build /src/build/lpt-agent /src/build/lpt-workload /opt/lpt/
COPY --from=build /src/scripts /opt/lpt/scripts
COPY --from=build /src/docs /opt/lpt/docs
EXPOSE 9100
ENTRYPOINT ["/opt/lpt/lpt-agent"]
CMD ["--listen", "0.0.0.0", "--port", "9100"]
