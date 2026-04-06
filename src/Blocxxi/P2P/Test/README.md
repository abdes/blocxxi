# P2P Test Module

This directory contains the **surviving P2P unit/integration-style tests** for
the modernized Mainline runtime path.

The old paper-style custom protocol stack has been retired from the live build,
so this test module is now centered on:

- channel/node shared utilities
- KRPC protocol correctness
- `MainlineDhtNode`
- `MainlineSession`
- libtorrent-assisted interoperability checks via the external harness

---

## What is tested here

Current in-repo P2P test target:

- `main.cpp`
- `krpc_test.cpp`
- `mainline_node_test.cpp`
- `mainline_session_test.cpp`
- `node_test.cpp`
- `channel_test.cpp`

These tests cover:

- compact-node / KRPC serialization semantics
- Mainline request/response behavior
- active client flows
- token / `announce_peer` rules
- `implied_port`
- `sample_infohashes`
- request timeout behavior
- channel and node utility behavior

---

## Why libtorrent is used

The project uses the installed Python `libtorrent` bindings as an **external
Mainline reference implementation** for interoperability checks.

The goal is not to replace the in-repo unit tests. The goal is to add an
outside observer that can:

1. encode/decode Mainline bencoded/KRPC traffic using a real client library
2. exercise the blocxxi Mainline runtime from outside the process
3. provide a second-stage public-network proof path

This gives two complementary layers:

- **in-repo unit tests** for deterministic behavior
- **libtorrent harness runs** for interoperability-oriented verification

---

## Harness location

The libtorrent-based harness lives at:

```text
src/Blocxxi/P2P/Test/mainline_dht_interop.py
```

It drives the compiled:

```text
out/build-ninja/bin/Debug/mainline-node
```

which is backed by the production:

- `MainlineDhtNode`
- `MainlineSession`

---

## Libtorrent testing approach

### 1. Local mode

Command:

```bash
python src/Blocxxi/P2P/Test/mainline_dht_interop.py --mode local --duration 6
```

Local mode launches a blocxxi `mainline-node` fixture on localhost and then
uses Python + `libtorrent` bencode helpers to validate Mainline RPC behavior
directly over UDP.

It currently verifies this local sequence:

1. `ping`
2. `find_node`
3. `get_peers`
4. `announce_peer`
5. `get_peers` confirmation
6. `sample_infohashes`

Why this is useful:

- direct protocol proof against the production-backed runtime
- no dependence on public-network volatility
- deterministic enough for routine regression checks

The harness records structured JSON results, including decoded replies and
whether the run produced a product-behavior-confirmed verdict.

Typical artifact:

```text
.omx/logs/kademlia-mainline-interop/local-runtime-final2.json
```

### 2. Public mode

Command:

```bash
python src/Blocxxi/P2P/Test/mainline_dht_interop.py --mode public --duration 8 \
  --bootstrap router.bittorrent.com:6881 \
  --bootstrap router.utorrent.com:6881 \
  --bootstrap dht.transmissionbt.com:6881
```

Public mode launches the blocxxi Mainline fixture and has it bootstrap against
real public Mainline routers.

The current public proof is intentionally narrower than local mode:

- it proves public bootstrap reachability
- it records which routers were attempted
- it records which bootstrap endpoints replied
- it records success / timeout classification

Typical artifact:

```text
.omx/logs/kademlia-mainline-interop/public-runtime-final2.json
```

### 3. Direct public CLI proof

For manual inspection, the production `mainline-node` example can also issue
direct public queries itself, for example:

```bash
./out/build-ninja/bin/Debug/mainline-node \
  --bind-address 0.0.0.0 \
  --duration-ms 6000 \
  --remote dht.transmissionbt.com:6881 \
  --query find_node \
  --target 9f61e4724fff275d7b1495ca13f94f24dcf27c69
```

And for longer-running discovery:

```bash
./out/build-ninja/bin/Debug/mainline-node \
  --bind-address 0.0.0.0 \
  --duration-ms 20000 \
  --remote dht.transmissionbt.com:6881 \
  --bootstrap router.bittorrent.com:6881 \
  --bootstrap router.utorrent.com:6881 \
  --query discover \
  --target 9f61e4724fff275d7b1495ca13f94f24dcf27c69
```

This mode now:

- resolves hostnames
- deduplicates discovered nodes
- seeds from multiple routers
- shows `DISCOVERY_TOTAL` growth over time

---

## What libtorrent is **not** doing here

The harness is **not** the canonical source of truth for everything.

It does **not** replace:

- unit tests for protocol internals
- deterministic in-process Mainline tests
- direct runtime assertions in `mainline_node_test.cpp`
- `MainlineSession` API tests

Also, the harness intentionally does **not** require every client-binding quirk
to cooperate. For example, if a particular `libtorrent` direct alert path is
flaky or binding-version-sensitive, the harness prefers direct protocol proof
from the blocxxi runtime itself over forcing success from a fragile side signal.

---

## Running the tests

### Build the relevant targets

```bash
cmake --build out/build-ninja --target Blocxxi.P2P.Unit.Tests mainline-node
```

### Run focused P2P tests

```bash
cd out/build-ninja
ctest -C Debug --output-on-failure -R 'MainlineDhtNodeTest|MainlineSessionTest|NodeTest|ChannelTest'
```

### Run the local harness

```bash
python src/Blocxxi/P2P/Test/mainline_dht_interop.py --mode local --duration 6
```

### Run the public harness

```bash
python src/Blocxxi/P2P/Test/mainline_dht_interop.py --mode public --duration 8 \
  --bootstrap router.bittorrent.com:6881 \
  --bootstrap router.utorrent.com:6881 \
  --bootstrap dht.transmissionbt.com:6881
```

---

## Test philosophy

For this module, the intended verification ladder is:

1. **unit tests first**
2. **local libtorrent-assisted protocol proof second**
3. **public bootstrap/discovery proof third**

That ordering keeps regressions easy to diagnose while still proving real
interop against the Mainline network.
