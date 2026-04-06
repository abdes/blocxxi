#!/usr/bin/env python3
"""Run reproducible public-network validation for Blocxxi examples."""

from __future__ import annotations

import argparse
import hashlib
import ipaddress
import random
import re
import socket
import struct
import subprocess
import sys
import time
from pathlib import Path


MAINLINE_DEFAULTS = [
    "67.215.246.10:6881",
    "212.129.33.59:6881",
    "82.221.103.244:6881",
]
SIGNET_MAGIC = bytes.fromhex("0a03cf40")
SIGNET_GENESIS_HASH = (
    "00000008819873e925422c1ff0f99f7cc9bbb232af63a077a480a3633bee1ef6"
)
BITCOIN_PROTOCOL_VERSION = 70016


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


def resolve_addresses(host: str, port: int) -> list[str]:
    infos = socket.getaddrinfo(host, port, type=socket.SOCK_STREAM)
    addresses: list[str] = []
    for info in infos:
        address = info[4][0]
        if address not in addresses:
            addresses.append(address)
    return addresses


def compact_size(length: int) -> bytes:
    if length < 253:
        return bytes([length])
    if length <= 0xFFFF:
        return b"\xfd" + struct.pack("<H", length)
    if length <= 0xFFFFFFFF:
        return b"\xfe" + struct.pack("<I", length)
    return b"\xff" + struct.pack("<Q", length)


def read_compact_size(payload: bytes, offset: int = 0) -> tuple[int, int]:
    first = payload[offset]
    if first < 253:
        return first, offset + 1
    if first == 253:
        return struct.unpack_from("<H", payload, offset + 1)[0], offset + 3
    if first == 254:
        return struct.unpack_from("<I", payload, offset + 1)[0], offset + 5
    return struct.unpack_from("<Q", payload, offset + 1)[0], offset + 9


def double_sha256(payload: bytes) -> bytes:
    return hashlib.sha256(hashlib.sha256(payload).digest()).digest()


def encode_network_address(address: str, port: int) -> bytes:
    ip = ipaddress.ip_address(address)
    if ip.version == 4:
        ip_bytes = b"\x00" * 10 + b"\xff\xff" + ip.packed
    else:
        ip_bytes = ip.packed
    return struct.pack("<Q", 0) + ip_bytes + struct.pack(">H", port)


def encode_message(command: str, payload: bytes) -> bytes:
    checksum = double_sha256(payload)[:4]
    return (
        SIGNET_MAGIC
        + command.encode("ascii").ljust(12, b"\x00")
        + struct.pack("<I", len(payload))
        + checksum
        + payload
    )


def encode_version_payload(remote_address: str, remote_port: int) -> bytes:
    user_agent = b"/blocxxi:0.1.0/"
    return b"".join(
        [
            struct.pack("<i", BITCOIN_PROTOCOL_VERSION),
            struct.pack("<Q", 0),
            struct.pack("<q", int(time.time())),
            encode_network_address(remote_address, remote_port),
            encode_network_address("0.0.0.0", 0),
            struct.pack("<Q", random.getrandbits(64)),
            compact_size(len(user_agent)),
            user_agent,
            struct.pack("<i", 0),
            struct.pack("<?", False),
        ]
    )


def encode_getheaders_payload(locator_hashes: list[bytes]) -> bytes:
    return b"".join(
        [
            struct.pack("<i", BITCOIN_PROTOCOL_VERSION),
            compact_size(len(locator_hashes)),
            *locator_hashes,
            b"\x00" * 32,
        ]
    )


def wire_hash_from_hex(block_hash_hex: str) -> bytes:
    return bytes.fromhex(block_hash_hex)[::-1]


def recv_exact(connection: socket.socket, size: int) -> bytes:
    chunks = bytearray()
    while len(chunks) < size:
        chunk = connection.recv(size - len(chunks))
        if not chunk:
            raise RuntimeError("connection closed while reading message")
        chunks.extend(chunk)
    return bytes(chunks)


def read_message(connection: socket.socket) -> tuple[str, bytes]:
    header = recv_exact(connection, 24)
    if header[:4] != SIGNET_MAGIC:
        raise RuntimeError(f"unexpected network magic: {header[:4].hex()}")

    command = header[4:16].rstrip(b"\x00").decode("ascii")
    payload_size = struct.unpack("<I", header[16:20])[0]
    checksum = header[20:24]
    payload = recv_exact(connection, payload_size)
    if double_sha256(payload)[:4] != checksum:
        raise RuntimeError(f"checksum mismatch for {command}")
    return command, payload


def header_hash_to_wire(block_hash_hex: str) -> bytes:
    return bytes.fromhex(block_hash_hex)[::-1]


def parse_headers_payload(payload: bytes) -> tuple[int, list[str]]:
    count, offset = read_compact_size(payload, 0)
    headers: list[str] = []
    for _ in range(count):
        header = payload[offset : offset + 80]
        if len(header) != 80:
            raise RuntimeError("truncated headers payload")
        offset += 80
        tx_count, offset = read_compact_size(payload, offset)
        if tx_count != 0:
            raise RuntimeError("headers message included non-zero txn count")
        headers.append(double_sha256(header)[::-1].hex())
    return count, headers


