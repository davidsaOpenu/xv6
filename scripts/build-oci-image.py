# Author: Ron Shabi <ron@ronsh.net>
# Based on oci_image_extractor.sh

import argparse
import json
import tarfile
import os
import subprocess
from collections import namedtuple

Hash = namedtuple("Hash", ["Type", "Digest"])



def get_blob(dir: str, hash: Hash):
    path = f"{dir}/blobs/{hash.Type}/{hash.Digest}"
    with open(path, "r") as f:
        return json.load(f)


def hash_from_string(s: str) -> Hash:
    t, h = s.split(":")
    return Hash(t, h)

class OciImage:
    def __init__(self, path: str):
        self.path = path
        self.build_path = f"{path}/build"
        self.name = self.path.split('/')[-1]
        self.digests = []
        self.layers = []
        self.files = []

    def get_index_as_json(self):
        with open(f"{self.path}/index.json", 'r') as f:
            return json.load(f)

    def get_blob_as_json(self, hash: Hash):
        with open(f"{self.path}/blobs/{hash.Type}/{hash.Digest}", 'r') as f:
            return json.load(f)

    def make_build_dir(self):
        if not os.path.exists(self.build_path):
            os.mkdir(self.build_path)

    def extract(self):
        index = self.get_index_as_json()

        for m in index["manifests"]:
            if m["mediaType"] == "application/vnd.oci.image.manifest.v1+json":
                self.digests.append(hash_from_string(m["digest"]))

        for h in self.digests:
            blob = self.get_blob_as_json(h)

            for layer in blob["layers"]:
                self.layers.append(hash_from_string(layer["digest"]))

        for l in self.layers:
            tar = tarfile.open(f"{self.path}/blobs/{l.Type}/{l.Digest}")
            self.make_build_dir()
            tar.extractall(self.build_path, filter="data")

            with open(f"{self.path}/{self.name}.attr", "w") as attrfile:
                for name in tar.getnames():
                    self.files.append(f"{self.build_path}/{name}")
                    attrfile.write(f"{name}\n")

    def make_fs(self, makefs_executable_path: str):
        p = subprocess.run(
            [makefs_executable_path, self.name, "1", *self.files, f"{self.path}/{self.name}.attr"]
        )

        p.check_returncode()



def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("path")
    parser.add_argument("mkfs")

    args = parser.parse_args()

    mkfs_executable_path = args.mkfs

    # Hack
    if not mkfs_executable_path.startswith("./"):
        mkfs_executable_path = f"./{mkfs_executable_path}"
        
    image = OciImage(args.path)
    image.extract()
    image.make_fs(mkfs_executable_path)


if __name__ == "__main__":
    main()
