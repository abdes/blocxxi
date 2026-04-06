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

The module intentionally excludes transport, storage-engine, and Bitcoin-only
headers.
