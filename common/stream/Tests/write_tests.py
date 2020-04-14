#!/usr/bin/env python3

import testdata

import hashlib
import json
import os
import subprocess
import sys


BOM = b'\xEF\xBB\xBF'


def gen_compr_variants(name, content, limit):
    yield testdata.Doc(name + ".txt", content, limit, None)
    yield testdata.Doc(name + ".txt.gz", content, limit, "gz")
    yield testdata.Doc(name + ".txt.bz2", content, limit, "bz2")
    yield testdata.Doc(name + ".txt.xz", content, limit, "xz")
    yield testdata.Doc(name + ".txt.lz4", content, limit, "lz4")


def gen_docs():
    input = testdata.SHERLOCK

    # Whole file
    yield from gen_compr_variants('sherlock', input, None)

    # Empty file
    yield from gen_compr_variants('empty', b'', None)

    # First 16 lines
    head = b'\n'.join(input.split(b'\n')[:16]) + b'\n'
    yield from gen_compr_variants('small', head, None)

    # Buffer size boundary cases
    for base_size in [1024, 2048, 4096, 8192, 16384]:
        for delta in [-1, 0, 1]:
            size = base_size + delta
            yield from gen_compr_variants(f'block{size}', input, size)


def test_write(opener, text_mode, doc):
    # determine a file name to write to
    filename = doc.pick_tmp_name()
    test = f"write {opener} {doc.name}"

    print()

    if os.path.exists(filename):
        os.remove(filename)

    cmd = ['streamcat', 'write', filename, opener]
    results = subprocess.run(cmd, input=doc.content, stderr=subprocess.PIPE)
    if results.returncode != 0 or results.stderr:
        print(
            f"Test {test}: streamcat returned with exit code {results.returncode}:\n{results.stderr or ''}")
        return False

    if not os.path.exists(filename):
        print(f"Test {test} failed to create file '{filename}'")
        return False

    # Trial run to rule out i/o errors
    open(filename, 'rb').read()

    try:
        output = doc.read(filename)  # should decompress it
    except Exception as e:
        print(f"Test {test} failed on file {filename}: {e}")
        return False

    complaint = doc.verify(output, text_mode)

    if complaint:
        print(f"Test {test} failed: {complaint}")
        return False
    else:
        print(f"Test {test} OK")
        os.remove(filename)
        return True


def test_writes(doc):
    failures = 0

    failures += not test_write('wstream', False, doc)
    failures += not test_write('wastream', True, doc)

    return failures


def all_tests(filename_filter):
    failures = 0
    for d in gen_docs():
        if not filename_filter(d.name):
            continue
        failures += test_writes(d)

    return failures


# if __name__ == "__main__":
#   docname = "small.txt.gz"
#   doc = [d for d in gen_docs()][0]
#   test_write('rastream', True, doc)
