"""Saint-Lo's training played by a script (host copy): prints the COD3_COOP_TESTGO and COD3_PAD values.

Places come from the census of t975b (COD3_PROBE40): helmets 'ev1_helmets', the grenade box, the barn's
glowing door and windows, the Thompson crate, the intro truck. Times are seconds of the host process
(the pad script's clock); control comes at about 200 s with films skipped.

usage: python -I tools/coop/stlo_route.py [--upto STEP]   -> two lines: TESTGO=..., PAD=...
"""
import argparse

FEET = 292
HELMETS = [(7914, 9233, 391), (7843, 9505, 399), (7837, 9642, 391), (7958, 9440, 374), (7901, 9374, 388), (7676, 9233, 439)]

ap = argparse.ArgumentParser()
ap.add_argument("--upto", default="truck")
ap.add_argument("--start", type=float, default=200.0)
a = ap.parse_args()

go, pad = [], []
aimmodel = ""
t = a.start


def at(dt):
    return "%g" % round(t + dt, 1)


# 1: move and look (the known recipe)
pad += ["%s:ly+50/1.2" % at(0), "%s:rx+40/1" % at(2), "%s:ry+30/0.5" % at(4), "%s:ry-30/0.5" % at(5)]
t += 15
# 2: the Garand on the barrel
go.append("%s:8165,9373,300" % at(0))
pad += ["%s:ry-50/0.7" % at(2), "%s:x/3" % at(5)]
t += 10
steps = ["garand"]
if a.upto != "garand":
    # 3: fire, then aim and fire
    pad += ["%s:rt/0.2" % at(0), "%s:lt/1.5" % at(3), "%s:rt/0.2" % at(3.8)]
    t += 6
    # 4: the helmets downrange (they move when the step begins, t976a): from the firing line, turned to the nearest
    # helmet by COD3_COOP_TESTAIMMODEL, aimed down the sights the whole time, a shot every 0.7 s
    go.append("%s:8178,9449,293" % at(0))
    aimmodel = "%s-%s:helmet_us_objctv" % (at(1), at(44))
    # (0.5.979) the aid steers the right stick, holds the sights and fires when settled: no pad here
    t += 45
    steps.append("helmets")
if a.upto not in ("garand", "helmets"):
    t += 4
    # 5: the grenades on the box, then through the barn's door and its windows (300 short)
    go.append("%s:8169,9390,326,30,%d,270" % (at(0), FEET))
    pad += ["%s:x/2" % at(1)]
    t += 5
    # (t982a: rs/0.3 threw nothing; the game throws on the release of a held button) held 1 s, aimed into the
    # openings' middles (the census places are their tops), each opening twice (four grenades in the box)
    for _ in range(2):
        go.append("%s:7628,9434,370,250,%d,175" % (at(0), FEET))
        pad += ["%s:rs/1" % at(1.5)]
        t += 5
        go.append("%s:7655,9588,355,250,%d,160" % (at(0), FEET))   # t982b missed at 7667,9621,360: the outline sat ~7 degrees left
        pad += ["%s:rs/1" % at(1.5)]
        t += 5
    # 6: the smoke grenades on the box, one thrown down range
    go.append("%s:8165,9394,321,30,%d,270" % (at(0), FEET))
    pad += ["%s:x/2" % at(1)]
    t += 5
    go.append("%s:7800,9440,400,400,%d,180" % (at(0), FEET))
    pad += ["%s:ls/1" % at(2)]
    t += 6
    steps.append("grenades")
if a.upto in ("thompson", "truck", "struggle"):
    t = max(t, a.start + 146)   # (t982c) the crate's objective comes at 344 s, after the dialogue
    # 7: the Thompson's crate: melee the lid, take the gun, switch to it
    go.append("%s:9544,9356,337,45,300,10" % at(0))
    # (t983a: with two copies the lid's objective came after both presses) four presses over 6 s, then the gun
    pad += ["%s:r3/0.4" % at(1.5), "%s:r3/0.4" % at(3), "%s:r3/0.4" % at(4.5), "%s:r3/0.4" % at(6),   # (t986d: a 2-read click is lost with two copies running; 0.4 s held is not)
            "%s:x/2" % at(8), "%s:x/2" % at(10.5), "%s:y/0.4" % at(13), "%s:y/0.4" % at(15)]
    t += 16
    steps.append("thompson")
if a.upto in ("truck", "struggle"):
    # 8: the truck
    # (t982d: put on the truck bed, no objective) the user (t959u) finished it walking up behind the truck at
    # 8747 9174: stood there facing the truck, walking into it, Use held after
    go.append("%s:8864,9029,326,185,288,-51" % at(0))
    pad += ["%s:ly+/2.5" % at(1), "%s:x/3" % at(5), "%s:x/3" % at(10)]   # t982e: the prompt "Press and hold to climb into the truck" stood at 364 s
    t += 14
    steps.append("truck")
if a.upto == "struggle":
    # (t990e/f) NOT a struggle: the truck ride carries the host over the wall and the 30 s hold is a cinematic that
    # needs no input; the prompt texts below came from a string dump. Kept as the way to run past the wall.
    # 9 (t959u, the user): the truck ride ends ~140 s after boarding, off at 1637 -1150; the wall at 1557 -1449 (feet 32),
    # hopped by a prompt to land near 1354 -1420; the struggle held him at 1160 -1414 for 30 s ("Pull r1 and l1
    # alternately as fast as you can", then "Press [BATTLE_ACTION_BUTTON] to attack / to finish")
    t = max(t, a.start + 315)
    go.append("%s:1354,-1420,62,203,32,172" % at(0))
    pad += ["%s:x/3" % at(2), "%s:x/3" % at(6)]
    t += 10
    for k in range(130):
        pad += ["%s:lt/0.1" % at(0.3 * k), "%s:rt/0.1" % at(0.3 * k + 0.15)]
    t += 40
    steps.append("struggle")
print("COD3_COOP_TESTGO=" + " ".join(go))
print("COD3_PAD=" + " ".join(pad))
print("COD3_COOP_TESTAIMMODEL=" + aimmodel)
print("# steps:", ", ".join(steps), "; ends at %.0f s" % t)
