# scoriginquality

Automatically assigns an **A/B/C/D quality grade** to each SeisComP origin using a
worst-of rule across a set of standard quality parameters, and writes the result
back as an origin Comment accessible in scolv and scesv.

---

## How it works

Each parameter is independently graded A–D against configurable thresholds.
The overall grade is the **worst** of all individual grades (worst-of rule).
Parameters not present in the data are silently skipped — the grade is based on
whatever is available, so the module degrades gracefully across locator types.

---

## Quality parameters

### Network geometry — from `OriginQuality`

These are statistical proxies for how well the network constrains the location.
They are populated by **all SeisComP locators** (LOCSAT, NLL, iLoc, stdloc, …).

| Parameter | SeisComP field | Description |
|-----------|---------------|-------------|
| Azimuthal gap | `OriginQuality.azimuthalGap` | Largest angular gap between adjacent stations as seen from the epicentre. A large gap means the event is poorly constrained from one direction. |
| Secondary azimuthal gap | `OriginQuality.secondaryAzimuthalGap` | Largest gap when the single closest station is removed. More robust than the primary gap against outlier stations. |
| Used station count | `OriginQuality.usedStationCount` | Number of stations whose picks were used in the solution. |
| Minimum distance | `OriginQuality.minimumDistance` | Distance (degrees) to the nearest used station. Events with no nearby station have poorly constrained depth and origin time. |

> **Note:** Secondary azimuthal gap requires the fix in
> [SeisComP/common#193](https://github.com/SeisComP/common/pull/193) — without it
> the field may not be populated by all locators.

### Locator output — from `OriginQuality` and `OriginUncertainty`

These are direct output measures from the locator itself.
Which fields are populated depends on the locator used.

| Parameter | SeisComP field | Populated by | Description |
|-----------|---------------|-------------|-------------|
| RMS residual | `OriginQuality.standardError` | All locators | Travel-time RMS residual (seconds). Measures how well the computed travel times fit the observed picks. |
| Horizontal uncertainty | `OriginUncertainty.maxHorizontalUncertainty` | LOCSAT, iLoc | Semi-major axis of the horizontal uncertainty ellipse (km). Represents the formal horizontal location error. |
| Depth uncertainty | `OriginUncertainty.depthUncertainty` | iLoc, stdloc | Formal depth uncertainty (km). Not populated by LOCSAT. |
| Ground truth level | `OriginQuality.groundTruthLevel` | iLoc only | iLoc GT classification (GT0–GT25). Indicates the event is a known calibration event with a verified location. GT0/GT1 → A, GT2/GT5 → B, GT10/GT25 → C. |

---

## Default thresholds

| Grade | Az. Gap | Sec. Gap | RMS    | Stations | Min. Dist | Horiz. Unc. | Depth Unc. |
|-------|---------|----------|--------|----------|-----------|-------------|------------|
| A     | ≤ 90°   | ≤ 135°   | ≤ 0.15s | ≥ 10   | ≤ 30°     | ≤ 5 km      | ≤ 5 km     |
| B     | ≤ 135°  | ≤ 180°   | ≤ 0.30s | ≥ 6    | ≤ 60°     | ≤ 10 km     | ≤ 15 km    |
| C     | ≤ 180°  | ≤ 210°   | ≤ 0.50s | ≥ 4    | ≤ 90°     | ≤ 20 km     | ≤ 30 km    |
| D     | anything worse than C |||||||

All thresholds are configurable via the SeisComP config system (see Configuration below).

---

## Comment format

The grade is written as an origin Comment with ID `quality`. The text is
multi-line: grade letter on line 1, one parameter breakdown per line after:

```
B
Az. Gap: 76.3° → A
Sec. Gap: 115.1° → B
RMS: 0.937 s → D
Stations: 12 → A
Min. Dist: 18.2° → A
Horiz. Unc.: 7.4 km → B
Depth Unc.: 12.3 km → B
```

The companion GUI PR ([SeisComP/common#194](https://github.com/SeisComP/common/pull/194))
splits on the first newline: the label shows the grade letter, the tooltip shows the
full per-parameter breakdown on hover.

---

## Modes of operation

| Mode | Flag | Description |
|------|------|-------------|
| Live | _(default)_ | Subscribe to LOCATION messages, grade each incoming origin |
| Single event | `--event <id>` | Grade preferred origin of one DB event and exit |
| XML file | `--ep <file>` | Grade all origins in a SeisComP XML file |
| All DB events | `--all` | Regrade every event in the database in one run |

The `--all` mode is useful for backfilling grades after deployment or after
changing thresholds.

---

## Configuration

All thresholds can be overridden in `scoriginquality.cfg`:

```ini
# Comment ID written to the origin (default: quality)
quality.commentID = quality

# Grade A thresholds
quality.A.maxGap              = 90.0    # degrees
quality.A.maxSecondaryGap     = 135.0   # degrees
quality.A.maxRMS              = 0.15    # seconds
quality.A.minStations         = 10
quality.A.maxMinDist          = 30.0    # degrees
quality.A.maxHorizUncertainty = 5.0     # km
quality.A.maxDepthUncertainty = 5.0     # km

# Grade B thresholds
quality.B.maxGap              = 135.0
quality.B.maxSecondaryGap     = 180.0
quality.B.maxRMS              = 0.30
quality.B.minStations         = 6
quality.B.maxMinDist          = 60.0
quality.B.maxHorizUncertainty = 10.0
quality.B.maxDepthUncertainty = 15.0

# Grade C thresholds
quality.C.maxGap              = 180.0
quality.C.maxSecondaryGap     = 210.0
quality.C.maxRMS              = 0.50
quality.C.minStations         = 4
quality.C.maxMinDist          = 90.0
quality.C.maxHorizUncertainty = 20.0
quality.C.maxDepthUncertainty = 30.0
```

---

## Related

- GUI display (scolv + scesv): [SeisComP/common#194](https://github.com/SeisComP/common/pull/194)
- Secondary azimuthal gap fix: [SeisComP/common#193](https://github.com/SeisComP/common/pull/193)
