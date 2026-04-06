#############################
Blocxxi platform module guide
#############################

Blocxxi now exposes a reusable blockchain platform surface in addition to the
existing Kademlia networking stack.

Platform modules
================

**Blocxxi.Core**
  Developer-facing primitives: chain configuration, blocks, transactions,
  chain snapshots, status codes, events, and plugin hooks.

**Blocxxi.Chain**
  The minimal local blockchain kernel: bootstrap, pending transaction
  admission, block validation, and chain progression.

**Blocxxi.Storage**
  Storage ports plus in-memory and file-backed reference implementations.

**Blocxxi.Node**
  Public composition facade for third-party app/plugin developers. A node can
  run without P2P and exposes lifecycle, event subscription, and submission
  APIs without leaking storage or Bitcoin adapter types.

Examples
========

The platform-level examples live under ``src/Blocxxi/Examples/``:

- ``hello-plugin`` — tiny external-developer plugin story
- ``local-custom-chain`` — custom local chain proof with no DHT dependency

Deferred work
=============

The current platform intentionally defers mining, VM/smart contracts,
Bitcoin-specific adapters, full Bitcoin script validation, and RPC/REST
surfaces.
