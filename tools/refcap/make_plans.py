"""Generate reference-capture input plans that exercise every Mario Combat move.

Usage: python -I make_plans.py <out_dir>
Key codes: 37 Left, 38 Up(jump), 39 Right, 40 Down, 65 A(punch).
"""
import json, os, sys

out = sys.argv[1]
os.makedirs(out, exist_ok=True)


def plan(name, steps, level=486, settle=40, zoom=0):
    json.dump({"level": level, "settle": settle, "zoom": zoom, "steps": steps}, open(os.path.join(out, name + ".json"), "w"), indent=1)


# Ground combos: punch taps at different spacings so every attack-clip window gets hit.
steps, t = [], 0
for gap in (3, 5, 7, 9, 11, 13, 16, 20, 24, 30):
    for i in range(6):
        steps.append([t, "65", "combo_g%d_%d" % (gap, i) if i in (1, 3) and gap in (7, 13) else ""])
        steps.append([t + 1, "", ""])
        t += gap
    t += 60
# directional chains: punch with Right/Left held at spacings
for keys in ("65+39", "65+37"):
    for gap in (6, 10, 14, 18):
        for i in range(5):
            steps.append([t, keys, ""])
            steps.append([t + 1, "", ""])
            t += gap
        t += 60
# down attack and down attack while moving
steps += [[t, "40+65", "down1"], [t + 2, "", ""]]; t += 80
steps += [[t, "39", ""], [t + 12, "39+40+65", ""], [t + 14, "", ""]]; t += 90
steps += [[t, "37", ""], [t + 12, "37+40+65", ""], [t + 14, "", ""]]; t += 90
steps.append([t, "END", ""])
plan("combos", steps)

# Air: jump heights (hold lengths), short hops, air attack, air control.
steps, t = [], 0
for hold in (1, 2, 3, 4, 5, 6, 8, 12, 40):
    steps += [[t, "38", ""], [t + hold, "", ""]]; t += 70
steps += [[t, "38", ""], [t + 6, "38+65", "air_punch"], [t + 8, "38", ""], [t + 40, "", ""]]; t += 90
steps += [[t, "38+39", ""], [t + 10, "39+65", ""], [t + 12, "39", ""], [t + 40, "", ""]]; t += 90
steps += [[t, "38+37", ""], [t + 20, "37", ""], [t + 30, "39", ""], [t + 50, "", ""]]; t += 90
steps.append([t, "END", ""])
plan("air", steps)

# Movement: walk/run/skid/turn, stopSliding, left and right.
steps, t = [], 0
for n in (2, 4, 6, 9, 14, 30):
    steps += [[t, "39", ""], [t + n, "", ""]]; t += 40
    steps += [[t, "37", ""], [t + n, "", ""]]; t += 40
steps += [[t, "39", ""], [t + 25, "37", "skid1"], [t + 45, "", ""]]; t += 70
steps += [[t, "37", ""], [t + 25, "39", "skid2"], [t + 27, "", ""]]; t += 70
steps.append([t, "END", ""])
plan("moves", steps)

# Wall: hop the stairs to the plateau (y 78), run right off the cliff at x -186 into the pillar at x -37.5,
# slide down it, wall-jump back left into the plateau's wall, slide, wall-jump again.
steps = [[0, "39", ""], [35, "39+38", ""], [49, "39", ""], [150, "39", "wall_slide_r"],
         [162, "38", "walljump_r"], [164, "37", ""], [172, "37", "flying_left"], [180, "37", "wall_slide_l"],
         [186, "38", "walljump_l"], [188, "39", ""], [200, "", ""], [300, "END", ""]]
plan("wall", steps)

# Combo branches not reached by the timing sweep: attack10 and the attack102-105 chain.
steps, t = [], 0
for rep in range(3):
    steps += [[t, "65", ""], [t + 1, "", ""],              # attack1
              [t + 12, "65", ""], [t + 13, "", ""],        # attack1 f13 -> attack4 (76)
              [t + 17, "65+39", ""], [t + 18, "", ""],     # attack4 f81 + dir -> attack10 (248)
              [t + 23, "65", "a102"], [t + 24, "", ""],    # attack10 f253 -> attack102 (263)
              [t + 29, "65", "a103"], [t + 30, "", ""],    # f268 -> attack103 (278)
              [t + 35, "65", "a104"], [t + 36, "", ""],    # f283 -> attack104 (293)
              [t + 41, "65", "a105"], [t + 42, "", ""]]    # f298 -> attack105 (308)
    t += 120
steps.append([t, "END", ""])
plan("combos2", steps)
# Which art does the jump show? Jump from idle, from a walk, from a run, and right after a skid.
steps, t = [], 0
for label, pre in (("idle", []), ("walk", [(0, "39")]), ("run", [(0, "39")]), ("skid", [(0, "39"), (22, "37")])):
    run_for = {"idle": 0, "walk": 4, "run": 22, "skid": 25}[label]
    for dt, k in pre:
        steps.append([t + dt, k, ""])
    t += run_for
    steps.append([t, "38", ""])
    steps.append([t + 3, "38", f"jump_{label}_03"])
    steps.append([t + 8, "38", f"jump_{label}_08"])
    steps.append([t + 15, "", f"jump_{label}_15"])
    t += 80
    if label in ("walk", "run", "skid"):  # walk back to keep away from the stairs
        steps += [[t, "37", ""], [t + run_for + 2, "", ""]]
        t += 60
steps.append([t, "END", ""])
plan("jumpposes", steps)
print("plans written to", out)
