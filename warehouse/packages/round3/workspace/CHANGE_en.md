> English translation for reference; the contestants received the original Chinese text.

[简体中文](CHANGE.md)

# Requirement change (phase 2)

Operations requests three changes. Implement them in `src/` (your lead's code; large changes, rewrites or restarting from `baseline/` are allowed).

Except for the changes specified here, all behavior must remain **exactly identical** to the phase-1 baseline, including every quirk.

## Input change

Append two integers to `PARAMS`:

```text
PARAMS batteryMax chargeRate lowThreshold safetyMargin reportEvery hotRadius agingEvery carryCost
```

Guaranteed `agingEvery ≥ 1`, `carryCost ≥ 1`. Other input unchanged.

## Change 1: order aging

Longer-waiting orders gain priority. At dispatch on `tick`, a pending order o has:

```text
effective priority = original priority + (tick − arrival tick) / agingEvery   (integer division)
```

Sort by **effective priority descending**, arrival tick ascending, integer ID ascending.

- Use the current dispatch tick; new orders have wait 0.
- `REQUEUE` after robot death preserves the original arrival tick, as before.
- `ASSIGN` output unchanged.

## Change 2: loaded movement energy

While `DELIVERING`, each movement step (including `SIDESTEP`) consumes `carryCost`; other states still consume 1.

- Charge according to state **before movement**: the step reaching pickup costs 1; only subsequent `DELIVERING` steps cost `carryCost`.
- **Death:** during movement, when not at target, replace “battery is 0” with “**battery < this step's cost**”: `carryCost` for `DELIVERING`, otherwise 1. Check position and subsequent handling unchanged: already-at-target robots do not die; check remains before blocked target, unreachable path and occupied next-cell checks. Thus insufficient battery kills even a robot that would otherwise wait or sidestep.
- **Dispatch battery check:** replace battery ≥ a+b+c+safetyMargin with:

```text
battery ≥ a + b × carryCost + c + safetyMargin
```

a,b,c unchanged: robot→pickup, pickup→delivery, delivery→nearest passable charger.
- Other battery logic (low threshold, charger selection feasibility, etc.) unchanged.

## Change 3: `aged` report field

Insert `aged=` after `pending=`:

```text
REPORT pending=P aged=A idle=… to_pickup=… delivering=… to_charger=… charging=… dead=… hot=X,Y,K
```

A counts `PENDING` orders with (current tick − arrival tick)/agingEvery ≥ 1 at reporting, i.e. effective priority above original.

## Acceptance

- `workloads2/public/`: 4 public `.in`/`.out` workloads; `workloads2/edge/`: 30 small edge cases.
- `workloads2/regression/`: other teams' counterexamples where your lead's **output differed**, converted to the new format (see `TAKEOVER.md`); all must pass too.
- `python tools/bench2.py`: compile `src/`, all phase-2 tests, byte comparison, CPU timing.
- `python tools/gen2.py CONFIG SEED`: more phase-2 inputs, without expected output.

Official evaluation uses phase-2 hidden workloads; scoring is in `TASK.md`.
