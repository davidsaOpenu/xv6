FROM ubuntu:22.04 as base
ENV DEBIAN_FRONTEND=noninteractive
# Update package lists and install dependencies
RUN apt-get update && \
    apt-get install -y \
        g++ \
        make \
        libpcre3-dev \
        curl \
        tar \
        python3 \
        vim \
        git \
        cmake \
        sudo \
        python3.10-venv \
        qemu-kvm \
        libvirt-daemon-system \
        libvirt-clients \
        bridge-utils \
        virt-manager \
        expect \
        jq

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

# Install cpplint+bashate for the user
ENV XV6_VENV=/home/$USERNAME/xv6-venv
RUN python3 -m venv $XV6_VENV && \
    $XV6_VENV/bin/activate && \
    pip install cpplint bashate

# Set the default command to start the container
CMD ["/bin/bash"]

# Set the entrypoint command
# ENTRYPOINT ["/home/$USERNAME/xv6/run-ci.sh"]
