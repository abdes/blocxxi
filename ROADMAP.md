# Blocxxi Roadmap

## 1. Positioning

Blocxxi is a C++ platform for building persistent, long-running intelligence applications.

Its job is not merely to wrap a protocol or an RPC API. Its job is to provide the systems layer for applications that must:
- ingest live data from external networks or services
- persist raw observations and derived state
- run continuously with retries, checkpoints, and recovery
- derive structured events from that state
- hand off those events to downstream consumers such as AI agents, dashboards, mobile apps, or messaging/automation systems

Bitcoin is the first serious domain Blocxxi targets, but the platform direction is broader than Bitcoin-specific tooling.

### Blocxxi is not
- just a Bitcoin Core RPC wrapper
- just a rule engine
- just a DHT publisher
- just an AI summarizer

### Blocxxi is
- an ingestion platform
- a runtime/orchestration platform
- a persistence platform
- an event and insight contract platform
- a consumer-facing SDK for intelligence applications

## 2. Vision

The long-term vision for Blocxxi is:

> a high-performance, persistent intelligence application platform that can ingest live network data, derive structured events, synthesize higher-level insights, and distribute those insights to humans, apps, and agents.

That means Blocxxi should support applications that:
- collect raw data continuously
- persist enough history and state to reason over time
- produce machine-readable events
- optionally generate AI-assisted insights on top of those events
- publish those insights to multiple sinks:
  - local stores
  - local consumer SDKs
  - dashboards
  - mobile clients
  - agent workflows
  - decentralized publication layers such as a DHT

## 3. Core Product Principle

Blocxxi platform logic should own the reusable hard parts.

Applications built on Blocxxi should stay thin and own only their domain-specific logic.

### Platform-owned concerns
- data-source adapters
- runtime orchestration
- persistence and checkpointing
- normalized observation models
- structured event records
- signing and identity
- publication/query contracts
- consumer/query SDK helpers
- optional insight pipeline plumbing

### App-owned concerns
- rule definitions
- domain taxonomy
- app-specific thresholds and priorities
- display/notification behavior
- app-specific AI prompts or business policy

## 4. Platform Layers

Blocxxi should evolve around the following layers.

### Layer 1 — Ingestion
Responsible for acquiring raw observations from external systems.

Examples:
- Bitcoin Core RPC
- future native Bitcoin P2P ingestion
- future replay or offline data sources

### Layer 2 — Persistence
Responsible for durable storage of:
- raw observations
- rolling windows
- event history
- dedup state
- service checkpoints
- AI context state when applicable

This is a foundational platform capability, not a later nice-to-have.

### Layer 3 — Runtime / Orchestration
Responsible for:
- long-running services
- retries/backoff
- service scheduling
- health reporting
- graceful shutdown
- restart/resume behavior

### Layer 4 — Event Contracts
Responsible for:
- normalized events
- signatures and provenance
- deterministic keys
- severity/confidence/revision semantics
- cross-app consumability

### Layer 5 — Insight Contracts
Responsible for:
- higher-level synthesized insights derived from observations and events
- human-readable summaries
- machine-consumable fields
- provenance back to source observations/events

### Layer 6 — Publication / Consumption
Responsible for delivering events and insights to:
- local consumers
- dashboards
- mobile apps
- agent frameworks
- webhooks / messaging systems
- optional decentralized backends such as DHT publication

## 5. Why Blocxxi Matters

The major value of Blocxxi is not that it talks to Bitcoin.

The major value is that it helps build long-running, high-performance, stateful intelligence applications that can later extend to multiple consumer surfaces and even decentralized publication models.

That is what makes Blocxxi a platform instead of a single-purpose integration.

## 6. Current State

Blocxxi already demonstrates the early platform direction through:
- `Blocxxi.Core` for signed event records and developer-facing primitives
- `Blocxxi.Bitcoin` for Bitcoin Core RPC ingestion and normalized observations
- `Blocxxi.Node` for runtime services and checkpoint-aware orchestration
- `Blocxxi.P2P` for deterministic publish/query contracts over signed events
- thin example consumers that prove the platform/app separation

The current examples prove the architecture, but they do not yet represent the final operational shape of a mature intelligence platform.

## 7. Application Strategy

Blocxxi apps should not be massive monoliths.

A good Blocxxi app should be a thin consumer of the platform.

### Example app categories
- network analyzers
- alerting services
- dashboard backends
- mobile notification bridges
- AI insight agents
- message-routing bridges (for example to agent systems like OpenClaw)
- research or replay tools

### Example Bitcoin-first applications
- mempool and block analyzer
- fee market monitor
- large transaction watcher
- reorg and chain health notifier
- AI-generated network summary agent

## 8. Roadmap

## Phase 0 — Platform Proofs
**Status:** already underway

Goals:
- prove the platform/app split
- prove signed event contracts
- prove runtime services
- prove a first real ingestion backend
- prove more than one consumer can exist

Representative outputs:
- Bitcoin Core RPC ingestion adapter
- signed event records
- runtime services in `Blocxxi.Node`
- deterministic publish/query contracts
- thin analyzer example
- second consumer example

