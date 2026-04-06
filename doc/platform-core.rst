##############
Blocxxi.Core
##############

`Blocxxi.Core` holds the developer-first primitives that external applications
see first:

- `ChainConfig`
- `Transaction`
- `Block`
- `ChainSnapshot`
- `ChainEvent`
- `Plugin`
- `Status` / `StatusCode`
- `EventEnvelope`
- `SignedEventRecord`

The module intentionally excludes transport, storage-engine, Bitcoin-specific
wire adapters, and DHT transport internals. It is the neutral home for signed
event identity/modeling used by both the Bitcoin adapter and future consumers.
