# Architecture

## Layers

```mermaid
flowchart TB
    subgraph YAML["Configuration (YAML packages)"]
        DEV["base.yaml + network.yaml<br/>logger · web UI · Wi-Fi · OTA"]
        R433["radio-433.yaml"]
        R868["radio-868.yaml"]
        BLINDS["dooya-blinds.yaml<br/>(optional)"]
    end

    subgraph COMP["components/rf_bridge (ESPHome glue)"]
        BRIDGE["RfBridge<br/>RemoteReceiverListener + Component"]
        ENT["Entities<br/>select · buttons · text sensors · sensors"]
        ACT["Actions & triggers<br/>transmit_dooya · transmit_capture · on_dooya …"]
    end

    subgraph CORE["Core - plain C++, no ESPHome headers, unit-tested on the host"]
        FRAME["RfFrame<br/>describe · fingerprint · airtime"]
        DEDUP["FrameDeduplicator"]
        QUEUE["BoundedQueue"]
        SCHED["TxScheduler"]
        DUTY["DutyCycleLimiter"]
    end

    R433 & R868 --> BRIDGE
    BLINDS --> ACT
    BRIDGE --> ENT
    ACT --> BRIDGE
    BRIDGE --> FRAME & DEDUP & QUEUE & SCHED
    SCHED --> DUTY
    SCHED --> QUEUE
```

The split is deliberate: everything that has a *decision* in it (is this a repeat? may I
transmit now? how long is this on air?) lives in the core and is covered by unit tests
that run in milliseconds on any machine. The glue layer only adapts ESPHome types and
calls into the core.

## Receive path

```mermaid
flowchart LR
    RF(("RF")) --> CC["CC1101<br/>RX, async serial"]
    CC -->|GDO2 pulses| RX["remote_receiver<br/>(RMT)"]
    RX -->|RemoteReceiveData| DEC{"decode"}
    DEC -->|Dooya| PRESS{"press<br/>frame?"}
    DEC -->|RC-Switch| DUP
    DEC -->|unknown, ≥ N pulses| DUP
    DEC -->|noise| DROP1["dropped"]
    PRESS -->|release| DROP2["ignored"]
    PRESS -->|press| DUP{"repeat within<br/>dedup window?"}
    DUP -->|yes| DROP3["ignored"]
    DUP -->|no| OUT["log · history · entities<br/>on_frame / on_dooya"]
```

- **Decoders are tried in a fixed order** - Dooya first (strict, 40-bit with sync), then
  every RC-Switch protocol (at least 12 bits, to reject noise), then a raw capture fallback
  for anything long enough to be a real frame.
- **De-duplication** uses a sliding window keyed by a command fingerprint: a remote repeats
  a frame several times per press and keeps repeating while held, and the window slides
  with each repeat, so a long press is still one event. Raw frames are quantised to 100 µs
  before hashing, so timing jitter between repeats does not create new events.

## Transmit path

```mermaid
sequenceDiagram
    autonumber
    participant S as Scene / buttons / covers
    participant B as RfBridge
    participant Q as TxScheduler
    participant T as remote_transmitter
    participant C as CC1101

    S->>B: close living room, kitchen, bedroom (same instant)
    B->>Q: enqueue 3 encoded jobs
    Note over Q: FIFO, capacity-bounded<br/>full → rejected and logged
    loop every loop() iteration
        B->>Q: next(now)
        alt queue empty, gap not elapsed or duty-cycle budget spent
            Q-->>B: nothing
        else job ready
            Q-->>B: job
            B->>T: transmit (blocking)
            T->>C: on_transmit → begin_tx
            T->>C: N repeats on GDO0
            T->>C: on_complete → begin_rx
            B->>Q: mark_sent(end, airtime)
        end
    end
```

Why a queue at all? A radio can put exactly one signal on air at a time, and a receiver
needs a quiet gap to tell two commands apart. Without serialisation, a scene that closes
three blinds "at once" interleaves their bursts and some motors never see a valid frame.
Every producer therefore only *enqueues*; `loop()` is the single consumer.

Guarantees:

- **one transmission at a time per radio**, with a configurable gap measured from the real
  end of the previous transmission (`non_blocking: false`),
- **strict FIFO** - a job waiting for duty-cycle budget also holds the jobs behind it, so
  commands to the same device are never reordered,
- **bounded memory** - the queue rejects new jobs when full instead of growing,
- **regulatory airtime** - an optional sliding-window duty-cycle limit (ETSI EN 300 220:
  1 % in most of the 868 MHz SRD band).

## Radio state

```mermaid
stateDiagram-v2
    [*] --> RX: on_boot → cc1101.begin_rx
    RX --> TX: remote_transmitter.on_transmit → begin_tx
    TX --> RX: remote_transmitter.on_complete → begin_rx
```

The CC1101 is half-duplex: while one module transmits it hears nothing, which also means
the device never decodes its own transmissions on the same band.