def parse_block_transaction_count(payload: bytes) -> int:
    if len(payload) < 81:
        raise RuntimeError("block payload too small")
    tx_count, _ = read_compact_size(payload, 80)
    return tx_count


def parse_first_transaction_witness_flag(payload: bytes) -> tuple[bool, str | None, str | None]:
    if len(payload) < 81:
        raise RuntimeError("block payload too small")

    tx_count, offset = read_compact_size(payload, 80)
    if tx_count == 0:
        return False, None, None

    tx_start = offset
    version = payload[offset : offset + 4]
    offset += 4

    has_witness = False
    if offset + 2 <= len(payload) and payload[offset] == 0 and payload[offset + 1] != 0:
        has_witness = True
        offset += 2

    vin_count, vin_count_end = read_compact_size(payload, offset)
    vin_start = offset
    offset = vin_count_end
    for _ in range(vin_count):
        offset += 36
        script_len, offset = read_compact_size(payload, offset)
        offset += script_len
        offset += 4

    vout_count, vout_count_end = read_compact_size(payload, offset)
    vout_start = offset
    offset = vout_count_end
    for _ in range(vout_count):
        offset += 8
        script_len, offset = read_compact_size(payload, offset)
        offset += script_len

    if has_witness:
        for _ in range(vin_count):
            stack_items, offset = read_compact_size(payload, offset)
            for _ in range(stack_items):
                item_len, offset = read_compact_size(payload, offset)
                offset += item_len

    locktime = payload[offset : offset + 4]
    offset += 4

    wtxid = double_sha256(payload[tx_start:offset])[::-1].hex()
    txid_payload = bytearray()
    txid_payload.extend(version)
    txid_payload.extend(payload[vin_start:vin_count_end])
    cursor = vin_count_end
    for _ in range(vin_count):
        txid_payload.extend(payload[cursor : cursor + 36])
        cursor += 36
        script_len, script_end = read_compact_size(payload, cursor)
        txid_payload.extend(payload[cursor:script_end])
        cursor = script_end
        txid_payload.extend(payload[cursor : cursor + script_len])
        cursor += script_len
        txid_payload.extend(payload[cursor : cursor + 4])
        cursor += 4
    txid_payload.extend(payload[vout_start:vout_count_end])
    cursor = vout_count_end
    for _ in range(vout_count):
        txid_payload.extend(payload[cursor : cursor + 8])
        cursor += 8
        script_len, script_end = read_compact_size(payload, cursor)
        txid_payload.extend(payload[cursor:script_end])
        cursor = script_end
        txid_payload.extend(payload[cursor : cursor + script_len])
        cursor += script_len
    txid_payload.extend(locktime)
    txid = double_sha256(bytes(txid_payload))[::-1].hex()
    return has_witness, txid, wtxid


def signet_handshake_validation(
    host: str, port: int, timeout: float
) -> tuple[str, list[str], int | None, list[str]]:
    addresses = resolve_addresses(host, port)

    for address in addresses:
        family = socket.AF_INET6 if ":" in address else socket.AF_INET
        probe = socket.socket(family, socket.SOCK_STREAM)
        probe.settimeout(timeout)
        try:
            probe.connect((address, port))
            probe.sendall(
                encode_message("version", encode_version_payload(address, port))
            )

            peer_version: int | None = None
            commands: list[str] = []
            deadline = time.monotonic() + timeout
            while time.monotonic() < deadline:
                command, payload = read_message(probe)
                commands.append(command)
                if command == "version":
                    if len(payload) >= 4:
                        peer_version = struct.unpack("<i", payload[:4])[0]
                    probe.sendall(encode_message("verack", b""))
                elif command == "ping":
                    probe.sendall(encode_message("pong", payload))
                elif command == "verack":
                    return address, addresses, peer_version, commands
        except (OSError, RuntimeError):
            pass
        finally:
            probe.close()
    raise RuntimeError(f"unable to complete signet handshake with {host}:{port}")


def signet_getheaders_validation(
    host: str, port: int, timeout: float, locator_hash_hex: str
) -> tuple[str, int, list[str]]:
    addresses = resolve_addresses(host, port)

    for address in addresses:
        family = socket.AF_INET6 if ":" in address else socket.AF_INET
        probe = socket.socket(family, socket.SOCK_STREAM)
        probe.settimeout(timeout)
        try:
            probe.connect((address, port))
            probe.sendall(
                encode_message("version", encode_version_payload(address, port))
            )

            handshake_deadline = time.monotonic() + timeout
            saw_version = False
            saw_verack = False
            while time.monotonic() < handshake_deadline and not (saw_version and saw_verack):
                command, payload = read_message(probe)
                if command == "version":
                    saw_version = True
                    probe.sendall(encode_message("verack", b""))
                elif command == "verack":
                    saw_verack = True
                elif command == "ping":
                    probe.sendall(encode_message("pong", payload))

            if not (saw_version and saw_verack):
                continue

            probe.sendall(
                encode_message(
                    "getheaders",
                    encode_getheaders_payload(
                        [wire_hash_from_hex(locator_hash_hex)]
                    ),
                )
            )

            headers_deadline = time.monotonic() + timeout
            while time.monotonic() < headers_deadline:
                command, payload = read_message(probe)
                if command == "ping":
                    probe.sendall(encode_message("pong", payload))
                    continue
                if command == "headers":
                    count, headers = parse_headers_payload(payload)
                    if count <= 0:
                        raise RuntimeError("received empty headers response")
                    return address, count, headers
        except (OSError, RuntimeError):
            pass
        finally:
            probe.close()

    raise RuntimeError(f"unable to fetch signet headers from {host}:{port}")


