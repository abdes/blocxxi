#################
Blocxxi.Bitcoin
#################

`Blocxxi.Bitcoin` currently proves the adapter boundary with a
adapter-first Bitcoin interoperability surface:

- `HeaderSyncAdapter`
- `SignetLiveClient`
- `BitcoinCoreRpcAdapter`
- `RpcTransport`
- `ObservationBatch`
- `TransactionObservation`
- `BlockObservation`
- `NetworkHealthObservation`

The module now has two complementary concerns:

1. header-sync proof work through the public node API, preserving the local
   kernel contract
2. Bitcoin Core RPC ingestion for mempool + network-state access, normalized
   into platform-owned observation models

When a ``peer_hint`` is provided, the adapter also emits a bootstrap/discovery
hint through the existing node surface instead of inventing a Bitcoin-specific
node API. The current bounded sync flow still supports ordered header batches
with adapter-level continuity checks. The new RPC ingestion layer keeps the
analyzer thin by translating source-specific responses into reusable platform
observations before any rule logic runs. Full Bitcoin script validation remains
out of scope.
