#!/usr/bin/env python3
"""Generate the spiral face's artwork for src/embedded.h.

The face is one filled ribbon: a spiral arm that starts pencil-thin at the hub,
broadens as it winds out, and runs into the rim rather than stopping short of
it.  It is drawn as a single closed path -- out along one side of the arm and
back along the other -- because Qt renders SVG Tiny, which has no variable-width
stroke and no clip-path.

Two laws describe it.

    radius   r(t) = R * t**TAPER          t running 0..1 over TURNS turns
    width    w(r) = W_HUB + (W_RIM - W_HUB) * (r/R)**SWELL

TAPER is the interesting one.  At 1 the spiral is Archimedean: every turn sits
the same distance from the last, which is the one thing a shell or a fern never
does and is what made the old face look mechanical.  Above 1 the gap between
turns closes up towards the middle -- the spacing goes as r**(1 - 1/TAPER) --
so the winding tightens as it approaches the hub, which is what growth actually
looks like.

Tightening the middle without also widening the outside means there have to be
more turns to fill the same disc, and TURNS is tied to TAPER for that reason:
the gap at the rim is about R*TAPER/TURNS, so holding TURNS at 5.75*TAPER holds
the rim spacing where it was and spends the taper entirely on the middle.

1.5 is as far as it goes.  The gap between the first and second turns falls as
TAPER**-something steep -- 6.9 units at 1.0, 2.9 at 1.5, 0.9 at 2.0 -- and the
face has to survive being drawn at ninety pixels across, where a gap under
about 1.5 units closes up and the hub becomes a smudge.

The arm is allowed to run past the rim and is clamped there, so the last part
of it is a band lying along the rim rather than a spiral stopping at a tangent.
That is what makes the arm appear to melt into the outer circle.
"""

import math

R = 40.0  # the face radius the rest of the artwork is drawn to
RIM = 40.5  # the arm is clamped here, just past the rim, and clipped by it
TURNS = 8.62  # TAPER * 5.75, which holds the gap at the rim where it was
TAPER = 1.5  # >1 packs the turns towards the hub
SWELL = 3.26  # how sharply the arm broadens with radius
W_HUB = 0.40  # the width it starts at, at the very centre
W_RIM = 3.20  # the width it would reach at the rim
STEPS = 780  # samples along the arm, one side


def centreline(t):
    """A point on the middle of the arm, and the direction it is heading."""
    theta = 2.0 * math.pi * TURNS * t
    r = R * t**TAPER
    return r, theta


def width_at(r):
    return W_HUB + (W_RIM - W_HUB) * (r / R) ** SWELL


def offset_points(sign):
    """One side of the arm: the centreline pushed out along its own normal."""
    out = []
    for i in range(STEPS + 1):
        t = i / STEPS
        r, theta = centreline(t)
        # The tangent of r(theta) in polar form, as a plane vector.
        dt = 1e-6
        r2, th2 = centreline(min(1.0, t + dt))
        x1, y1 = r * math.cos(theta), r * math.sin(theta)
        x2, y2 = r2 * math.cos(th2), r2 * math.sin(th2)
        dx, dy = x2 - x1, y2 - y1
        length = math.hypot(dx, dy) or 1.0
        nx, ny = -dy / length, dx / length
        half = width_at(r) / 2.0
        x, y = x1 + sign * nx * half, y1 + sign * ny * half
        # Clamp to the rim rather than stopping at it, so the arm ends as a
        # band lying along the outer circle instead of a spiral cut off at a
        # tangent.
        rr = math.hypot(x, y)
        if rr > RIM:
            x, y = x * RIM / rr, y * RIM / rr
        out.append((50.0 + x, 50.0 + y))
    return out


def main():
    forward = offset_points(+1)
    backward = offset_points(-1)[::-1]
    pts = forward + backward
    body = " ".join(
        ("M" if i == 0 else "L") + f" {x:.2f} {y:.2f}" for i, (x, y) in enumerate(pts)
    )
    print('  <circle cx="50" cy="50" r="40" fill="#e8e8e8" />')
    print(f'  <path d="{body} Z" fill="#141414"/>')
    print('  <circle cx="50" cy="50" r="40" stroke="#171717" stroke-width="1" fill="none" />')


if __name__ == "__main__":
    main()
