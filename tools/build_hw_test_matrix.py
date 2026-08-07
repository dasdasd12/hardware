#!/usr/bin/env python
"""Build hardware-test firmware from hw_tests/catalog.json."""

from __future__ import print_function

import argparse
import io
import json
import os
import subprocess
import sys


ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), os.pardir))
CATALOG = os.path.join(ROOT, "hw_tests", "catalog.json")


def load_tests():
    with io.open(CATALOG, "r", encoding="utf-8") as handle:
        return json.load(handle)["tests"]


def selected(entry, group):
    if group == "all":
        return True
    if group in ("h417", "ch585"):
        return entry["chip"] == group
    return entry["status"] == group


def configurations(entry, include_aliases):
    build = entry["build"]
    values = [build.get("value")]
    if include_aliases:
        values.extend(build.get("aliases", []))
    variants = build.get("variants") or [{}]
    for value in values:
        for variant in variants:
            yield value, variant


def label_for(entry, value, variant):
    parts = [entry["id"]]
    if value and value != entry["build"].get("value"):
        parts.append("alias=%s" % value)
    for key in sorted(variant):
        parts.append("%s=%s" % (key, variant[key]))
    return " ".join(parts)


def command_for(entry, value, variant, force):
    build = entry["build"]
    command = ["make"]
    if force:
        command.append("-B")
    command.extend(["-C", build["directory"]])
    if build["kind"] == "make-selector":
        command.append("%s=%s" % (build["selector"], value))
        if build["directory"] == "hw_tests/h417":
            command.append("BUILD_ROOT=build/catalog")
        elif build["directory"] == "hw_tests/ch585":
            command.append("BUILD_ROOT=build/catalog")
    else:
        command.append("BUILD_ROOT=build/catalog")
    for key in sorted(variant):
        command.append("%s=%s" % (key, variant[key]))
    return command


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--group",
        choices=("all", "h417", "ch585", "active", "bringup", "feasibility"),
        default="all",
    )
    parser.add_argument("--force", action="store_true", help="pass -B to make")
    parser.add_argument("--include-aliases", action="store_true")
    parser.add_argument("--keep-going", action="store_true")
    parser.add_argument("--list", action="store_true")
    args = parser.parse_args(argv)

    jobs = []
    for entry in load_tests():
        if not selected(entry, args.group):
            continue
        for value, variant in configurations(entry, args.include_aliases):
            jobs.append((entry, value, variant))

    if args.list:
        for entry, value, variant in jobs:
            print(label_for(entry, value, variant))
        print("%u build configurations" % len(jobs))
        return 0

    failures = []
    for index, (entry, value, variant) in enumerate(jobs, 1):
        label = label_for(entry, value, variant)
        command = command_for(entry, value, variant, args.force)
        process = subprocess.Popen(
            command,
            cwd=ROOT,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            universal_newlines=True,
        )
        output, _unused = process.communicate()
        if process.returncode == 0:
            print("[%u/%u] PASS %s" % (index, len(jobs), label), flush=True)
        else:
            print("[%u/%u] FAIL %s" % (index, len(jobs), label), flush=True)
            tail = output.splitlines()[-30:]
            for line in tail:
                print("  %s" % line)
            failures.append(label)
            if not args.keep_going:
                break

    if failures:
        print("FAILED: %u configuration(s)" % len(failures))
        return 1
    print("PASS: %u hardware build configuration(s)" % len(jobs))
    return 0


if __name__ == "__main__":
    sys.exit(main())
