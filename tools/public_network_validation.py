#!/usr/bin/env python3
"""Run reproducible public-network validation for Blocxxi examples."""

from __future__ import annotations

import argparse
import re
import socket
import subprocess
import sys
from pathlib import Path


MAINLINE_DEFAULTS = [
    "67.215.246.10:6881",
    "212.129.33.59:6881",
    "82.221.103.244:6881",
]


def run_mainline_validation(example: Path, duration_ms: int) -> tuple[int, str]:
    command = [
        str(example),
        "--bind-address",
        "0.0.0.0",
        "--bootstrap",
        MAINLINE_DEFAULTS[0],
        "--bootstrap",
        MAINLINE_DEFAULTS[1],
        "--bootstrap",
        MAINLINE_DEFAULTS[2],
        "--remote",
        MAINLINE_DEFAULTS[0],
        "--query",
        "discover",
        "--duration-ms",
        str(duration_ms),
    ]
    completed = subprocess.run(
        command,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        check=False,
    )
    return completed.returncode, completed.stdout


def signet_tcp_validation(host: str, port: int, timeout: float) -> tuple[str, list[str]]:
    infos = socket.getaddrinfo(host, port, type=socket.SOCK_STREAM)
    addresses: list[str] = []
    for info in infos:
        address = info[4][0]
        if address not in addresses:
            addresses.append(address)

    for address in addresses:
        family = socket.AF_INET6 if ":" in address else socket.AF_INET
        probe = socket.socket(family, socket.SOCK_STREAM)
        probe.settimeout(timeout)
        try:
            probe.connect((address, port))
            return address, addresses
        except OSError:
            pass
        finally:
            probe.close()
    raise RuntimeError(f"unable to connect to {host}:{port}")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--build-dir",
        default="out/build-ninja/bin/Debug",
        help="Directory containing built example binaries",
    )
    parser.add_argument(
        "--duration-ms",
        type=int,
        default=12000,
        help="Mainline discovery duration",
    )
    parser.add_argument(
        "--signet-host",
        default="seed.signet.bitcoin.sprovoost.nl",
        help="Public signet seed hostname",
    )
    parser.add_argument(
        "--signet-port",
        type=int,
        default=38333,
        help="Public signet seed TCP port",
    )
    parser.add_argument(
        "--tcp-timeout",
        type=float,
        default=3.0,
        help="TCP connection timeout in seconds",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    build_dir = Path(args.build_dir)
    mainline = build_dir / "mainline-node"
    if not mainline.exists():
        print(f"ERROR missing example binary: {mainline}", file=sys.stderr)
        return 1

    exit_code, output = run_mainline_validation(mainline, args.duration_ms)
    print("=== MAINLINE VALIDATION ===")
    print(output, end="")
    if exit_code != 0:
        print(f"ERROR mainline-node exited with {exit_code}", file=sys.stderr)
        return exit_code

    bootstrap_ok = "BOOTSTRAP_OK" in output
    discovered = re.findall(r"DISCOVERY_TOTAL (\d+)", output)
    discovered_count = int(discovered[-1]) if discovered else 0
    if not bootstrap_ok or discovered_count == 0:
        print(
            "ERROR mainline validation did not report bootstrap success and discoveries",
            file=sys.stderr,
        )
        return 1

    print("=== SIGNET TCP VALIDATION ===")
    connected, addresses = signet_tcp_validation(
        args.signet_host, args.signet_port, args.tcp_timeout
    )
    print(f"SIGNET_RESOLVED {args.signet_host} {addresses}")
    print(f"SIGNET_TCP_OK {connected} {args.signet_port}")
    print("=== SUMMARY ===")
    print(f"MAINLINE_BOOTSTRAP_OK {bootstrap_ok}")
    print(f"MAINLINE_DISCOVERED_TOTAL {discovered_count}")
    print(f"SIGNET_REACHABLE {connected}:{args.signet_port}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
