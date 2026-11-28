import argparse
import math
import struct
import sys
from collections import deque

PRESETS = {
    "classic": {
        "eye": (90.0, -120.0, 95.0),
        "center": (0.0, 0.0, -10.0),
        "up": (0.0, 0.0, 1.0),
        "fov": 50.0,
    },
    "topdown": {
        "eye": (55.0, -55.0, 500.0),
        "center": (0.0, 0.0, -30.0),
        "up": (0.0, 1.0, 0.0),
        "fov": 50.0,
    },
}


def sub(a, b):
    return (a[0] - b[0], a[1] - b[1], a[2] - b[2])


def dot(a, b):
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def cross(a, b):
    return (
        a[1] * b[2] - a[2] * b[1],
        a[2] * b[0] - a[0] * b[2],
        a[0] * b[1] - a[1] * b[0],
    )


def normalize(a):
    length = math.sqrt(dot(a, a))
    if length < 1e-12:
        return (0.0, 0.0, 0.0)
    return (a[0] / length, a[1] / length, a[2] / length)


def parse_args():
    parser = argparse.ArgumentParser(
        description="Verify caustic ring-band connectivity on a 24-bit BMP frame dump."
    )
    parser.add_argument("frame", help="Path to 24-bit BMP image")
    parser.add_argument(
        "--preset",
        choices=sorted(PRESETS.keys()),
        default="classic",
        help="Camera preset shorthand (default: classic = (90,-120,95)->(0,0,-10) up (0,0,1))",
    )
    parser.add_argument("--eye", nargs=3, type=float, metavar=("X", "Y", "Z"), help="Camera eye position")
    parser.add_argument("--center", nargs=3, type=float, metavar=("X", "Y", "Z"), help="Camera target position")
    parser.add_argument("--up", nargs=3, type=float, metavar=("X", "Y", "Z"), help="Camera up vector")
    parser.add_argument("--fov", type=float, default=None, help="Vertical FOV in degrees (default: 50)")
    parser.add_argument("--floor-z", type=float, default=-30.0, help="Pool floor Z coordinate (default: -30)")
    parser.add_argument("--inner", type=float, default=94.0, help="Annulus inner radius (default: 94)")
    parser.add_argument("--outer", type=float, default=120.0, help="Annulus outer radius (default: 120)")
    parser.add_argument("--percentile", type=float, default=90.0, help="Luminance percentile (default: 90)")
    return parser.parse_args()


def main():
    args = parse_args()

    with open(args.frame, "rb") as handle:
        data = handle.read()

    if len(data) < 54 or data[0:2] != b"BM":
        print("not a valid bmp")
        sys.exit(2)

    pixel_off = struct.unpack_from("<I", data, 10)[0]
    width = struct.unpack_from("<i", data, 18)[0]
    raw_height = struct.unpack_from("<i", data, 22)[0]
    top_down = raw_height < 0
    height = abs(raw_height)
    bpp = struct.unpack_from("<H", data, 28)[0]
    stride = (width * 3 + 3) & ~3
    if bpp != 24 or width <= 0 or height <= 0:
        print("not a 24-bit bmp")
        sys.exit(2)

    preset = PRESETS[args.preset]
    eye = tuple(args.eye) if args.eye is not None else preset["eye"]
    center = tuple(args.center) if args.center is not None else preset["center"]
    up = tuple(args.up) if args.up is not None else preset["up"]
    fov = args.fov if args.fov is not None else preset["fov"]

    floor_z = args.floor_z
    inner = args.inner
    outer = args.outer
    percentile = args.percentile

    forward = normalize(sub(center, eye))
    right = normalize(cross(forward, up))
    if dot(right, right) < 1e-8:
        right = normalize(cross(forward, (0.0, 1.0, 0.0)))
    cam_up = cross(right, forward)
    focal = 1.0 / math.tan(math.radians(fov) * 0.5)
    aspect = width / float(height)

    def floor_xy(px, py):
        ndc_x = ((px + 0.5) / width) * 2.0 - 1.0
        ndc_y = ((py + 0.5) / height) * 2.0 - 1.0
        dir_view = (ndc_x * aspect / focal, ndc_y / focal, -1.0)
        direction = (
            right[0] * dir_view[0] + cam_up[0] * dir_view[1] + forward[0],
            right[1] * dir_view[0] + cam_up[1] * dir_view[1] + forward[1],
            right[2] * dir_view[0] + cam_up[2] * dir_view[1] + forward[2],
        )
        if abs(direction[2]) < 1e-8:
            return None
        travel = (floor_z - eye[2]) / direction[2]
        if travel <= 0.0:
            return None
        return (eye[0] + travel * direction[0], eye[1] + travel * direction[1])

    count = width * height
    lum = [0.0] * count
    inside = bytearray(count)
    parity = bytearray(count)
    white = 0
    annulus_values = []

    for y in range(height):
        src_y = (height - 1 - y) if top_down else y
        row = pixel_off + src_y * stride
        for x in range(width):
            b, g, r = data[row + x * 3 : row + x * 3 + 3]
            if r == 255 and g == 255 and b == 255:
                white += 1
            i = y * width + x
            lum[i] = 0.2126 * r + 0.7152 * g + 0.0722 * b
            hit = floor_xy(x, y)
            if hit is None:
                continue
            if hit[0] < -200.0 or hit[0] > 200.0 or hit[1] < -200.0 or hit[1] > 200.0:
                continue
            radius = math.hypot(hit[0], hit[1])
            if inner <= radius <= outer:
                inside[i] = 1
                parity[i] = (int(math.floor(hit[0] / 25.0) + math.floor(hit[1] / 25.0)) & 1) + 1
                annulus_values.append(lum[i])

    if not annulus_values:
        print("annulus empty")
        sys.exit(2)

    ordered = sorted(annulus_values)
    rank = min(len(ordered) - 1, int((percentile / 100.0) * (len(ordered) - 1)))
    thresh = ordered[rank]
    mask = bytearray(count)
    above = 0
    above_even = 0
    above_odd = 0
    for i in range(count):
        if inside[i] and lum[i] > thresh:
            mask[i] = 1
            above += 1
            if parity[i] == 1:
                above_even += 1
            elif parity[i] == 2:
                above_odd += 1

    seen = bytearray(count)
    largest = 0
    total = 0
    parts = 0
    for i in range(count):
        if not mask[i] or seen[i]:
            continue
        queue = deque([i])
        seen[i] = 1
        size = 0
        while queue:
            cur = queue.popleft()
            size += 1
            x = cur % width
            y = cur // width
            for nx, ny in ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)):
                if nx < 0 or ny < 0 or nx >= width or ny >= height:
                    continue
                j = ny * width + nx
                if mask[j] and not seen[j]:
                    seen[j] = 1
                    queue.append(j)
        total += size
        parts += 1
        if size > largest:
            largest = size

    fraction = (largest / total) if total else 0.0
    print("camera_eye", eye, "center", center, "up", up, "fov", fov)
    print("annulus", inner, outer)
    print("percentile", int(percentile))
    print("connectivity", 4)
    print("threshold", round(thresh, 2))
    print("above", above)
    print("above_even", above_even)
    print("above_odd", above_odd)
    print("parts", parts)
    print("largest", largest)
    print("fraction", round(fraction, 4))
    print("white", white)
    if white == 0 and fraction >= 0.40:
        print("ring band: PASS")
        sys.exit(0)
    print("ring band: FAIL")
    sys.exit(1)


if __name__ == "__main__":
    main()
