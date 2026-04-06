#################
Blocxxi.Bitcoin
#################

`Blocxxi.Bitcoin` currently proves the adapter boundary with a
header-sync-oriented surface:

- `HeaderSyncAdapter`
- `Network`
- `Header`

The adapter imports Bitcoin header observations through the public node API and
preserves the local kernel contract. Full Bitcoin script validation remains
out of scope.
