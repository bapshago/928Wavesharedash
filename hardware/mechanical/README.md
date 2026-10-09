# Mechanical parts (laser cut)

Cut files for the dash pod and electronics brackets. The DXF drawings are in
millimetres.

| File | Part | Qty (both units) | Material | How it's made |
|---|---|---|---|---|
| `InstrumentCluster_RV1_1_cut3_SCS.dxf` | Instrument cluster plate: two 90 mm gauge openings, mounting tab each end (416 × 120 mm flat) | 1 | 16 gauge (1/16") cold rolled steel | SendCutSend, then bent (below) |
| `InstrumentCluster_RV1_1_cut4_SCS.dxf` | Screen bracket: joins a Waveshare screen to the back of cut3 (33 × 10 mm flat, two 4.5 mm holes) | 8 | 16 gauge (1/16") cold rolled steel | SendCutSend, then bent (below) |
| `SCS_BuckConverterForDash_Plate_V1_1.dxf` | Buck converter mounting plate (attaches to the screen; flat, no bends) | see below | 1/16" ABS | Laser cut |
| `SCS_BuckConverterForDash_Plate2_V1_1.dxf` | Buck converter mounting plate 2 (attaches to the screen; flat, no bends) | see below | 1/16" ABS | Laser cut |
| `CanBusCableHolder.xcs` | CAN bus wire management (attaches to the screen; flat, no bends) | see below | 1/16" ABS | Laser cut (xTool Creative Space project) |

## Ordering the steel parts from SendCutSend

Upload the `InstrumentCluster_*_SCS.dxf` files, confirm the units are
**millimetres** when the part preview loads (cut3 should show about 416 × 120 mm),
and choose **cold rolled steel, 16 gauge**. SendCutSend's 16 ga steel is nominally
0.060" (1.52 mm), close enough to 1/16" (0.0625") for these parts.

## Bending the instrument cluster plate (cut3)

![Bend lines on the instrument cluster plate](InstrumentCluster_RV1_1_cut3_bends.svg)

Each end of the plate has a tab with a 6.5 mm hole near its top. Bend each tab
**90°** on a line **20 mm down from the top edge of the tab** (the end with the
hole). The tab is free of the main plate for its top 25 mm, so the bend line sits
5 mm above where the tab joins the plate; the 20 mm flap with the hole is what
folds over.

## Bending the screen brackets (cut4)

![Bend lines on the screen bracket](InstrumentCluster_RV1_1_cut4_bends.svg)

Two 90° bends in opposite directions make a **5 mm jog** (a Z shape):

1. **Bend 1:** 2 mm in from the edge of one hole, toward the middle of the part
   (about 9.3 mm from that end).
2. **Bend 2:** 5 mm past bend 1 (about 14.3 mm from the same end), back the other
   way, so the two hole ends finish parallel and 5 mm apart.

The holes sit almost exactly symmetric on the strip, so either end can be the start.

## Assembly stack

The pair needs 2 buck converter brackets and 2 CAN bus wire brackets in ABS,
one of each per unit. Each unit stacks from the front of the car backwards:

1. **cut3**, the instrument cluster plate. One plate carries both screens, one
   per gauge opening.
2. **Waveshare screen**, face forward through the opening.
3. **ABS brackets** (buck converter bracket and CAN bus wire bracket) against
   the back of the screen.
4. **Four cut4 screen brackets** over the ABS brackets. One end of each bracket
   screws into the screen through the ABS brackets. The 5 mm jog brings the other
   end forward onto cut3, where it bolts on.

## Power

The buck converter's 5 V output has a USB-C cable soldered on, which plugs into
the Waveshare screen. Before plugging it into the screen, set the output to
**5 V** and **check the polarity** at the USB-C end.

## Assembly hardware

| Joint | Hardware |
|---|---|
| Screen bracket (cut4) to the Waveshare screen, clamping the ABS brackets | M4 × 5 mm screws |
| Screen bracket (cut4) to the back of cut3 | M4 bolts, flat washers, lock washers and nuts |
| Buck converter to its ABS mounting plate | M2.5 × 5 mm standoffs, screws and nuts |
| Wires to the brackets | Small zip ties through the notches in the brackets |

![Back of the instrument cluster plate with one unit assembled](photos/back_assembly.jpg)

*Back of cut3 with one unit fitted:*
- *The Waveshare screen sits over a gauge opening, held by four cut4 brackets (two M4 screws each).*
- *The SN65HVD230 CAN transceiver plugs onto the screen's GPIO header.*
- *The LM2596 buck converter sits on M2.5 standoffs to the right.*
- *Zip ties through the bracket notches hold the wiring.*

Full assembly steps will be added with the build guide.
