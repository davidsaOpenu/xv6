#!/bin/python3

# XV6FS debugging and visualization tool, made for catching bugs that are introduced
# by malformed creation of xv6-filesystems.

# Author: Ron Shabi <ron@ronsh.net>

import argparse
import struct
from collections import namedtuple
from typing import List

BSIZE = 1024
SIZEOF_INODE = 64
INODES_PER_BLOCK = BSIZE // SIZEOF_INODE
N_INODES = 600
N_LOG_BLOCKS = 30
N_INODE_BLOCKS = N_INODES // INODES_PER_BLOCK + 1
N_INODE_DIRECT_ADDRS = 12
BLOCK_START_INODE = 2 + N_LOG_BLOCKS

INODE_TYPE_DIR = 1  # Directory
INODE_TYPE_FILE = 2  # File
INODE_TYPE_DEV = 3  # Device
INODE_TYPE_CGFILE = 4  # Cgroup file
INODE_TYPE_CGDIR = 5  # Cgroup directory
INODE_TYPE_PROCFILE = 6  # Proc file
INODE_TYPE_PROCDIR = 7  # Proc directory

DirectoryEntry = namedtuple("DirectoryEntry", ["inode_number", "name"])

def isprint(x):
    return x >= 0x20 and x <= 0x7e

def hexdump(blob: bytes, width=16):
    line_hex = []
    line_chars = []
    for i, byte in enumerate(blob):
        line_hex.append(f"{byte:<02x}")
        line_chars.append(chr(byte) if isprint(byte) else '.')

        if (i + 1) % width == 0:
            print(' '.join(line_hex) + '   ' + ''.join(line_chars))
            line_hex = []
            line_chars = []


def read_block(disk_image_bytes: bytes, block_number: int) -> bytes:
    start = block_number * BSIZE
    end = start + BSIZE

    assert end - start == BSIZE

    return disk_image_bytes[start:end]


def read_block_array(
    disk_image_bytes: bytes, block_number_start: int, amount_of_blocks: int
) -> List[bytes]:
    o = []

    for i in range(amount_of_blocks):
        o.append(read_block(disk_image_bytes, block_number_start + i))

    return o

def ensure_zero(blob: bytes) -> None:
    for x in blob:
        if x != 0:
            raise ValueError("SOME VALUE IS NOT ZERO")


def lsdir(ent, padding=0):
    for obj in ent:
        if isinstance(obj, DirectoryEntry):
            spaces = padding * " "
            print(f"{spaces}{obj.name:<16} {obj.inode_number:<4}")
        else:
            lsdir(obj, padding + 4)



