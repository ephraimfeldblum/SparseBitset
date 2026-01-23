# Dockerfile to build and run vEBitset tests
FROM ubuntu:22.04
ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
       build-essential cmake python3 python3-venv python3-pip redis-server git wget ca-certificates software-properties-common lsb-release apt-transport-https gnupg \
    && rm -rf /var/lib/apt/lists/*

# Install newer GCC/G++ (14) for full C++23 support
RUN apt-get update \
    && add-apt-repository ppa:ubuntu-toolchain-r/test -y \
    && apt-get update \
    && apt-get install -y --no-install-recommends gcc-14 g++-14 \
    && update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-14 60 \
    && update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-14 60 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /workspace

# Copy repository into container
COPY . /workspace

# Normalize line endings and make helper script executable
RUN sed -i 's/\r$//' /workspace/run_flow_tests.sh || true
RUN chmod +x /workspace/run_flow_tests.sh || true

# Default command: open a shell. Use the run script to execute tests.
CMD ["/bin/bash"]
