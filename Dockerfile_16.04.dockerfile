# Build image
# docker build --build-arg USERNAME=$(whoami) \
#              --build-arg GRPNAME=$(id -gn) \
#              --build-arg UID=$(id -u)
#              --build-arg GID=$(id -g) -t xv6-test-image-16.04 .

# Run tests
# docker run --mount type=bind,source="$(pwd)",target=/home/$(whoami)/xv6 \
#            --rm xv6-test-image-16-04 \
#            /home/$(whoami)/xv6/run-ci.sh

# Interactive run
# docker run -it \
#            --mount type=bind,source="$(pwd)",target=/home/$(whoami)/xv6 \
#            --rm xv6-test-image-16-04 
FROM ubuntu:16.04 as base

# Update package lists and install dependencies
RUN apt-get update
RUN DEBIAN_FRONTEND=noninteractive apt-get install -y \
    g++ \
    make \
    libpcre3-dev \
    curl \
    tar \
    vim \
    git \
    sudo \
    python3-software-properties \
    software-properties-common \
    qemu-kvm \
    libvirt-daemon-system \
    bridge-utils \
    virt-manager \
    expect \
    libssl-dev \
    zlib1g-dev \
    libbz2-dev \
    libreadline-dev \
    libsqlite3-dev \
    wget \
    llvm \
    libncurses5-dev \
    xz-utils \
    tk-dev \
    libxml2-dev \
    libxmlsec1-dev \
    libffi-dev
# libvirt-clients causes an error when installing on Ubuntu 16.04

# Download and build Python 3.6 from source - 3.6 or higher is required for cmake but Ubuntu 16.04 only has 3.5. Solution was to install from source.
WORKDIR /opt
RUN curl -LO https://www.python.org/ftp/python/3.6.13/Python-3.6.13.tgz && \
    tar -xf Python-3.6.13.tgz && \
    cd Python-3.6.13 && \
    ./configure --enable-optimizations && \
    make -j$(nproc) && \
    make install

# Install CMake 3.21.3 - cmake 3.13 or higher is required for clang-format but Ubuntu 16.04 only has 3.5. Solution was to install from source again.
RUN curl -LO https://github.com/Kitware/CMake/releases/download/v3.21.3/cmake-3.21.3.tar.gz && \
    tar -xf cmake-3.21.3.tar.gz && \
    cd cmake-3.21.3 && \
    ./bootstrap && \
    make -j$(nproc) && \
    make install

# Download and extract Cppcheck
WORKDIR /opt
RUN curl -LO https://github.com/danmar/cppcheck/archive/2.11.tar.gz && \
    tar -xf 2.11.tar.gz

# Build and install Cppcheck
WORKDIR /opt/cppcheck-2.11
RUN make MATCHCOMPILER=yes FILESDIR=/usr/share/cppcheck HAVE_RULES=yes \
    CXXFLAGS="-O2 -DNDEBUG -Wall -Wno-sign-compare -Wno-unused-function" \
    install

# Clone the llvm-project repository
WORKDIR /opt/llvm-project
RUN git clone --depth 1 --branch llvmorg-16.0.6 \
    https://github.com/llvm/llvm-project.git clang-format

# Build and install clang-format
WORKDIR /opt/llvm-project/clang-format/build

# gcc-7 is required to build clang-format. Ubuntu 16.04 only has gcc-5. Solution was to add a PPA and install gcc-7.
RUN apt-get update && apt-get install -y software-properties-common
RUN add-apt-repository -y ppa:ubuntu-toolchain-r/test
RUN apt-get update && apt-get install -y gcc-7 g++-7
ENV CC=/usr/bin/gcc-7 CXX=/usr/bin/g++-7

RUN cmake -G "Unix Makefiles" -DLLVM_ENABLE_PROJECTS="clang" \
    -DCMAKE_BUILD_TYPE=Release ../llvm && \
    make clang-format && \
    cp bin/clang-format /usr/local/bin

# Create a non-root user with the same username,uid,gid
# as the user running the container
ARG USERNAME
ARG GRPNAME
ARG UID
ARG GID
RUN groupadd -g $GID $GRPNAME
RUN useradd -u $UID -g $GID -s /bin/bash $USERNAME

# Change user
USER $USERNAME

# Set the working directory inside the container
WORKDIR /home/$USERNAME/xv6

# Set the default command to start the container
CMD ["/bin/bash"]

# Set the entrypoint command
# ENTRYPOINT ["/home/$USERNAME/xv6/run-ci.sh"]