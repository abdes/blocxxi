# mainline-node

`mainline-node` is the current **Mainline DHT example/runtime probe** for
blocxxi.

It is backed by the production Mainline path:

- `blocxxi::p2p::kademlia::MainlineDhtNode`
- `blocxxi::p2p::kademlia::MainlineSession`

It is intentionally used both as:

1. a small node that can **answer Mainline KRPC requests**
2. a small client that can **issue Mainline KRPC requests**
3. a proof tool for **public Mainline interoperability**

The older paper-style custom protocol stack has been retired from the live
build. This example is the canonical P2P example path now.

---

## Build

From the repository root:

```bash
cmake --build out/build-ninja --target mainline-node
```

The executable is produced at:

```text
out/build-ninja/bin/Debug/mainline-node
```

---

## Basic server mode

Run a node that listens for Mainline KRPC traffic:

```bash
./out/build-ninja/bin/Debug/mainline-node \
  --bind-address 127.0.0.1 \
  --duration-ms 15000
```

Typical startup output:

```text
READY 127.0.0.1:NNNNN <node-id-hex>
```

That line shows:

- bound address/port
- generated node ID

When the node receives requests it prints lines such as:

```text
QUERY ping 127.0.0.1:54321
QUERY find_node 127.0.0.1:54321
```

---

## Client query mode

`mainline-node` can also send active client queries to a remote Mainline node.

### Ping

```bash
./out/build-ninja/bin/Debug/mainline-node \
  --bind-address 0.0.0.0 \
  --duration-ms 6000 \
  --remote dht.transmissionbt.com:6881 \
  --query ping
```

### Find node

```bash
./out/build-ninja/bin/Debug/mainline-node \
  --bind-address 0.0.0.0 \
  --duration-ms 6000 \
  --remote dht.transmissionbt.com:6881 \
  --query find_node \
  --target 9f61e4724fff275d7b1495ca13f94f24dcf27c69
```

### Get peers

```bash
./out/build-ninja/bin/Debug/mainline-node \
  --bind-address 0.0.0.0 \
  --duration-ms 6000 \
  --remote dht.transmissionbt.com:6881 \
  --query get_peers \
  --target 9f61e4724fff275d7b1495ca13f94f24dcf27c69
```

### Sample infohashes

```bash
./out/build-ninja/bin/Debug/mainline-node \
  --bind-address 0.0.0.0 \
  --duration-ms 6000 \
  --remote dht.transmissionbt.com:6881 \
  --query sample_infohashes \
  --target 9f61e4724fff275d7b1495ca13f94f24dcf27c69
```

### Announce peer

Requires a token obtained from a prior `get_peers` response:

```bash
./out/build-ninja/bin/Debug/mainline-node \
  --bind-address 0.0.0.0 \
  --duration-ms 6000 \
  --remote dht.transmissionbt.com:6881 \
  --query announce_peer \
  --target <info-hash> \
  --token <token-from-get-peers> \
  --announce-port 49000
```

If you want the announced port to come from the sender socket instead of the
explicit `--announce-port`, add:

```bash
--implied-port
```

---

## Discovery mode

The example also supports a longer-running recursive discovery mode:

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

This mode:

- starts from the explicit `--remote` node
- also seeds the crawl with any `--bootstrap` routers you provide
- recursively follows returned compact-node lists via `find_node`
- deduplicates endpoints before re-queueing
- prints only **unique discovered nodes**
- prints a running total

Typical output:

```text
READY 0.0.0.0:47488 703c004343542b249cb22ceef8caa805104d3425
DISCOVERED_NODE 112.87.174.167:6882 c1468a3a5ee9af5f4abc766169872303e1972f98
DISCOVERY_TOTAL 1
DISCOVERED_NODE 112.87.174.50:6882 9f19f6c0fde980dd07cfde93251b301fa329a85e
DISCOVERY_TOTAL 2
...
DISCOVERY_TOTAL 667
```

The crawl is bounded only by the configured duration.

---

## Output conventions

### Common

- `READY <ip:port> <node-id>` — node is listening

### Server-side

- `QUERY <method> <sender-ip:port>` — request received
- `BOOTSTRAP_OK <bootstrap-endpoint>` — bootstrap reply received

### Client-side

- `CLIENT_RESPONSE type=<response|error|query> tx=<transaction-id>`
- `CLIENT_TOKEN <token>`
- `CLIENT_PEER <ip:port>`
- `CLIENT_NODE <ip:port> <node-id>`
- `CLIENT_NODES_BYTES <N>`
- `CLIENT_SAMPLES <N>`
- `CLIENT_ERROR <message>`

### Discovery mode

- `DISCOVERED_NODE <ip:port> <node-id>`
- `DISCOVERY_TOTAL <count>`

---

## Dedupe behavior

By default the example deduplicates:

- returned `values` peers in displayed output
- returned `nodes` in displayed output
- queued endpoints during discovery

If you want raw response printing without display dedupe, use:

```bash
--no-dedupe
```

---

## Hostname support

`--remote` and `--bootstrap` accept hostnames, not just numeric IP addresses.

Examples:

```bash
--remote dht.transmissionbt.com:6881
--bootstrap router.bittorrent.com:6881
--bootstrap router.utorrent.com:6881
```

The example resolves them before sending queries.

---

## Verification / proof workflow

### Local proof

Use the harness:

```bash
python tools/mainline_dht_interop.py --mode local --duration 6
```

This validates the local Mainline path for:

- `ping`
- `find_node`
- `get_peers`
- `announce_peer`
- `get_peers` confirmation
- `sample_infohashes`

### Public proof

```bash
python tools/mainline_dht_interop.py --mode public --duration 8 \
  --bootstrap router.bittorrent.com:6881 \
  --bootstrap router.utorrent.com:6881 \
  --bootstrap dht.transmissionbt.com:6881
```

This proves public bootstrap reachability through real Mainline routers.

### Direct public query proof

You can also issue a direct public `find_node` query from the example itself:

```bash
./out/build-ninja/bin/Debug/mainline-node \
  --bind-address 0.0.0.0 \
  --duration-ms 6000 \
  --remote dht.transmissionbt.com:6881 \
  --query find_node \
  --target 9f61e4724fff275d7b1495ca13f94f24dcf27c69
```

---

## Current scope

The example/runtime currently demonstrates a verified Mainline-compatible path
for the implemented RPC set and public bootstrap/discovery proofs.

It is **not** claiming full persistent routed-DHT parity in the stronger sense
of a full long-lived routing-table-backed production node yet.

It is the canonical modernized runtime path for blocxxi’s P2P example code.
