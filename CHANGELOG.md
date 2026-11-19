# Changelog

All notable changes to this project are documented here.
The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and the project
uses [Semantic Versioning](https://semver.org/).

## [1.0.0] - 2026-11-19

### Added

- `rf_bridge` ESPHome external component: Dooya + RC-Switch decoding, raw capture of
  unknown frames, sliding-window repeat suppression, capture history with replay.
- Serialised transmit queue per radio: strict FIFO, inter-frame gap measured from the end
  of the previous transmission, bounded capacity, optional ETSI duty-cycle limit.
- Entities: capture slot select, transmit selected / last / clear buttons, last frame and
  selected capture text sensors, queue depth / frames received / duty-cycle sensors.
- Actions `rf_bridge.transmit_dooya`, `transmit_rc_switch`, `transmit_capture`,
  `clear_captures`; triggers `on_frame`, `on_dooya`.
- Two-radio configuration for the Ebyte E07-M1101D-SMA (433 MHz) and E07-900M10S
  (868 MHz), standalone web UI and logs, optional Home Assistant API.
- Dooya blinds package: time-based covers kept in sync with the physical remotes.
- Prebuilt image and browser installer (ESP Web Tools) on GitHub Pages; Wi-Fi provisioned
  over USB (Improv Serial).
- Host unit tests (GoogleTest, ASan + UBSan) with a published coverage report.
- Docs: WireViz wiring diagrams, architecture (Mermaid), Dooya reverse-engineering
  write-up.

[1.0.0]: https://github.com/lukasz-malicki-wrx/esp32-rf-sniffer/releases/tag/v1.0.0
