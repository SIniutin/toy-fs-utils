FROM ubuntu:24.04

RUN apt-get update && apt-get install -y \
    cmake make gcc g++ bash \
    && rm -rf /var/lib/apt/lists

WORKDIR /app
COPY . /app

RUN cmake -S . -B build -DCMAKE_C_COMPILER=gcc
RUN cmake --build build

CMD [ "ctest", "--test-dir", "build", "--output-on-failure" ]
