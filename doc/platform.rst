#############################
Blocxxi platform module guide
#############################

Blocxxi now exposes a reusable blockchain platform surface in addition to the
existing Kademlia networking stack.

Platform modules
================

**Blocxxi.Core**
  Developer-facing primitives: chain configuration, blocks, transactions,
  chain snapshots, status codes, events, plugin hooks, and signed event
  envelope modeling.

**Blocxxi.Chain**
  The minimal local blockchain kernel: bootstrap, pending transaction
  admission, block validation, and chain progression.

**Blocxxi.Storage**
  Storage ports plus in-memory and file-backed reference implementations.

**Blocxxi.Node**
  Public composition facade for third-party app/plugin developers. A node can
  run without P2P and exposes lifecycle, event subscription, submission, and
  runtime/service orchestration APIs without leaking storage, Bitcoin adapter,
  or DHT wire types.

**Blocxxi.Bitcoin**
  Adapter-first Bitcoin interoperability surface. The current platform now
  includes Bitcoin Core RPC ingestion for mempool + network-state access, while
  preserving the existing header-sync proof path and peer/bootstrap hints
  routed through the public node API.

**Blocxxi.P2P**
  Optional Mainline/Kademlia transport plus deterministic publish/query
  plumbing for signed event records, including exact-key lookups and
  Mainline-backed peer discovery. The analyzer app never talks to Kademlia
  directly.

Examples
========

The platform-level examples live under ``src/Blocxxi/Examples/``:

- ``hello-plugin`` — tiny external-developer plugin story
- ``local-custom-chain`` — custom local chain proof with no DHT dependency
- ``bitcoin-observer`` — Bitcoin-facing adapter sample using header imports
- ``bitcoin-mempool-analyzer`` — thin analyzer proof with rule/taxonomy only;
  runs continuously by default through the Node runtime loop, prints published
  events, uses Bitcoin Core RPC by default, and supports ``--scripted
  --oneshot`` for bounded proof runs
- ``bitcoin-event-reader`` — second consumer proof that reuses the same SDK
  and queries published records by deterministic key

Deferred work
=============

The current platform intentionally defers mining, VM/smart contracts, full
Bitcoin script validation, and RPC/REST server surfaces. Those remain out of
scope for the reusable SDK contract, even though Bitcoin Core RPC ingestion is
now part of the platform surface.
