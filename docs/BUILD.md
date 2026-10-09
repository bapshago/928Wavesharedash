# Build guide: 928 EV round-screen dash

How to build the pair of round dash screens for the Porsche 928 EV, from ordering
parts to a working dash in the car. It covers both units: a **left** screen
(speed, drive direction, odometer, state of charge) and a **right** screen
(current, voltages, temperatures, status). Both run the same firmware.

Detailed reference lives elsewhere and is linked rather than repeated:

- [`hardware/mechanical/README.md`](../hardware/mechanical/README.md): cut files,
  bend diagrams, shroud, assembly stack, build photos
- [`README.md`](../README.md): wiring table, CAN messages, firmware options
- [`docs/ZombieVerter_CAN_Mappings.md`](ZombieVerter_CAN_Mappings.md): the
  ZombieVerter CAN mappings the dash needs

> **Open items** are marked like this. They're details still to be confirmed or
> photographed.

---

## 1. Parts list

Quantities are for the pair (two screens).

### Electronics

| Part | Qty | Notes |
|---|---|---|
| Waveshare ESP32-P4-WIFI6-Touch-LCD-3.4C | 2 | 3.4" round 800×800 touch display. Check the chip revision (see [Firmware](#5-firmware)) |
| SN65HVD230 CAN transceiver breakout | 2 | Pins: 3V3, GND, CTX, CRX, CANH, CANL. Plugs onto the Waveshare 40-pin header |
| LM2596 12 V → 5 V buck converter module (with voltage display) | 2 | One per screen. Rated 2.1 A each |
| USB-C cable | 2 | Cut and soldered to the buck converter output |
| Inline fuse holder and **4 A** fuse | 1 | On the 12 V feed, before the buck converters |
| Wire for 12 V power and ground | about 5 ft | Run from the CE panel. Length depends on your car |
| Wire for CAN-H / CAN-L | about 6 ft | Twisted pair recommended. Length depends on your car |

### Cut and formed parts

See the parts table in the
[mechanical README](../hardware/mechanical/README.md) for files and drawings.

