# A Note about clang-format
# -------------------------
# Ubuntu has almost all versions of clang-format in its repositories, meaning
# There is no reason to compile LLVM tooling from scratch in the docker container!
# See: https://pkgs.org/search/?q=clang-format
#
# While as of date, clang-format 20 is available, we'll settle on clang-format
# 17 for compatibility reasons. Version 17 is also available in the repositories
# of newer Ubuntu versions, in case we want to upgrade in the future!
#
# ~ Ron

FROM ubuntu:24.04

RUN apt update && \
    apt install --yes gcc make perl python3 jq expect wget curl qemu-system-x86\
    git vim clang-format-16

CMD bash