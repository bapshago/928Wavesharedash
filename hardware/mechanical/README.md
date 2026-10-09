# Mechanical parts (laser cut)

Cut files for the dash pod and electronics brackets. The DXF drawings are in
millimetres.

| File | Part | Material | How it's made |
|---|---|---|---|
| `InstrumentCluster_RV1_1_cut3_SCS.dxf` | Instrument cluster plate: two 90 mm gauge openings, mounting tab each end (416 × 120 mm flat) | 16 gauge (1/16") cold rolled steel | SendCutSend, then bent (below) |
| `InstrumentCluster_RV1_1_cut4_SCS.dxf` | Instrument cluster strip, two 4.5 mm holes (33 × 10 mm flat) | 16 gauge (1/16") cold rolled steel | SendCutSend, then bent |
| `SCS_BuckConverterForDash_Plate_V1_1.dxf` | Buck converter mounting plate | 1/16" ABS | Laser cut |
| `SCS_BuckConverterForDash_Plate2_V1_1.dxf` | Buck converter mounting plate 2 | 1/16" ABS | Laser cut |
| `CanBusCableHolder.xcs` | CAN bus cable management holder | 1/16" ABS | Laser cut (xTool Creative Space project) |
| `InstrumentClusterBracket1_v1_1.dxf` | Unconfirmed: same outline as the buck converter plate with different small holes | — | — |

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

## Bending the strip (cut4)

Bend details to be added.

Fasteners for each part will be added with the full build guide.
