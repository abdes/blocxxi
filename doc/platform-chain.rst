###############
Blocxxi.Chain
###############

`Blocxxi.Chain` is the minimal local blockchain kernel.

It owns:

- bootstrap/genesis creation
- pending transaction admission
- block validation for the active local head
- chain progression through a stable storage-port contract

This keeps consensus- and chain-rule logic above Core without coupling it to
transport or Bitcoin-specific adapters.
