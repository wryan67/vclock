import math
import random
import sys

################################################################################
# Function: create_arc_path                                                    #
# Calculates the SVG path string for an arc.                                   #
################################################################################
def create_arc_path(cx, cy, r, start_angle, end_angle):
    start_rad = math.radians(start_angle)
    end_rad = math.radians(end_angle)
    x1 = cx + r * math.cos(start_rad)
    y1 = cy + r * math.sin(start_rad)
    x2 = cx + r * math.cos(end_rad)
    y2 = cy + r * math.sin(end_rad)
    large_arc_flag = 1 if end_angle - start_angle > 180 else 0
    return f"M {x1} {y1} A {r} {r} 0 {large_arc_flag} 1 {x2} {y2}"

################################################################################
# Function: generate_svg                                                       #
# Builds the complete SVG content with filters and elements.                   #
################################################################################
def generate_svg():
    width = 1000
    height = 1000
    cx = width / 2
    cy = height / 2
    svg_elements = []

    svg_elements.append(f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {width} {height}" width="100%" height="100%">')
    svg_elements.append('<rect width="100%" height="100%" fill="#0d141f" />')
    
    svg_elements.append('<defs>')
    svg_elements.append('<filter id="glow" x="-50%" y="-50%" width="200%" height="200%">')
    svg_elements.append('<feGaussianBlur stdDeviation="15" result="blur1" />')
    svg_elements.append('<feGaussianBlur stdDeviation="5" result="blur2" />')
    svg_elements.append('<feMerge>')
    svg_elements.append('<feMergeNode in="blur1" />')
    svg_elements.append('<feMergeNode in="blur2" />')
    svg_elements.append('<feMergeNode in="SourceGraphic" />')
    svg_elements.append('</feMerge>')
    svg_elements.append('</filter>')
    svg_elements.append('</defs>')
    
    svg_elements.append('<g filter="url(#glow)" fill="none" stroke="#a3fcff">')

    outer_radius = 420
    circumference_outer = 2 * math.pi * outer_radius
    dash_outer = circumference_outer / 6
    gap_outer = dash_outer * 0.2
    svg_elements.append(f'<circle cx="{cx}" cy="{cy}" r="{outer_radius}" stroke-width="60" stroke-dasharray="{dash_outer - gap_outer} {gap_outer}" transform="rotate(15 {cx} {cy})" />')

    r_mid = 340
    svg_elements.append(f'<circle cx="{cx}" cy="{cy}" r="{r_mid}" stroke-width="8" stroke-dasharray="25 15 80 20 150 40" opacity="0.9" />')

    r_inner_thick = 270
    c_inner = 2 * math.pi * r_inner_thick
    svg_elements.append(f'<circle cx="{cx}" cy="{cy}" r="{r_inner_thick}" stroke-width="25" stroke-dasharray="{c_inner/12} {c_inner/24}" transform="rotate(-45 {cx} {cy})" />')

    r_circuit = 210
    for i in range(36):
        if random.random() > 0.2:
            start = i * 10
            end = start + random.randint(8, 28)
            path = create_arc_path(cx, cy, r_circuit, start, end)
            svg_elements.append(f'<path d="{path}" stroke-width="{random.choice([3, 6, 10])}" opacity="{random.uniform(0.6, 1.0)}" />')
            if random.random() > 0.4:
                path2 = create_arc_path(cx, cy, r_circuit - random.randint(15, 35), start, end - 5)
                svg_elements.append(f'<path d="{path2}" stroke-width="3" opacity="0.7" />')

    r_core = 110
    c_core = 2 * math.pi * r_core
    svg_elements.append(f'<circle cx="{cx}" cy="{cy}" r="{r_core}" stroke-width="80" stroke-dasharray="{c_core/4 - 15} 15" transform="rotate(60 {cx} {cy})" />')

    svg_elements.append(f'<circle cx="{cx}" cy="{cy}" r="35" fill="#a3fcff" stroke="none" />')

    svg_elements.append('</g>')
    svg_elements.append('</svg>')

    return "\n".join(svg_elements)

################################################################################
# Function: main                                                               #
# Entry point of the application. Writes the generated SVG to a file.          #
################################################################################
def main():
    try:
        svg_content = generate_svg()
        with open("hud_generator.svg", "w") as f:
            f.write(svg_content)
    except Exception:
        sys.exit(2)

if __name__ == "__main__":
    main()

