#!/usr/bin/env python3

"""Dump metadata for a specific waveguide node from regression logs."""

import argparse
import re


def extract(line: str):
    match = re.search(r"node (\d+).*locator \((\d+), (\d+), (\d+)\).*boundary_type (\d+).*boundary_index (\d+).*coeffs (.*)", line)
    if match:
        return {
            "node": int(match.group(1)),
            "locator": tuple(int(match.group(i)) for i in range(2, 5)),
            "boundary_type": int(match.group(5)),
            "boundary_index": int(match.group(6)),
            "coeffs": match.group(7),
        }
    return None


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("log", help="Log file from regression run")
    args = parser.parse_args()

    with open(args.log) as fh:
        for line in fh:
            if "[waveguide]" in line:
                data = extract(line)
                if data:
                    print(data)


if __name__ == "__main__":
    main()
