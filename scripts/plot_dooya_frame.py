#!/usr/bin/env python3
"""Draw one Dooya frame as an SVG timing diagram: docs/images/dooya-frame.svg.

Timings are the ones ESPHome's remote_base uses to encode/decode Dooya
(components/remote_base/dooya_protocol.cpp), so the picture is the signal the ESP
actually puts on air. Standard library only.

    python3 scripts/plot_dooya_frame.py
"""

from __future__ import annotations

from pathlib import Path

HEADER = (5000, 1500)  # mark, space in microseconds
BIT_ONE = (750, 350)
BIT_ZERO = (350, 750)

# Example frame - not a real remote id.
REMOTE_ID = 0xA1B2C3
CHANNEL = 0x21
BUTTON = 0x1  # UP
CHECK = 0x1  # equals BUTTON on a press frame

FIELDS = [
    ("remote id", REMOTE_ID, 24, "#4C72B0"),
    ("channel", CHANNEL, 8, "#DD8452"),
    ("button", BUTTON, 4, "#55A868"),
    ("check", CHECK, 4, "#C44E52"),
]

OUT = Path(__file__).resolve().parent.parent / "docs" / "images" / "dooya-frame.svg"

WIDTH = 1200
MARGIN = 40
TRACE_TOP, TRACE_HIGH = 110, 40  # y of the high level, height of a pulse
FONT = "font-family='Helvetica, Arial, sans-serif'"


def frame_bits() -> list[int]:
    bits = []
    for _, value, width, _ in FIELDS:
        bits += [(value >> i) & 1 for i in range(width - 1, -1, -1)]
    return bits


def timings() -> list[tuple[int, int]]:
    return [HEADER] + [BIT_ONE if b else BIT_ZERO for b in frame_bits()]


def waveform(
    pairs: list[tuple[int, int]], x0: float, scale: float, top: int, height: int
) -> str:
    low = top + height
    points = [(x0, low)]
    x = x0
    for mark, space in pairs:
        points += [(x, top), (x + mark * scale, top), (x + mark * scale, low)]
        x += (mark + space) * scale
        points.append((x, low))
    return " ".join(f"{px:.1f},{py}" for px, py in points)


def main() -> None:
    pairs = timings()
    total_us = sum(m + s for m, s in pairs)
    scale = (WIDTH - 2 * MARGIN) / total_us
    svg = [
        f"<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 {WIDTH} 420' {FONT}>",
        f"<rect width='{WIDTH}' height='420' fill='#ffffff'/>",
        f"<text x='{MARGIN}' y='32' font-size='20' font-weight='bold' fill='#222'>"
        "Dooya frame - OOK, PWM-coded, 40 bits MSB first</text>",
        f"<text x='{MARGIN}' y='56' font-size='14' fill='#555'>"
        f"id 0x{REMOTE_ID:06X} · channel 0x{CHANNEL:02X} · button {BUTTON} (UP) · "
        f"check {CHECK} · {total_us / 1000:.1f} ms on air, "
        "sent 6× with ~8 ms gaps</text>",
    ]

    # Field bands under the trace.
    x = MARGIN + sum(HEADER) * scale
    band_y = TRACE_TOP + TRACE_HIGH + 18
    svg.append(
        f"<rect x='{MARGIN}' y='{band_y}' width='{sum(HEADER) * scale:.1f}' "
        "height='26' fill='#888' opacity='0.25'/>"
        f"<text x='{MARGIN + sum(HEADER) * scale / 2:.1f}' y='{band_y + 18}' "
        "font-size='13' text-anchor='middle' fill='#333'>header</text>"
    )
    bit_iter = iter(pairs[1:])
    for name, value, width, color in FIELDS:
        span = sum(sum(next(bit_iter)) for _ in range(width)) * scale
        svg.append(
            f"<rect x='{x:.1f}' y='{band_y}' width='{span:.1f}' height='26' "
            f"fill='{color}' opacity='0.25'/>"
            f"<text x='{x + span / 2:.1f}' y='{band_y + 18}' font-size='13' "
            f"text-anchor='middle' fill='#222'>{name} = 0x{value:0{width // 4}X}</text>"
        )
        x += span

    svg.append(
        f"<polyline points='{waveform(pairs, MARGIN, scale, TRACE_TOP, TRACE_HIGH)}' "
        "fill='none' stroke='#222' stroke-width='1.6'/>"
    )

    # Legend: the three symbols. Bits share one scale so "1" and "0" compare directly;
    # the 6.5 ms header gets its own, or it would not fit its column.
    legend_top = 280
    column = 340
    for i, (label, pair, desc) in enumerate(
        [
            ("header", HEADER, "5000 µs mark + 1500 µs space (scaled to fit)"),
            ("bit 1", BIT_ONE, "750 µs mark + 350 µs space"),
            ("bit 0", BIT_ZERO, "350 µs mark + 750 µs space"),
        ]
    ):
        x0 = MARGIN + i * (column + 40)
        zoom = column / sum(pair if label == "header" else BIT_ONE)
        svg.append(
            f"<text x='{x0}' y='{legend_top - 14}' font-size='15' font-weight='bold' "
            f"fill='#222'>{label}</text>"
            f"<polyline points='{waveform([pair], x0, zoom, legend_top, 50)}' "
            "fill='none' stroke='#222' stroke-width='2'/>"
            f"<text x='{x0}' y='{legend_top + 80}' font-size='13' fill='#555'>"
            f"{desc}</text>"
        )

    svg.append(
        f"<text x='{MARGIN}' y='{legend_top + 115}' font-size='13' fill='#555'>"
        "A press repeats the frame while the button is held; the final frame carries "
        "check = 0xE (release) and is ignored by the bridge.</text>"
    )
    svg.append("</svg>")
    OUT.write_text("\n".join(svg) + "\n", encoding="utf-8")
    print(f"wrote {OUT}")


if __name__ == "__main__":
    main()