class INode:
    def __init__(self):
        self.type = 0
        self.major = 0
        self.minor = 0
        self.nlink = 0
        self.size = 0
        self.addrs = [0] * N_INODE_DIRECT_ADDRS
        self.indirect = 0

    def check_corruption(self):
        if self.type != INODE_TYPE_DEV and (self.major != 0 or self.minor != 0):
            raise ValueError("Inode is corrupted - major/minor garbage")
        
        if len(self.get_addrs()) != N_INODE_DIRECT_ADDRS and self.indirect != 0:
            raise ValueError("Inode is corrupted - uses indirect block too soon!")
        
        if self.nlink == 0:
            raise ValueError("Inode is corrupted - no links!")


    def is_valid(self) -> bool:
        return self.type in [INODE_TYPE_DIR, INODE_TYPE_FILE, INODE_TYPE_DEV, INODE_TYPE_CGFILE, INODE_TYPE_CGDIR, INODE_TYPE_PROCFILE, INODE_TYPE_PROCDIR]

    def is_dir(self) -> bool:
        return self.type == INODE_TYPE_DIR

    def is_file(self) -> bool:
        return self.type == INODE_TYPE_FILE

    def is_using_indirect(self) -> bool:
        return self.indirect != 0

    def get_type_string(self) -> str:
        return self.type_to_string(self.type)

    def get_addrs(self) -> List[int]:
        o = []
        for a in self.addrs:
            if a == 0:
                break

            o.append(a)

        return o

    @staticmethod
    def type_to_string(tp: int) -> str:
        if tp == INODE_TYPE_DIR:
            return "DIR"
        if tp == INODE_TYPE_FILE:
            return "FILE"
        if tp == INODE_TYPE_DEV:
            return "DEV"
        if tp == INODE_TYPE_CGFILE:
            return "CGFILE"
        if tp == INODE_TYPE_CGDIR:
            return "CGDIR"
        if tp == INODE_TYPE_PROCFILE:
            return "PROCFILE"
        if tp == INODE_TYPE_PROCDIR:
            return "PROCDIR"

        raise ValueError(f"unknown type {tp}")

    @classmethod
    def from_bytes(cls, blob: bytes):
        if len(blob) != SIZEOF_INODE:
            raise ValueError(
                f"If you create an Inode from bytes, it needs to be the size of an inode, not {len(blob)} bytes"
            )

        itype, imaj, imin, inlink, isize, *iaddrs = struct.unpack(
            f"<HHHHI{N_INODE_DIRECT_ADDRS + 1}I", blob
        )
        o = cls()
        o.type = itype
        o.major = imaj
        o.minor = imin
        o.nlink = inlink
        o.size = isize
        o.addrs = iaddrs[:N_INODE_DIRECT_ADDRS]
        o.indirect = iaddrs[N_INODE_DIRECT_ADDRS]

        return o

    def __str__(self) -> str:
        if self.type == INODE_TYPE_DEV:
            return f"INode(type: {self.get_type_string()}, {self.minor}/{self.major}, links: {self.nlink}, size: {self.size})"

        return f"INode(type: {self.get_type_string()}, links: {self.nlink}, size: {self.size})"


