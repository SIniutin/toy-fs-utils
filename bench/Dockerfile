FROM ubuntu:24.04

RUN apt-get update && apt-get install -y \
    bash \
    cmake \
    gcc \
    g++ \
    make \
    && rm -rf /var/lib/apt/lists

WORKDIR /app
COPY . /app

RUN cmake -S . -B build_bench \
    -DCMAKE_C_COMPILER=gcc \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_BENCHMARKS=ON
RUN cmake --build build_bench --target bench_crawler

ENV BENCH_BIN=/app/build_bench/bin/bench_crawler
ENV BENCH_MODELS="A B"
ENV BENCH_THREADS="1 2 4 8"
ENV BENCH_CALLBACKS="cheap heavy"
ENV BENCH_REPEATS=5
ENV BENCH_SHAPE=mixed
ENV BENCH_FILES=100000
ENV BENCH_OUT=/results/results.csv
ENV BENCH_META=/results/metadata.txt

CMD ["bash", "bench/docker_bench.sh"]
