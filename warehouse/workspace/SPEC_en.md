> English translation for reference; the contestants received the original Chinese text.

[简体中文](SPEC.md)

# Warehouse-robot scheduling simulation: behavior specification (v2.0, 2022)

> **This document was written at launch in 2022. Code has changed repeatedly without documentation updates.** Most remains accurate, but some disagrees with the code; comments may also be outdated. **Actual behavior of `baseline/` is authoritative:** every legal input must produce byte-identical output, treating CRLF/LF as equivalent.

## 1. Input

Whitespace-separated text from stdin:

```text
W H T
<H rows, W characters each>
PARAMS batteryMax chargeRate lowThreshold safetyMargin reportEvery hotRadius
ROBOTS R
x y            (R rows; row i gives robot i's initial position, named "R<i>")
EVENTS E
<E event rows>
```

- Map: `.` floor, `#` wall/shelf, `C` charger (also floor).
- Coordinates `(x, y)`: column/row, 0-based.
- Events: `tick TYPE arguments…`, four types: `ORDER id px py dx dy priority` (pickup, delivery, larger priority first); `CANCEL id`; `BLOCK x y`; `UNBLOCK x y`.
- Guarantees: nondecreasing event ticks, 0 ≤ tick < T, unique positive integer order IDs without leading zeros, distinct initial floor cells, map enclosed by walls.

## 2. Concepts

- **Passable:** inside map, not wall, not blocked. Chargers passable unless blocked.
- **Direction order:** up (0,−1), right (1,0), down (0,1), left (−1,0); all direction ties follow this.
- **Row-major:** compare y then x.
- **BFS distance `dist(a, b)`:** 0 if a == b; otherwise INF if b impassable; otherwise four-neighbor BFS from a, **starting from a whether or not it is passable**, then traversing passable cells. Unreachable = INF. If a is impassable, `dist(a,b)` and `dist(b,a)` may differ.
- **Robot states:** `IDLE`, `TO_PICKUP`, `DELIVERING`, `TO_CHARGER`, `CHARGING`, `DEAD`.
- **Order states:** `PENDING`, `ASSIGNED`, `PICKED`, `DONE`, `CANCELLED`, `LOST`.
- **Occupancy:** every non-`DEAD` robot occupies its cell.

## 3. Tick processing

For tick = 0,1,…,T−1, in order:

### 3.1 External events

Process this tick's events in input order:

- **ORDER:** pickup/delivery off-map or wall → reject, `REJECT id`. Otherwise `PENDING`, arrival = current tick.
- **CANCEL:** nonexistent or not `PENDING`/`ASSIGNED` → `CANCEL_FAIL id`; `PENDING` → `CANCELLED`, `CANCEL id`; `ASSIGNED` → responsible robot immediately `IDLE`, clear order/target and consecutive waits, order `CANCELLED`, `CANCEL id`.
- **BLOCK:** block an in-map non-wall cell (already blocked: unchanged). Does not affect a robot already there.
- **UNBLOCK:** remove block; unblocked cells unchanged.

### 3.2 Rescue

In ID order, for every `DEAD` robot: if current tick − death tick ≥ 100 and no other robot occupies its cell, fill battery, set `IDLE`, clear consecutive waits, output `RESCUE NAME`.

### 3.3 Charging

In ID order, increase each `CHARGING` robot's battery by chargeRate, capped at batteryMax. At full battery set `IDLE`, output `CHARGED NAME`.

### 3.4 Dispatch

1. Sort `PENDING` orders by priority descending, arrival tick ascending, integer ID ascending.
2. Process each o. Candidates: `IDLE` robots with battery ≥ lowThreshold (already assigned this tick no longer qualify). For each r calculate a = dist(robot position, pickup), b = dist(pickup, delivery), c = minimum dist(delivery, q) over **passable** chargers q. Any INF, or battery < a+b+c+safetyMargin, disqualifies r.
3. Choose smallest a, then lowest robot ID. Robot → `TO_PICKUP`, target pickup, clear consecutive waits; order → `ASSIGNED`; output `ASSIGN id NAME a`. Without a candidate it stays `PENDING`.

### 3.5 Low battery: go charge

In ID order, for each `IDLE` robot with battery < lowThreshold:

1. Enumerate passable chargers row-major, excluding those occupied by **another** `CHARGING` robot or targeted by another `TO_CHARGER` robot, and those with dist(position,q)+safetyMargin > battery. Choose smallest distance, row-major tie.
2. If none, choose minimum finite distance among all passable chargers, row-major tie, ignoring occupancy.
3. If still none, do nothing. Otherwise set `TO_CHARGER`, target charger, clear consecutive waits, output `GO_CHARGE NAME x y`.

### 3.6 Movement

In ID order process `TO_PICKUP`, `DELIVERING`, `TO_CHARGER` robots. Earlier robots' movement this tick affects later occupancy checks.

1. Already at target → step 6 (arrival).
2. Battery 0 → **death** (3.8), finish.
3. Impassable target → wait, total waits +1, consecutive waits unchanged; finish.
4. Among passable orthogonal neighbors choose minimum `dist(target, neighbor)` (BFS **from target**), direction-order ties. Minimum INF → wait, total +1, consecutive unchanged; finish.
5. If next cell occupied by another robot: if consecutive waits ≥ 4 and next cell **is not the target**, sidestep into first passable unoccupied neighbor in direction order: battery −1, steps +1, clear consecutive waits, output `SIDESTEP NAME`, go to step 6. If none, or sidestep condition fails: wait, total +1, consecutive +1; finish. If next cell is free: move, battery −1, steps +1, clear consecutive waits.
6. **Arrival:** if at target, clear consecutive waits and act by state: `TO_PICKUP` → order `PICKED`, robot `DELIVERING`, target delivery, `PICK id NAME`, no more movement this tick; `DELIVERING` → order `DONE`, latency = current − arrival tick, `DELIVER id NAME LATENCY`, robot `IDLE`, clear order/target; `TO_CHARGER` → `CHARGING`, `CHARGE NAME`.

### 3.7 Reports

If (tick+1) is a multiple of reportEvery:

```text
REPORT pending=P idle=… to_pickup=… delivering=… to_charger=… charging=… dead=… hot=X,Y,K
```

P counts `PENDING` orders; remaining state counts are robots. **Hotspot:** enumerate non-wall cells row-major, count **non-dead** robots within Manhattan distance ≤ hotRadius; choose maximum K, row-major tie at (X,Y).

### 3.8 Death

Output `DEAD NAME`. If `TO_PICKUP`, order → `PENDING` retaining arrival tick, output `REQUEUE id`. If `DELIVERING`, order → `LOST`, output `LOST id`. Robot → `DEAD`, record death tick, clear order/target. Dead robots no longer occupy cells.

## 4. Output

- Event lines: `tick EVENT arguments…` in generation order.
- After all ticks:

```text
SUMMARY delivered=… lost=… rejected=… cancelled=… open=…
LATENCY sum=… max=… avg=…
ROBOT NAME x y BATTERY STATE STEPS TOTAL_WAITS      (one per robot, ID order)
```

`open` counts `PENDING`/`ASSIGNED`/`PICKED` orders. `avg` is integer sum/delivered, 0 if none delivered. All arithmetic is integer, no floating point.
