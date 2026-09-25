# ZombieVerter CAN Mappings

The dash reads three values from a custom ZombieVerter CAN TX frame on
**ID 0x31A (794 decimal)**. ZombieVerter only transmits what is explicitly
mapped, so all three mappings below must exist. If one is deleted, that
value silently stops updating on the dash (everything else keeps working).

## The three required mappings

| # | Parameter (spot value) | CAN ID | Position (start bit) | Length (bits) | Gain | Offset |
|---|------------------------|--------|----------------------|---------------|------|--------|
| 1 | `drivedir` | 794 (0x31A) | 0  | 8  | 1  | 0 |
| 2 | `opmode`   | 794 (0x31A) | 8  | 8  | 1  | 0 |
| 3 | `uaux`     | 794 (0x31A) | 16 | 16 | 10 | 0 |

As terminal/console commands (format: `can tx <param> <id> <position> <length> <gain> <offset>`):

```
can tx drivedir 794 0 8 1 0
can tx opmode   794 8 8 1 0
can tx uaux     794 16 16 10 0
```

## Resulting frame layout

| Byte(s) | Content | Decoding |
|---------|---------|----------|
| 0 | Drive direction | 0x01 forward, 0x00 neutral, 0xFF reverse |
| 1 | Op mode | 0 off, 1 run, 2 precharge, 3 precharge fail, 4 charging |
| 2–3 | 12V battery voltage | little-endian, 0.1 V/bit (e.g. `84 00` = 0x0084 = 132 → 13.2 V) |
| 4–7 | unused | 0 |

The `uaux` gain of 10 matters: it gives 0.1 V resolution so the dash can
tell 11.4 V from 11.9 V (the gauge redline starts at 11.5 V). The ESP32
decodes bytes 2–3 by multiplying by 0.1 — if you ever change the gain, the
multiplier in `main/core/leaf_can.c` (0x31A case) must change to match.

## Adding a mapping in the ZombieVerter web UI

1. Power the ZombieVerter and connect to its WiFi network.
2. Open the ZombieVerter web interface in a browser.
3. Go to the **CAN mapping** section.
4. Add a new **TX** mapping and fill in the fields from the table above:
   parameter name, CAN ID **794**, position, length, gain (offset 0).
5. Repeat for any missing mapping — the existing entries are listed on the
   same page; compare them against the table.
6. **Save to flash** so the mappings survive a power cycle (on older
   firmware run `save` / `can save` from the console). Then power-cycle and
   confirm all three mappings are still listed.

## Verifying with the dashboard

Swipe left on the dash to the **Service / Diagnostics** screen and check the
**"ZombieVerter 0x31A raw"** row, which shows the live frame in hex:

```
01 01 84 00 00 00 00 00
 |  |  |___|
 |  |  uaux = 0x0084 = 132 → 13.2 V
 |  opmode = 1 (Run)
 drivedir = 1 (Forward)
```

- Bytes 2–3 stuck at `00 00` → the `uaux` mapping is missing.
- `no 0x31A frames received` → none of the mappings exist (or CAN is down).
- Plausible hex but a wrong gauge reading → gain or byte order mismatch;
  note the hex and adjust.