| Part | Qty | Material |
|---|---|---|
| Main instrument faceplate | 1 | 16 ga (1/16") cold rolled steel, SendCutSend |
| Waveshare mounting brackets | 8 | 16 ga (1/16") cold rolled steel, SendCutSend |
| Buck converter bracket (6 round holes) | 2 | 1/16" ABS, laser cut |
| CAN bus wire bracket (2 round holes) | 2 | 1/16" ABS, laser cut |
| Shroud | 1 | 18 ga cold rolled steel strip, 3" × 31", hand formed |

### Hardware

| Item | Used for |
|---|---|
| M4 × 5 mm screws | Waveshare mounting brackets into the screens (through the ABS brackets) |
| M4 bolts, flat washers, lock washers, nuts | Waveshare mounting brackets to the faceplate |
| M2.5 × 5 mm standoffs, screws and nuts | Buck converters to their ABS brackets |
| M6 × 12 mm flange bolts (2) | Faceplate tabs to the pod, if not using the original Porsche brackets |
| Small zip ties | Wiring, through the notches in the brackets |

### Consumables

- Posterboard, for the shroud template
- Epoxy primer and matte black paint
- Solder, heat shrink

### Tools

- Sheet metal brake or vise, for the 90° bends
- Welder, for tacking and stitch welding the shroud
- Laser cutter for the ABS brackets, or a service that cuts ABS
- Soldering iron
- Multimeter
- A computer with ESP-IDF and a USB-C data cable, for flashing

---

## 2. Ordering and cutting

1. **Steel parts:** order the faceplate (qty 1) and the Waveshare mounting
   brackets (qty 8) from SendCutSend in 16 ga cold rolled steel. See
   [ordering](../hardware/mechanical/README.md#ordering-the-steel-parts-from-sendcutsend)
   for the upload checks.
2. **ABS parts:** laser cut 2 buck converter brackets and 2 CAN bus wire brackets
   from 1/16" ABS.
3. **Shroud strip:** cut a 3" × 31" strip of 18 ga cold rolled steel. Don't trim it
   yet; that happens after the template (step 3.3).

---

## 3. Metalwork

1. **Bend the faceplate tabs:** 90°, 20 mm down from the top of each end tab.
   See the [bend diagram](../hardware/mechanical/README.md#bending-the-main-instrument-faceplate).
2. **Bend the 8 Waveshare mounting brackets:** two 90° bends making a 5 mm jog.
   See the [bend diagram](../hardware/mechanical/README.md#bending-the-waveshare-mounting-brackets).
3. **Make the shroud:** test fit the faceplate in the pod, measure, make a
   posterboard template, then form, close, tack, refit and stitch weld. Follow the
   [shroud steps](../hardware/mechanical/README.md#shroud) in order. Measure your
   own pod: the first car's 10–33 mm won't necessarily fit yours.
4. **Paint:** prep, epoxy primer, then a matte black top coat on the faceplate,
   shroud and brackets. Matte black keeps reflections off the screens.

---

## 4. Electronics prep

Do this for each of the two units.

### CAN transceiver

1. Solder header pins to the SN65HVD230 breakout so it plugs onto the Waveshare
   40-pin header. CTX must go to GPIO35 and CRX to GPIO34 (see the
   [wiring table](../README.md#hardware)). If your header lands on other pins,
   change them in menuconfig to match ([Firmware](#5-firmware)).
2. **Termination:** the breakout has a 120 Ω terminating resistor (R2). The EV CAN
   bus is already terminated at both ends, so **remove R2** unless this dash is
   physically at one end of the bus.
3. Solder the CAN-H and CAN-L leads to the breakout's CANH and CANL pads.

> **Open item:** photo of the transceiver with header pins fitted and plugged in.

### Buck converter

1. Solder the USB-C cable to the buck converter's **output** terminals (OUT+ / OUT−).
2. Connect a 12 V bench supply to the **input**, then adjust the trim pot until
   the output reads **5.0 V**. Confirm with a multimeter, not just the module's
   display.
3. **Check the polarity at the USB-C end** with the multimeter (VBUS +5 V, GND)
   **before** plugging it into the Waveshare board.

> **Open item:** photo of the USB-C lead soldered to the buck converter.

---

## 5. Firmware

Full details are in the main README under
[Building and flashing](../README.md#building-and-flashing). The short version:

1. Install ESP-IDF **5.5 or newer**.
2. Check the chip revision on each board. The repo defaults are for **ESP32-P4
   rev v1.x**. For a rev v3.x chip, use the `sdkconfig.defaults.rev3_x` build in
   the README.
3. Run `idf.py menuconfig` → **928 EV Dash** and set:
   - **Screen side:** *Left* on one unit, *Right* on the other.
   - **Test mode:** *off* for the car. *On* is useful for a bench check without CAN.
   - **CAN TX / RX GPIO:** 35 / 34, unless your transceiver is on other pins.
4. Build and flash each unit:
   `idf.py build` then `idf.py -p <port> flash monitor`.
5. **Odometer:** before the first boot in the car, set `DASH_ODO_INITIAL_MILES`
   in `main/core/dash_calc.h` to the car's real mileage. It seeds once, on the
   first boot.

Because the side is a build setting, flash the left and right units one at a
time and label them.

---

## 6. Assembly

Each screen stacks, front to back:

1. **Main instrument faceplate**
2. **Waveshare screen**, face forward through its opening
3. **ABS brackets:** buck converter bracket and CAN bus wire bracket, against the
   back of the screen
4. **Four Waveshare mounting brackets**

Steps for each screen:

1. Mount the buck converter on its ABS bracket with the M2.5 × 5 mm standoffs,
   screws and nuts.
2. Lay the buck converter bracket and CAN bus wire bracket on the back of the
   screen.
3. Fit the four Waveshare mounting brackets over them and screw them into the
   screen with M4 × 5 mm screws. The screws clamp the ABS brackets in place.
4. Put the screen through its faceplate opening and bolt the other end of each
   bracket to the faceplate: M4 bolt, flat washer, lock washer and nut.
5. Plug the transceiver onto the 40-pin header and the USB-C lead into the screen.
6. Route the wires and secure them with small zip ties through the bracket notches.

The [back assembly photo](../hardware/mechanical/README.md#assembly-hardware) shows
one finished unit.

### Fitting the faceplate in the pod

Mount the faceplate in the 928 instrument pod with either:

- the **original Porsche bolt brackets**, or
- **two M6 × 12 mm flange bolts** through the 6.5 mm holes in the bent end tabs.

---

## 7. Wiring into the car

```
Terminal 15 (CE panel) ── 4 A fuse ──┬── Buck converter (left)  ── USB-C ── Left screen
                                     └── Buck converter (right) ── USB-C ── Right screen
Ground ──────────────────────────────┴── both buck converter IN−

EV CAN bus (CAN-H / CAN-L) ──┬── Left screen transceiver
                             └── Right screen transceiver
```

### 12 V power

- Take 12 V from **terminal 15 on the Porsche CE (central electrical) panel**.
  That's the switched ignition feed, so the dash is on only with the ignition.
- Fuse it with a **4 A fuse** before the buck converters.
- One fused feed powers both buck converters.
- The first car's power run is about 5 ft to the CE panel. Size yours to your car.

> **Open item:** where the ground is taken from.

### CAN bus

- Connect both screens to the **main EV CAN bus**, the same bus as the
  ZombieVerter, BMS and inverter. The dash doesn't get everything from the
  ZombieVerter:

  | Source | What the dash reads |
  |---|---|
  | ZombieVerter (0x31A, custom mapping) | Drive direction, op mode, 12 V aux voltage |
  | Inverter (0x1DA, 0x55A) | Pack voltage, motor speed (speed and odometer), inverter fault, temperatures |
  | BMS (0x55B, 0x1DB) | State of charge, battery current |
  | Charger (0x390) | Plug and charger status |

- Set up the ZombieVerter's CAN TX mapping for 0x31A
  ([guide](ZombieVerter_CAN_Mappings.md)), or drive direction, op mode and 12 V
  won't show.
- Connect CAN-H to CAN-H and CAN-L to CAN-L. The dashes are taps on the bus, not
  its ends, so their terminators stay off (step 4).
- The first car's CAN run is about 6 ft. Size yours to your car.

---

## 8. Commissioning

### On the bench

1. Build one unit with **test mode on** and power it from its buck converter.
   Every gauge should sweep its full range and back.
2. Swipe left, or tap the arrow on the right edge, to reach the service page.
   Check that touch works.
3. Rebuild with test mode off for the car.

### In the car

1. Ignition on: both screens should start. Before the first CAN frames arrive,
   gauges show `--`.
2. Open the service page on each screen. The CAN state should read **OK**, with
   frame counters going up and a recent "last frame" time.
3. Check each reading against the source: SOC and current against the BMS,
   voltage and temperatures against the inverter, and drive direction and op mode
   against the ZombieVerter.
4. Set the units (mph / km/h and °F / °C) on the service page. **Do this on both
   screens**, because settings are stored per unit.
5. Check the odometer against the car's mileage.
6. Drive: check speed against GPS and that R / N / F follows the selector.

---

## 9. Troubleshooting

| Symptom | Cause and fix |
|---|---|
| Flashing fails: *"requires chip revision in range [v3.0 - v3.99] (this chip is revision v1.3)"* | The build doesn't match the chip revision. Delete `sdkconfig` and `build/`, then rebuild with the right defaults for your chip (see [Firmware](#5-firmware)) |
| Build errors on ESP-IDF 6.x after updating the repo | An old `sdkconfig` or `managed_components/` is left over. Delete `sdkconfig`, `build/` and `managed_components/`, then rebuild |
| Screen works but no CAN frames (service page counters stay at 0) | Check the CTX/CRX pins match menuconfig (TX 35, RX 34 by default); on the first unit RX was wired to 34 when the setting said 36. Then check CAN-H / CAN-L aren't swapped, the bus is at 500 kbit/s, and listen-only is off on a bench |
| Some gauges stay `--` while others work | Their source isn't sending on this bus. Drive direction, op mode and 12 V need the ZombieVerter 0x31A mapping; SOC and current come from the BMS; temperatures from the inverter |
| Current reads about double | Some Leaf decodes use 0.5 A/bit. Compare with a clamp meter; see "Differences" in the main README |
| Screen doesn't power up | Check the fuse, terminal 15 is live with the ignition on, the buck converter output is 5 V and the USB-C polarity |
| Holding the board's BOOT button upsets the CAN bus | GPIO35 (CAN TX) is also the BOOT pin. Don't press BOOT with the car on |
