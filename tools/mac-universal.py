#!/usr/bin/env python3
"""
Based on Dolphin's BuildMacOSUniversalBinary.py
"""

import filecmp
import glob
import os
import shutil
import sys
import subprocess


def lipo(path0, path1, dst):
    if subprocess.call(["lipo", "-create", "-output", dst, path0, path1]) != 0:
        print(f"WARNING: {path0} and {path1} cannot be lipo'd")

        shutil.copy(path0, dst)


def recursive_merge_binaries(src0, src1, dst):
    """
    Merges two build trees together for different architectures into a single
    universal binary.

    The rules for merging are:

    1) Files that exist in either src tree are copied into the dst tree
    2) Files that exist in both trees and are identical are copied over
       unmodified
    3) Files that exist in both trees and are non-identical are lipo'd
    4) Symlinks are created in the destination tree to mirror the hierarchy in
       the source trees
    """

    # Check that all files present in the folder are of the same type and that
    # links link to the same relative location
    for newpath0 in glob.glob(src0+"/*"):
        filename = os.path.basename(newpath0)
        newpath1 = os.path.join(src1, filename)
        if not os.path.exists(newpath1):
            continue

        if os.path.islink(newpath0) and os.path.islink(newpath1):
            if os.path.relpath(newpath0, src0) == os.path.relpath(newpath1, src1):
                continue

        if os.path.isdir(newpath0) and os.path.isdir(newpath1):
            continue

        # isfile() can be true for links so check that both are not links
        # before checking if they are both files
        if (not os.path.islink(newpath0)) and (not os.path.islink(newpath1)):
            if os.path.isfile(newpath0) and os.path.isfile(newpath1):
                continue

        raise Exception(f"{newpath0} and {newpath1} cannot be " +
                        "merged into a universal binary because they are of " +
                        "incompatible types. Perhaps the installed libraries" +
                        " are from different versions for each architecture")

    for newpath0 in glob.glob(src0+"/*"):
        filename = os.path.basename(newpath0)
        newpath1 = os.path.join(src1, filename)
        new_dst_path = os.path.join(dst, filename)
        if os.path.islink(newpath0):
            # Symlinks will be fixed after files are resolved
            continue

        if not os.path.exists(newpath1):
            if os.path.isdir(newpath0):
                shutil.copytree(newpath0, new_dst_path)
            else:
                shutil.copy(newpath0, new_dst_path)

            continue

        if os.path.isdir(newpath1):
            os.makedirs(new_dst_path)
            recursive_merge_binaries(newpath0, newpath1, new_dst_path)
            continue

        if filecmp.cmp(newpath0, newpath1):
            shutil.copy(newpath0, new_dst_path)
        else:
            lipo(newpath0, newpath1, new_dst_path)

    # Loop over files in src1 and copy missing things over to dst
    for newpath1 in glob.glob(src1+"/*"):
        filename = os.path.basename(newpath1)
        newpath0 = os.path.join(src0, filename)
        new_dst_path = os.path.join(dst, filename)
        if (not os.path.exists(newpath0)) and (not os.path.islink(newpath1)):
            if os.path.isdir(newpath1):
                shutil.copytree(newpath1, new_dst_path)
            else:
                shutil.copy(newpath1, new_dst_path)

    # Fix up symlinks for path0
    for newpath0 in glob.glob(src0+"/*"):
        filename = os.path.basename(newpath0)
        new_dst_path = os.path.join(dst, filename)
        if os.path.islink(newpath0):
            relative_path = os.path.relpath(os.path.realpath(newpath0), src0)
            os.symlink(relative_path, new_dst_path)
    # Fix up symlinks for path1
    for newpath1 in glob.glob(src1+"/*"):
        filename = os.path.basename(newpath1)
        new_dst_path = os.path.join(dst, filename)
        newpath0 = os.path.join(src0, filename)
        if os.path.islink(newpath1) and not os.path.exists(newpath0):
            relative_path = os.path.relpath(os.path.realpath(newpath1), src1)
            os.symlink(relative_path, new_dst_path)


if __name__ == "__main__":
    recursive_merge_binaries(sys.argv[1], sys.argv[2], sys.argv[3])