## Phase 1 — Local-First Intelligence Platform

Goals:
- make Blocxxi a credible single-node intelligence platform before attempting full decentralization
- add persistent local state and local consumer access

Required work:
1. persistent analyzer state
2. local event store
3. rolling windows and feature store
4. event dedup/suppression
5. local consumer query API
6. operator-grade console and JSON output

Why this phase matters:
A local-first platform proves the hard systems design without prematurely forcing distributed trust, spam handling, or multi-writer DHT complexity.

## Phase 2 — Insight Pipeline

Goals:
- separate observations, events, and AI-generated insights into explicit stages
- make AI an upper-layer consumer of Blocxxi contracts, not a replacement for them

Required work:
1. observation model persistence
2. feature extraction and rolling baselines
3. rule-pack interface
4. insight contract schema
5. provenance links from insight -> event -> observation
6. severity/confidence/revision semantics

Why this phase matters:
Other apps may want raw observations, others may want events, and others may want concise AI insights. Blocxxi should support all three.

## Phase 3 — Consumer SDK and App Surfaces

Goals:
- make published data easy to consume by external apps and agents

Required work:
1. query-by-type / query-by-identifier / query-by-window APIs
2. verification helpers for signatures and provenance
3. local subscription/polling helpers
4. dashboard/mobile/webhook adapters
5. agent integration surfaces

Why this phase matters:
Publishing intelligence is not enough. The platform must make that intelligence usable.

## Phase 4 — Decentralized Publication

Goals:
- add a real network-backed distributed publication layer
- preserve the same event contract while changing the backend

Required work:
1. network-backed DHT storage/publish backend
2. republish and availability rules
3. multi-node retrieval
4. publisher identity and trust filtering
5. conflict and supersession handling

Why this phase matters:
This is where Blocxxi evolves from local-first intelligence infrastructure into distributed intelligence infrastructure.

## Phase 5 — Multi-App Platform

Goals:
- support a family of applications beyond the first Bitcoin analyzer
- make Blocxxi clearly reusable and extensible

Required work:
1. additional rule-pack applications
2. alerting/notification apps
3. dashboard backends
4. agent integrations
5. stronger SDK docs and templates

## 9. Bitcoin-Specific Roadmap

### Near term
- stabilize Bitcoin Core RPC ingestion
- improve runtime visibility and failure reporting
- persist analyzer state
- add better rule quality
- improve event dedup and suppression

### Medium term
- add richer block/mempool/network features
- add AI-assisted insight generation on top of persisted event streams
- expose local consumer APIs and dashboard backends

### Long term
- add alternative Bitcoin ingestion backends beyond Bitcoin Core RPC
- add distributed publication and multi-publisher consumption

## 10. Persistence Strategy

Persistence should be treated as a core part of the Blocxxi vision.

Blocxxi should persist:
- raw observations
- normalized features
- event history
- checkpoint state
- dedup state
- service health/recovery state
- optional AI context state

Without persistence, Blocxxi remains too demo-shaped.

With persistence, Blocxxi becomes a true systems platform for intelligence workloads.

## 11. DHT Strategy

Blocxxi should be designed so that a DHT is a backend, not the only identity of the system.

### Near-term approach
- local-first publication/query interfaces
- transport/storage abstraction that can later target a DHT

### Long-term approach
- open-write, signed-record publication
- selective trust at the consumer layer
- publisher-aware aggregation and filtering

This allows Blocxxi to grow into decentralized publication without forcing that complexity before the local platform is mature.

## 12. Guidance for Apps Built on Blocxxi

A Blocxxi app should ideally:
- consume normalized observations or events from the platform
- define its own rules, taxonomy, and output behavior
- avoid implementing transport, persistence, signing, or orchestration itself
- remain replaceable without changing Blocxxi platform code

If an app starts accumulating infrastructure logic, that is a signal that the platform is missing a reusable capability.

## 13. Short Positioning Statements

### Internal positioning
Blocxxi is the systems layer for durable, high-performance intelligence applications.

### External positioning
Blocxxi is a C++ platform for building persistent intelligence applications that ingest live network data, derive structured events, and produce machine- and human-consumable insights.

### Bitcoin-first positioning
Blocxxi is a Bitcoin intelligence platform SDK whose current real-world backend is Bitcoin Core RPC, with a roadmap toward richer persistence, insights, and optional decentralized distribution.

## 14. What Success Looks Like

Blocxxi will be successful when:
- multiple thin apps exist on top of the same platform contracts
- persistence is first-class
- observations, events, and insights are separate and reusable layers
- local consumers can query and react to published intelligence easily
- AI can consume Blocxxi event streams to produce useful higher-level insights
- decentralized publication becomes an extension of the platform, not its fragile foundation

## 15. Immediate Next Priorities

1. persistent local analyzer state
2. local event store and local consumer query path
3. dedup/suppression and event revision semantics
4. observation -> event -> insight pipeline separation
5. consumer SDK for dashboards, mobile apps, and agents

These steps strengthen the platform far more than prematurely optimizing for a public distributed intelligence network.
