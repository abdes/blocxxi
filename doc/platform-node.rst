##############
Blocxxi.Node
##############

`Blocxxi.Node` is the public composition facade for third-party app and plugin
developers.

It exposes:

- lifecycle control (`Start`, `Stop`)
- event subscription
- plugin registration
- transaction submission
- pending-block commitment
- explicit discovery/adapter attachment hooks
- service registration plus bounded and continuous runtime/orchestration hooks
- checkpoint/poll/retry loops for platform-owned services

`Blocxxi.Node` stays facade-oriented: it composes the kernel, runtime services,
and optional adapters without becoming the home for low-level scheduler,
checkpoint, or DHT transport logic.
