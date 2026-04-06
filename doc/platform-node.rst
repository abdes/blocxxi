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

A node can run entirely in-process without DHT participation.