class XV6FSImage:
    def __init__(self, file_name: str):
        self._file_name = file_name

        with open(self._file_name, "rb") as f:
            self._bytes = f.read()


        self._sb_size_blocks = 0
        self._sb_n_blocks = 0
        self._sb_n_log = 0
        self._sb_logstart = 0
        self._sb_inodestart = 0
        self._sb_bmapstart = 0
        self._sb_ninodes = 0
        self._inodes: List[INode] = []
        self._used_blocks = set()

        self._bootsector = read_block(self._bytes, 0)
        self._superblock = read_block(self._bytes, 1)
        self._bitmap_block = None
        self._log_blocks = read_block_array(self._bytes, 2, N_LOG_BLOCKS)

        self._read_superblock()
        self._print_superblock()
        self._read_all_inodes()
        self._mark_all_used_blocks()
        self._read_bitmap()

        self._read_directory(self._inodes[1])



    def _read_inode(self, inode_number: int) -> INode:
        if inode_number >= N_INODES:
            raise ValueError("inode number is more than max")

        block = inode_number // INODES_PER_BLOCK + BLOCK_START_INODE
        offset = (inode_number % INODES_PER_BLOCK) * SIZEOF_INODE
        blockbytes = read_block(self._bytes, block)

        buf = blockbytes[offset : offset + SIZEOF_INODE]
        return INode.from_bytes(buf)

    def _read_all_inodes(self):
        reached_end = False

        for i in range(self._sb_ninodes):

            inode = self._read_inode(i)

            if reached_end and inode.is_valid():
                raise ValueError(f"Dangling inode {i} in the wild")

            if i != 0 and inode.type == 0 and not reached_end: # This is the max allocated inode, sure no inode will be used after this
                reached_end = True

            if not reached_end:
                if i != 0:
                    inode.check_corruption()
                self._inodes.append(inode)


    def _mark_all_used_blocks(self):
        def add_addr(addr, error_message_note=""):
            if addr in self._used_blocks:
                raise ValueError(f"Block #{addr} is already used ({error_message_note}!")

            self._used_blocks.add(addr)

        def read_indirect_block(blob: bytes) -> List[int]:
            o = []

            i = 0
            has_reached_end = False
            while i < BSIZE:
                current_addr = struct.unpack("<I", blob[i:i+4])[0]

                if current_addr == 0:
                    has_reached_end = True
                else:
                    if has_reached_end:
                        raise ValueError(f"Indirect block has garbage value at offset {i}, value is {current_addr}")
                    else:
                        o.append(current_addr)

                i += 4

            return o


        for inumber, inode in enumerate(self._inodes):
            if inode.is_valid():
                for block_addr in inode.get_addrs():
                    add_addr(block_addr, f"user: {inode} #{inumber}")

                if inode.is_using_indirect():
                    indirect_block = read_block(self._bytes, inode.indirect)
                    used_by_indirect_block = read_indirect_block(indirect_block)

                    add_addr(inode.indirect)

                    for addr in used_by_indirect_block:
                        add_addr(addr, "used by indirect block {node.indirect}")

        self._used_blocks.update(range(2 + N_LOG_BLOCKS + N_INODE_BLOCKS + 1))





    def _read_superblock(self):
        (
            self._sb_size_blocks,
            self._sb_n_blocks,
            self._sb_n_log,
            self._sb_logstart,
            self._sb_inodestart,
            self._sb_bmapstart,
            self._sb_ninodes,
        ) = struct.unpack("<IIIIIII", self._superblock[:28])

        ensure_zero(self._superblock[28:])
        blocks_from_file= len(self._bytes) / BSIZE
        assert(self._sb_size_blocks == blocks_from_file)
        assert(self._sb_n_blocks == blocks_from_file - 2 - N_LOG_BLOCKS - N_INODE_BLOCKS - 1)
        assert(self._sb_n_log == N_LOG_BLOCKS)
        assert(self._sb_logstart == 2)
        assert(self._sb_inodestart == 2 + N_LOG_BLOCKS)
        assert(self._sb_bmapstart == self._sb_inodestart + N_INODE_BLOCKS)
        assert(self._sb_ninodes == N_INODES)
        

    def _read_bitmap(self):
        self._bitmap_block = read_block(self._bytes, self._sb_bmapstart)

        # Check bitmap
        for idx, byte in enumerate(self._bitmap_block):
            for bi in range(8):
                current_bit_overall = idx * 8 + bi
                value = (byte >> bi) & 1

                # print(idx, "-", hex(byte))

                value_should_be = 1 if current_bit_overall in self._used_blocks else 0
                if value != value_should_be:
                    print(f"Bitmap bit {current_bit_overall} is {value}, it should be {value_should_be}")

                    print(f"BLOCK {current_bit_overall}")
                    hexdump(read_block(self._bytes, current_bit_overall))

    def _read_directory(
        self, inode: INode
    ) -> List[DirectoryEntry | List[DirectoryEntry]]:
        
        assert(inode.is_dir())

        o = []

        for addr in inode.get_addrs():
            datablock = read_block(self._bytes, addr)

            i = 0
            assert len(datablock) % 16 == 0
            while i < len(datablock):
                entry_inumber, entry_name = struct.unpack(
                    "<H14s", datablock[i : i + 16]
                )
                entry_name = entry_name.split(b"\0")[0]
                entry_name = entry_name.decode()

                # Base case
                if entry_inumber == 0:
                    break

                # Read recursively
                if entry_name != "." and entry_name != "..":
                    dir_inside = self._read_inode(entry_inumber)
                    if dir_inside.is_dir():
                        o.append(self._read_directory(dir_inside))

                o.append(DirectoryEntry(entry_inumber, entry_name))
                i += 16

        return o

    def _print_superblock(self):
        print(
            "SUPERBLOCK:\n"
            f"{self._sb_size_blocks=}\n"
            f"{self._sb_n_blocks=}\n"
            f"{self._sb_n_log=}\n"
            f"{self._sb_logstart=}\n"
            f"{self._sb_inodestart=}\n"
            f"{self._sb_bmapstart=}\n"
            f"{self._sb_ninodes=}"
        )


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("image")
    args = parser.parse_args()

    img = XV6FSImage(args.image)


if __name__ == "__main__":
    main()
