#!/usr/bin/env python3

import argparse
import json
import socket
import subprocess
import sys
import time
from pathlib import Path

import libtorrent as lt


def normalize_bdecode(value):
    if isinstance(value, dict):
        return {
            (
                key.decode("ascii", errors="replace")
                if isinstance(key, bytes)
                else str(key)
            ): normalize_bdecode(item)
            for key, item in value.items()
        }
    if isinstance(value, list):
        return [normalize_bdecode(item) for item in value]
    if isinstance(value, bytes):
        return value.decode("latin1")
    return value


def wait_for_line(process, prefix, timeout_s):
    end = time.time() + timeout_s
    while time.time() < end:
        line = process.stdout.readline()
        if not line:
            time.sleep(0.05)
            continue
        line = line.rstrip("\n")
        print(line)
        if line.startswith(prefix):
            return line
    raise TimeoutError(f"timed out waiting for {prefix!r}")


def default_fixture_binary():
    return Path("out/build-ninja/bin/Debug/mainline-node")


def run_local_mode(args):
    fixture_cmd = [
        str(args.fixture_binary),
        "--bind-address",
        "127.0.0.1",
        "--duration-ms",
        str(args.duration * 1000),
    ]
    process = subprocess.Popen(
        fixture_cmd,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
    )
    try:
        ready = wait_for_line(process, "READY ", 5.0)
        endpoint = ready.split()[1]
        host, port = endpoint.rsplit(":", 1)
        port = int(port)

        udp = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        udp.settimeout(2.0)

        ping_query = {
            b"y": b"q",
            b"q": b"ping",
            b"t": b"aa",
            b"a": {b"id": b"abcdefghij0123456789"},
        }
        udp.sendto(lt.bencode(ping_query), (host, port))
        ping_bytes, response_addr = udp.recvfrom(2048)
        decoded_ping = lt.bdecode(ping_bytes)

        find_node_query = {
            b"y": b"q",
            b"q": b"find_node",
            b"t": b"ab",
            b"a": {
                b"id": b"abcdefghij0123456789",
                b"target": bytes.fromhex(ready.split()[2]),
            },
        }
        udp.sendto(lt.bencode(find_node_query), (host, port))
        find_node_bytes, _ = udp.recvfrom(2048)
        decoded_find_node = lt.bdecode(find_node_bytes)

        observed_queries = []
        end = time.time() + args.duration
        while time.time() < end:
            if process.stdout is None:
                break
            line = process.stdout.readline()
            if not line:
                time.sleep(0.05)
                continue
            line = line.rstrip("\n")
            print(line)
            if line.startswith("QUERY "):
                observed_queries.append(line)
                if len(observed_queries) >= 2:
                    break

        result = {
            "mode": "local",
            "fixture": endpoint,
            "interaction_attempted": ["ping", "find_node"],
            "success": (
                decoded_ping.get(b"y") == b"r"
                and decoded_ping.get(b"t") == b"aa"
                and decoded_find_node.get(b"y") == b"r"
                and decoded_find_node.get(b"t") == b"ab"
                and b"nodes" in decoded_find_node.get(b"r", {})
                and len(decoded_find_node[b"r"][b"nodes"]) == 26
                and len(observed_queries) >= 2
            ),
            "response_from": f"{response_addr[0]}:{response_addr[1]}",
            "decoded_ping": normalize_bdecode(decoded_ping),
            "decoded_find_node": normalize_bdecode(decoded_find_node),
            "observed_queries": observed_queries,
            "verdict": "product-behavior-confirmed"
            if len(observed_queries) >= 2
            else "inconclusive",
        }
        return 0 if result["success"] else 1, result
    finally:
        process.terminate()
        try:
            process.wait(timeout=2)
        except subprocess.TimeoutExpired:
            process.kill()


def run_public_mode(args):
    fixture_cmd = [
        str(args.fixture_binary),
        "--bind-address",
        "0.0.0.0",
        "--duration-ms",
        str(args.duration * 1000),
    ]
    for bootstrap in args.bootstrap:
        fixture_cmd.extend(["--bootstrap", bootstrap])

    process = subprocess.Popen(
        fixture_cmd,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
    )
    successes = []
    try:
        wait_for_line(process, "READY ", 5.0)
        end = time.time() + args.duration
        while time.time() < end:
          line = process.stdout.readline()
          if not line:
              time.sleep(0.05)
              continue
          line = line.rstrip("\n")
          print(line)
          if line.startswith("BOOTSTRAP_OK "):
              successes.append(line.split(" ", 1)[1])
        result = {
            "mode": "public",
            "routers_attempted": args.bootstrap,
            "successes": successes,
            "required_bootstraps": args.bootstrap,
            "interaction_attempted": "find_node bootstrap",
            "timeout_error_classification": (
                "no-bootstrap-response" if not successes else "none"
            ),
            "verdict": "product-behavior-confirmed"
            if successes
            else "inconclusive",
            "success": bool(successes),
        }
        return 0 if successes else 1, result
    finally:
        process.terminate()
        try:
            process.wait(timeout=2)
        except subprocess.TimeoutExpired:
            process.kill()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--mode", choices=["local", "public"], required=True)
    parser.add_argument(
        "--fixture-binary",
        type=Path,
        default=default_fixture_binary(),
    )
    parser.add_argument("--duration", type=int, default=8)
    parser.add_argument("--log-artifact", type=Path)
    parser.add_argument("--bootstrap", action="append", default=[])
    args = parser.parse_args()

    if not args.fixture_binary.exists():
        raise SystemExit(f"fixture binary not found: {args.fixture_binary}")

    if args.mode == "local":
        rc, result = run_local_mode(args)
    else:
        if not args.bootstrap:
            raise SystemExit("--bootstrap is required in public mode")
        rc, result = run_public_mode(args)

    if args.log_artifact:
        args.log_artifact.parent.mkdir(parents=True, exist_ok=True)
        args.log_artifact.write_text(json.dumps(result, indent=2) + "\n")

    print(json.dumps(result, indent=2))
    raise SystemExit(rc)


if __name__ == "__main__":
    main()
