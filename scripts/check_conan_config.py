#!/usr/bin/env python3
"""
Check if Conan configuration has changed and needs reinstallation.

This script computes a hash of Conan configuration files (conanfile.txt, profile)
and CONAN_* environment variables, then compares it with a stored hash to determine
if Conan dependencies need to be reinstalled.

Exit codes:
  0 - Configuration is up-to-date, no reinstall needed
  1 - Configuration has changed or is missing, reinstall needed
  2 - Error occurred during execution
"""

import argparse
import hashlib
import json
import os
import sys
from datetime import datetime
from pathlib import Path


def compute_file_hash(filepath: Path) -> str:
    """Compute MD5 hash of a file."""
    if not filepath.exists():
        return ""

    md5 = hashlib.md5()
    with open(filepath, "rb") as f:
        for chunk in iter(lambda: f.read(4096), b""):
            md5.update(chunk)
    return md5.hexdigest()


def get_conan_env_vars() -> dict:
    """Get all CONAN_* environment variables, sorted by key."""
    return {k: v for k, v in sorted(os.environ.items()) if k.startswith("CONAN_")}


def compute_config_hash(
    conanfile: Path, profile: Path, generator: str = "", is_multi_config: bool = False
) -> str:
    """Compute combined hash of all Conan configuration."""
    md5 = hashlib.md5()

    # Hash conanfile.txt
    conanfile_hash = compute_file_hash(conanfile)
    md5.update(f"conanfile:{conanfile_hash}".encode())

    # Hash profile file (empty string if doesn't exist)
    profile_hash = compute_file_hash(profile) if profile.exists() else ""
    md5.update(f"profile:{profile_hash}".encode())

    # Hash generator type and multi-config flag (critical for determining installation strategy)
    md5.update(f"generator:{generator}".encode())
    md5.update(f"multi_config:{is_multi_config}".encode())

    # Hash CONAN_* environment variables (sorted)
    env_vars = get_conan_env_vars()
    for key, value in env_vars.items():
        md5.update(f"{key}={value}".encode())

    return md5.hexdigest()


def load_hash_file(hash_file: Path) -> dict:
    """Load existing hash file, return empty dict if not exists or invalid."""
    if not hash_file.exists():
        return {}

    try:
        with open(hash_file, "r") as f:
            return json.load(f)
    except (json.JSONDecodeError, IOError):
        return {}


def save_hash_file(
    hash_file: Path,
    config_hash: str,
    conanfile: Path,
    profile: Path,
    generator: str = "",
    is_multi_config: bool = False,
):
    """Save hash file with current configuration."""
    hash_file.parent.mkdir(parents=True, exist_ok=True)

    data = {
        "hash": config_hash,
        "timestamp": datetime.utcnow().isoformat() + "Z",
        "generator": generator,
        "multi_config": is_multi_config,
        "files": {
            "conanfile.txt": str(conanfile.absolute()),
            "conanfile_hash": compute_file_hash(conanfile),
            "profile": str(profile.absolute()) if profile.exists() else None,
            "profile_hash": compute_file_hash(profile) if profile.exists() else None,
        },
        "env": get_conan_env_vars(),
    }

    with open(hash_file, "w") as f:
        json.dump(data, f, indent=2)


def main():
    parser = argparse.ArgumentParser(
        description="Check if Conan configuration has changed"
    )
    parser.add_argument(
        "--conanfile", required=True, type=Path, help="Path to conanfile.txt"
    )
    parser.add_argument(
        "--profile", required=True, type=Path, help="Path to Conan profile file"
    )
    parser.add_argument(
        "--hash-file", required=True, type=Path, help="Path to hash file"
    )
    parser.add_argument("--generator", default="", help="CMake generator name")
    parser.add_argument(
        "--multi-config",
        default="FALSE",
        help="Whether this is a multi-config generator (TRUE/FALSE)",
    )
    parser.add_argument(
        "--update-hash",
        action="store_true",
        help="Update hash file after successful install",
    )

    args = parser.parse_args()

    try:
        # Verify conanfile exists
        if not args.conanfile.exists():
            print(f"Error: conanfile not found: {args.conanfile}", file=sys.stderr)
            return 2

        # Parse multi-config flag
        is_multi_config = args.multi_config.upper() in ("TRUE", "ON", "1", "YES")

        # Compute current configuration hash
        current_hash = compute_config_hash(
            args.conanfile, args.profile, args.generator, is_multi_config
        )

        if args.update_hash:
            # Update mode: save the hash file
            save_hash_file(
                args.hash_file,
                current_hash,
                args.conanfile,
                args.profile,
                args.generator,
                is_multi_config,
            )
            print(f"Updated hash file: {args.hash_file}")
            return 0
        else:
            # Check mode: compare with stored hash
            stored_data = load_hash_file(args.hash_file)
            stored_hash = stored_data.get("hash", "")

            if not stored_hash:
                print("No stored hash found - reinstall needed")
                return 1

            if current_hash != stored_hash:
                print("Configuration has changed - reinstall needed")
                print(f"  Stored hash:  {stored_hash}")
                print(f"  Current hash: {current_hash}")
                return 1

            print("Configuration is up-to-date")
            return 0

    except Exception as e:
        print(f"Error: {e}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
