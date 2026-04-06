#################
Blocxxi.Storage
#################

`Blocxxi.Storage` provides reference implementations for the kernel storage
ports through exported factory functions:

- `MakeInMemoryBlockStore()`
- `MakeInMemorySnapshotStore()`
- `MakeFileBlockStore(...)`
- `MakeFileSnapshotStore(...)`

This keeps the exported boundary at the function level rather than the
concrete implementation-class level. The file-backed proof remains intentionally
simple and exists to validate the port boundary, not to define a long-term
database format.
