import math

R = 7.6
dx = math.sqrt(3) * R
dy = 1.5 * R
CX = CY = 50.0
CLIP = 40.0

# Qt's SVG renderer implements SVG Tiny, which has no clip-path, so the disc is
# cut into the geometry here rather than left to the renderer.
DISC = [(CX + CLIP*math.cos(2*math.pi*k/96), CY + CLIP*math.sin(2*math.pi*k/96))
        for k in range(96)]

def clip(poly, a, b):
    """Sutherland-Hodgman against the half-plane left of edge a->b."""
    def inside(p):
        return (b[0]-a[0])*(p[1]-a[1]) - (b[1]-a[1])*(p[0]-a[0]) >= 0
    def cross(p, q):
        d1, d2 = (q[0]-p[0], q[1]-p[1]), (b[0]-a[0], b[1]-a[1])
        den = d1[0]*d2[1] - d1[1]*d2[0]
        if abs(den) < 1e-12:
            return q
        t = ((a[0]-p[0])*d2[1] - (a[1]-p[1])*d2[0]) / den
        return (p[0]+t*d1[0], p[1]+t*d1[1])
    out = []
    for i, q in enumerate(poly):
        p = poly[i-1]
        if inside(q):
            if not inside(p):
                out.append(cross(p, q))
            out.append(q)
        elif inside(p):
            out.append(cross(p, q))
    return out

def clipToDisc(poly):
    for i in range(len(DISC)):
        poly = clip(poly, DISC[i-1], DISC[i])
        if not poly:
            return []
    return poly

def hexagon(cx, cy, r):
    return [(cx + r*math.cos(math.radians(90 + 60*k)),
             cy + r*math.sin(math.radians(90 + 60*k))) for k in range(6)]

def area(poly):
    a = 0.0
    for i, q in enumerate(poly):
        p = poly[i-1]
        a += p[0]*q[1] - q[0]*p[1]
    return abs(a) / 2.0

def points(poly):
    # Clipping against 96 edges leaves runs of coincident vertices once the
    # coordinates are rounded; they draw the same shape either way.
    out = []
    for p in poly:
        t = "%.1f,%.1f" % p
        if not out or out[-1] != t:
            out.append(t)
    if len(out) > 1 and out[0] == out[-1]:
        out.pop()
    return " ".join(out)

def grey(v):
    v = max(0, min(255, int(round(v))))
    return "#%02x%02x%02x" % (v, v, v)

# Lit from the top left: brightness falls along that diagonal, which is what
# gives recolor() a wide range to map onto the face and wire colours.  The cell
# interiors are the bright end, so they take the face colour; the walls and the
# rim are the dark end, so they take the wire colour.
def light(cx, cy):
    t = ((cx - CX) + (cy - CY)) / (2 * CLIP)
    return max(0.0, min(1.0, 0.5 - t * 0.62))

cells = []
rows, cols = int(CLIP/dy) + 2, int(CLIP/dx) + 2
for row in range(-rows, rows+1):
    cy = CY + row*dy
    offset = (dx/2) if row % 2 else 0.0
    for col in range(-cols, cols+1):
        cx = CX + col*dx + offset
        if math.hypot(cx-CX, cy-CY) <= CLIP + R:
            cells.append((cx, cy))

walls, cavities = [], []
for cx, cy in cells:
    l = light(cx, cy)
    w = clipToDisc(hexagon(cx, cy, R))
    if area(w) > 0.05:
        walls.append('  <polygon points="%s" fill="%s" />' % (points(w), grey(2 + 34*l)))
    # Offset down and right, so the lit top-left wall is the thicker one.
    c = clipToDisc(hexagon(cx + 0.55, cy + 0.75, R*0.72))
    if area(c) > 0.05:
        cavities.append('  <polygon points="%s" fill="%s" />' % (points(c), grey(105 + 150*l)))

svg = ('<svg xmlns="http://www.w3.org/2000/svg" width="100" height="100">\n'
       '  <circle cx="50" cy="50" r="%s" fill="#000000" />\n' % CLIP
       + "\n".join(walls) + "\n" + "\n".join(cavities) + "\n"
       '  <circle cx="50" cy="50" r="40" fill="none" stroke="#000000" stroke-width="3" />\n'
       '</svg>\n')
open('/tmp/comb.svg','w').write(svg)
print(len(walls), "walls,", len(cavities), "cavities,", len(svg), "bytes")
