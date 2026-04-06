#################
Blocxxi.Bitcoin
#################

`Blocxxi.Bitcoin` currently proves the adapter boundary with a
header-sync-oriented surface:

- `HeaderSyncAdapter`
- `SignetLiveClient`
- `Network`
- `Header`

The adapter imports Bitcoin header observations through the public node API and
preserves the local kernel contract. When a ``peer_hint`` is provided, the
adapter also emits a bootstrap/discovery hint through the existing node
surface instead of inventing a Bitcoin-specific node API. The current bounded
sync flow supports ordered header batches with adapter-level continuity checks.
For live-network proof work, `SignetLiveClient` can complete a bounded signet
handshake and `getheaders` exchange without promoting Bitcoin session logic
into the kernel. Full Bitcoin script validation remains out of scope.
