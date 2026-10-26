# ripgrep's benchmark suite with alf, on Linux (Debian trixie: ripgrep, GNU grep, ag, ugrep).
#
#   docker build -f bench/ripgrep-suite/Dockerfile -t alf-rg-suite .
#   docker volume create alf-rg-data      # the corpora live on the VM's own disk, not a share
#   docker run --rm -v alf-rg-data:/data -v "$PWD/bench/results:/results" alf-rg-suite
#
FROM debian:trixie-slim
RUN apt-get update \
 && apt-get install -y --no-install-recommends \
      build-essential cmake python3 curl ca-certificates patch gzip \
      ripgrep grep silversearcher-ag ugrep locales \
 && rm -rf /var/lib/apt/lists/* \
 && sed -i 's/^# *en_US.UTF-8/en_US.UTF-8/' /etc/locale.gen && locale-gen >/dev/null
# (the suite runs "grep" with LC_ALL=en_US.UTF-8 for Unicode; without the locale installed,
# grep silently falls back to ASCII rules)
WORKDIR /alf
COPY CMakeLists.txt Config.h.in ./
COPY libalf libalf
COPY src src
COPY bench/ripgrep-suite bench/ripgrep-suite
RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DALF_BUILD_TESTS=OFF \
 && cmake --build build --parallel \
 && cp build/alf /usr/local/bin/alf
CMD ["sh", "bench/ripgrep-suite/run.sh", "/data", "/results/rg-suite-linux-docker"]
