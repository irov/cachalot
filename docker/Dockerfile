FROM debian:10 as builder

ENV DEBUG=True

RUN apt-get update && apt-get install -y \
  build-essential \
  libsnappy-dev \
  libssl-dev \
  libz-dev \
  openssl \
  python3 \
  cmake \
  git

WORKDIR /app

COPY . /app/

WORKDIR /app/build/downloads/
RUN bash ./downloads_unix.bash

WORKDIR /app/build/unix/
RUN bash ./build_dependencies_debug.bash 
RUN bash ./build_solution_debug.bash


FROM bitnami/minideb:buster

RUN install_packages strace 
ENV DEBUG=False

ENV CACHALOT__TOKEN="NOT_FOR_PROD"

LABEL org.opencontainers.image.title = "Cachalot" \
  org.opencontainers.image.licenses="MIT" \
  org.opencontainers.image.source="https://github.com/irov/cachalot" \
  org.opencontainers.image.version="0.0.1-dev"

WORKDIR /app
COPY --from=builder /app/bin/cachalot_unix/ /app/tmp
RUN ["cp", "/app/tmp/Unix Makefiles/Debug/ch_grid", "/app"]

RUN echo '{ "max_thread": 8, "max_record": 10000, "max_time": 604800, "grid_uri": "0.0.0.0", "grid_port": 5555, "log_file": "/var/log/grid/cachalot.log", "log_verboselevel": "debug", "name": "grid" }' >> /app/docker_config.json

EXPOSE 5555
RUN chmod +X ./ch_grid
ENTRYPOINT ["/app/ch_grid"]
CMD ["--config", "/app/docker_config.json"]

