#!/usr/bin/env python3
"""Generate the spiral face's artwork for src/embedded.h.

The face is one filled ribbon: a spiral arm that winds out of the hub, thickens
as it goes, and runs into the rim rather than stopping short of it.  It is
drawn as a single closed path -- out along one side of the arm and back along
the other -- because Qt renders SVG Tiny, which has no variable-width stroke
and no clip-path.

The whole shape follows from two rules, and everything else here is worked out
from them rather than chosen:

    1. every full turn, the arm is GROWTH times thicker than it was
    2. the white space between two turns is as wide as the arm beside it

The first rule alone says the width at turn n is W_HUB * GROWTH**n.  The second
fixes how far apart the turns sit: the distance from one turn to the next has
to cover half of this arm, half of the next one, and a gap equal to the two --
which is w(n) + w(n+1), or w(n) * (1 + GROWTH).

Those two together decide the spiral.  Summing the pitches out from the hub
gives the radius after n turns,

    r(n) = A * (GROWTH**n - 1)      A = W_HUB * (1 + GROWTH) / (GROWTH - 1)

and inverting it gives the width as a function of radius, which falls out
linear:

    w(r) = W_HUB + r * (GROWTH - 1) / (1 + GROWTH)

Note what is *not* a parameter.  The turn count is not chosen -- it is however
many turns it takes to reach the rim, log(1 + R/A) over log(GROWTH).  Nor is the
width at the rim.  Ask for a thinner arm at the hub and the spiral answers with
more turns and a thicker one at the edge, because that is the only way to keep
both rules true across the same disc.

This also means the spiral is logarithmic rather than Archimedean.  An
Archimedean spiral puts the same gap between every pair of turns from hub to
rim, and that evenness is the one thing no shell or fern has; it reads as set
out with a ruler.  Here the spacing grows with the radius at a fixed rate,
which is what growth actually looks like.

W_HUB is the only real dial, and 1.75 is a balance: it puts about six turns on
the face, near enough to what the face has always had, and leaves the innermost
arm and its neighbouring gap both a little under two units of the hundred-wide
artwork -- about one and a half pixels at a ninety-pixel clock, which is the
smallest the face is asked to be drawn.

The arm is allowed to run past the rim and is clamped there, so the last part
of it is a band lying along the rim rather than a spiral stopping at a tangent.
That is what makes the arm appear to melt into the outer circle.
"""

import math

R = 40.0  # the face radius the rest of the artwork is drawn to
RIM = 40.5  # the arm is clamped here, just past the rim, and clipped by it
GROWTH = 1.2  # how much thicker the arm gets over one full turn
W_HUB = 1.75  # the width it starts at, at the very centre

# Everything below is worked out from the two rules; see the note above.
PITCH = W_HUB * (1.0 + GROWTH) / (GROWTH - 1.0)
TURNS = math.log(1.0 + R / PITCH) / math.log(GROWTH)
STEPS = int(120 * TURNS)  # samples along the arm, one side


def centreline(t):
    """A point on the middle of the arm, t running 0..1 from hub to rim."""
    theta = 2.0 * math.pi * TURNS * t
    r = PITCH * (GROWTH ** (TURNS * t) - 1.0)
    return r, theta


def width_at(r):
    return W_HUB + r * (GROWTH - 1.0) / (1.0 + GROWTH)


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
