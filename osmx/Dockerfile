FROM ubuntu:18.04 AS builder

ENV OSMEXPRESS_VERSION 0.2.0

RUN apt-get update && apt-get install -y \
  cmake \
  clang \
  git \
  libbz2-dev \
  libz-dev \
  libexpat-dev \
  libssl-dev \
  && rm -rf /var/lib/apt/lists/*
RUN git clone \
  --branch ${OSMEXPRESS_VERSION} \
  --depth 1 \
  --recursive \
  https://github.com/protomaps/OSMExpress.git \
  /usr/src/osmexpress
WORKDIR /usr/src/osmexpress
COPY . /usr/src/osmexpress
RUN cmake -DCMAKE_BUILD_TYPE=Release .
RUN make

FROM ubuntu:18.04

RUN apt-get update && apt-get install -y \
  libexpat1 \
  libssl1.1 \
  && rm -rf /var/lib/apt/lists/*
COPY --from=builder /usr/src/osmexpress/osmx /usr/local/bin/osmx
ENTRYPOINT [ "/usr/local/bin/osmx" ]
