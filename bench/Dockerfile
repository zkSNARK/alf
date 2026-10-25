# Linux benchmark: alf vs GNU grep, ripgrep, GNU sed, gawk and mawk.
#
#   docker build -f bench/Dockerfile --build-arg ALF_REV=$(git rev-parse --short HEAD) -t alf-bench .
#   docker run --rm -v "$PWD/bench/results:/results" alf-bench
#
FROM debian:trixie-slim
RUN apt-get update \
 && apt-get install -y --no-install-recommends \
      build-essential cmake python3 hyperfine ripgrep gawk mawk grep sed \
 && rm -rf /var/lib/apt/lists/*
ARG ALF_REV=""
ENV ALF_REV=$ALF_REV
WORKDIR /alf
COPY CMakeLists.txt Config.h.in ./
COPY libalf libalf
COPY src src
COPY bench/gen_log.py bench/run.py bench/
RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DALF_BUILD_TESTS=OFF \
 && cmake --build build --parallel \
 && mkdir -p /data && python3 bench/gen_log.py /data/server.log
CMD ["python3", "bench/run.py", "--alf", "build/alf", "--log", "/data/server.log", "--out", "/results/linux-docker.json"]
