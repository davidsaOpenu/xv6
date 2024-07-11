FROM ubuntu:16.04 as base

# Set the build argument to true if you want to build the linting tools
# conditional is repeated in the RUN commands to:
# - enable caching
# - speed up the build process
# - avoid unnecessary downloads
# Note: Building with linting enabled significantly increases the build time.
#       Consider this if time is a constraint.
ARG BUILD_LINTING_TOOLS
ENV DEBIAN_FRONTEND=noninteractive
# Update package lists and install dependencies
RUN if [ "$BUILD_LINTING_TOOLS" = "true" ]; then \
    apt-get update && \
    apt-get install -y \
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
    libffi-dev \
    gcc-multilib \
    jq; \
    fi
# libvirt-clients causes an error when installing on Ubuntu 16.04

# Download and build Python 3.6 from source - 3.6 or higher is required for cmake but Ubuntu 16.04 only has 3.5. Solution was to install from source.
WORKDIR /opt
RUN if [ "$BUILD_LINTING_TOOLS" = "true" ]; then \
    curl -LO https://www.python.org/ftp/python/3.6.13/Python-3.6.13.tgz && \
    tar -xf Python-3.6.13.tgz && \
    cd Python-3.6.13 && \
    ./configure --enable-optimizations && \
    make -j$(nproc) && \
    make install; \
    fi

# Install CMake 3.21.3 - cmake 3.13 or higher is required for clang-format but Ubuntu 16.04 only has 3.5. Solution was to install from source again.
RUN if [ "$BUILD_LINTING_TOOLS" = "true" ]; then \
    curl -LO https://github.com/Kitware/CMake/releases/download/v3.21.3/cmake-3.21.3.tar.gz && \
    tar -xf cmake-3.21.3.tar.gz && \
    cd cmake-3.21.3 && \
    ./bootstrap && \
    make -j$(nproc) && \
    make install; \
    fi

# Download and extract Cppcheck
WORKDIR /opt
RUN if [ "$BUILD_LINTING_TOOLS" = "true" ]; then \
    curl -LO https://github.com/danmar/cppcheck/archive/2.11.tar.gz && \
    tar -xf 2.11.tar.gz; \
    fi

# Build and install Cppcheck
WORKDIR /opt/cppcheck-2.11
RUN if [ "$BUILD_LINTING_TOOLS" = "true" ]; then \
    make MATCHCOMPILER=yes FILESDIR=/usr/share/cppcheck HAVE_RULES=yes \
    CXXFLAGS="-O2 -DNDEBUG -Wall -Wno-sign-compare -Wno-unused-function" \
    install; \
    fi

# Clone the llvm-project repository
WORKDIR /opt/llvm-project
RUN if [ "$BUILD_LINTING_TOOLS" = "true" ]; then \
    git clone --depth 1 --branch llvmorg-16.0.6 \
    https://github.com/llvm/llvm-project.git clang-format; \
    fi

# Build and install clang-format
WORKDIR /opt/llvm-project/clang-format/build

# gcc-7 is required to build clang-format. Ubuntu 16.04 only has gcc-5. Solution was to add a PPA and install gcc-7.
RUN if [ "$BUILD_LINTING_TOOLS" = "true" ]; then \
    apt-get update && apt-get install -y software-properties-common; \
    fi
RUN if [ "$BUILD_LINTING_TOOLS" = "true" ]; then \
    add-apt-repository -y ppa:ubuntu-toolchain-r/test; \
    fi
RUN if [ "$BUILD_LINTING_TOOLS" = "true" ]; then \
    apt-get update && apt-get install -y gcc-7 g++-7; \
    fi
ENV CC=/usr/bin/gcc-7 CXX=/usr/bin/g++-7

RUN  if [ "$BUILD_LINTING_TOOLS" = "true" ]; then \
    cmake -G "Unix Makefiles" -DLLVM_ENABLE_PROJECTS="clang" \
    -DCMAKE_BUILD_TYPE=Release ../llvm && \
    make clang-format && \
    cp bin/clang-format /usr/local/bin; \
    fi

# Install cpplint+bashate for the user in a venv, and activate it.
ENV XV6_VENV=/xv6-venv
RUN python3 -m venv $XV6_VENV
ENV PATH=$XV6_VENV/bin:$PATH
RUN pip install cpplint bashate && \
    chmod -R 777 $XV6_VENV

# Create a non-root user with the same username,uid,gid
# as the user running the container
ARG USERNAME
ARG GRPNAME
ARG UID
ARG GID
RUN if [ "$BUILD_LINTING_TOOLS" = "true" ]; then \
    groupadd -g $GID $GRPNAME && \
    useradd -u $UID -g $GID -s /bin/bash $USERNAME; \
    fi

# Change user
USER $USERNAME

# Set the working directory inside the container
WORKDIR /home/$USERNAME/xv6

# Set the default command to start the container
CMD ["/bin/bash"]

# Set the entrypoint command
# ENTRYPOINT ["/home/$USERNAME/xv6/run-ci.sh"]