def fetch_block_by_hash(host: str, port: int, timeout: float, block_hash_hex: str) -> bytes:
    addresses = resolve_addresses(host, port)
    getdata_payload = compact_size(1) + struct.pack("<I", 2) + wire_hash_from_hex(block_hash_hex)

    for address in addresses:
        family = socket.AF_INET6 if ":" in address else socket.AF_INET
        probe = socket.socket(family, socket.SOCK_STREAM)
        probe.settimeout(timeout)
        try:
            probe.connect((address, port))
            probe.sendall(
                encode_message("version", encode_version_payload(address, port))
            )

            saw_version = False
            saw_verack = False
            deadline = time.monotonic() + timeout
            while time.monotonic() < deadline and not (saw_version and saw_verack):
                command, payload = read_message(probe)
                if command == "version":
                    saw_version = True
                    probe.sendall(encode_message("verack", b""))
                elif command == "verack":
                    saw_verack = True
                elif command == "ping":
                    probe.sendall(encode_message("pong", payload))

            if not (saw_version and saw_verack):
                continue

            probe.sendall(encode_message("getdata", getdata_payload))
            deadline = time.monotonic() + timeout
            while time.monotonic() < deadline:
                command, payload = read_message(probe)
                if command == "ping":
                    probe.sendall(encode_message("pong", payload))
                    continue
                if command == "block":
                    return payload
        except (OSError, RuntimeError):
            pass
        finally:
            probe.close()
    raise RuntimeError(f"unable to fetch block {block_hash_hex} from {host}:{port}")


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
    parser.add_argument(
        "--signet-locator-hash",
        default=SIGNET_GENESIS_HASH,
        help="Locator hash for the signet getheaders request",
    )
    parser.add_argument(
        "--witness-search-limit",
        type=int,
        default=64,
        help="Maximum number of headers to scan for a witness-bearing block",
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

    print("=== SIGNET HANDSHAKE VALIDATION ===")
    connected, addresses, peer_version, commands = signet_handshake_validation(
        args.signet_host, args.signet_port, args.tcp_timeout
    )
    print(f"SIGNET_RESOLVED {args.signet_host} {addresses}")
    print(f"SIGNET_VERSION_OK {connected} {args.signet_port} {peer_version}")
    print(f"SIGNET_COMMANDS {' '.join(commands)}")
    if "version" not in commands or "verack" not in commands:
        print("ERROR signet peer did not complete version/verack handshake", file=sys.stderr)
        return 1
    print("=== SUMMARY ===")
    print(f"MAINLINE_BOOTSTRAP_OK {bootstrap_ok}")
    print(f"MAINLINE_DISCOVERED_TOTAL {discovered_count}")
    print(f"SIGNET_HANDSHAKE_OK {connected}:{args.signet_port}")
    headers_peer, headers_count, headers = signet_getheaders_validation(
        args.signet_host, args.signet_port, args.tcp_timeout, args.signet_locator_hash
    )
    print("=== SIGNET GETHEADERS VALIDATION ===")
    print(f"SIGNET_HEADERS_PEER {headers_peer}:{args.signet_port}")
    print(f"SIGNET_HEADERS_COUNT {headers_count}")
    print(f"SIGNET_LOCATOR_HASH {args.signet_locator_hash}")
    print(f"SIGNET_FIRST_HEADER {headers[0]}")
    print(f"SIGNET_LAST_HEADER {headers[-1]}")

    print("=== SIGNET WITNESS BLOCK SEARCH ===")
    witness_found = False
    for block_hash in headers[: args.witness_search_limit]:
        try:
            block_payload = fetch_block_by_hash(
                args.signet_host, args.signet_port, args.tcp_timeout, block_hash
            )
            has_witness, txid, wtxid = parse_first_transaction_witness_flag(block_payload)
            if has_witness:
                print(f"SIGNET_WITNESS_BLOCK {block_hash}")
                print(f"SIGNET_WITNESS_TXID {txid}")
                print(f"SIGNET_WITNESS_WTXID {wtxid}")
                witness_found = True
                break
        except RuntimeError:
            continue

    if not witness_found:
        print("SIGNET_WITNESS_BLOCK not-found")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
