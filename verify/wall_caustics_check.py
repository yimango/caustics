import argparse
import math
import struct
import sys
from collections import defaultdict

PRESETS = {
    "1": {
        "eye": (55.0, -55.0, 500.0),
        "center": (0.0, 0.0, -30.0),
        "up": (0.0, 1.0, 0.0),
        "fov": 50.0,
    },
    "topdown": {
        "eye": (55.0, -55.0, 500.0),
        "center": (0.0, 0.0, -30.0),
        "up": (0.0, 1.0, 0.0),
        "fov": 50.0,
    },
    "2": {
        "eye": (90.0, -120.0, 95.0),
        "center": (0.0, 0.0, -10.0),
        "up": (0.0, 0.0, 1.0),
        "fov": 50.0,
    },
    "classic": {
        "eye": (90.0, -120.0, 95.0),
        "center": (0.0, 0.0, -10.0),
        "up": (0.0, 0.0, 1.0),
        "fov": 50.0,
    },
    "3": {
        "eye": (130.0, -130.0, 55.0),
        "center": (-120.0, 120.0, -15.0),
        "up": (0.0, 0.0, 1.0),
        "fov": 50.0,
    },
    "corner": {
        "eye": (130.0, -130.0, 55.0),
        "center": (-120.0, 120.0, -15.0),
        "up": (0.0, 0.0, 1.0),
        "fov": 50.0,
    },
    "4": {
        "eye": (0.0, -240.0, 45.0),
        "center": (0.0, 0.0, -10.0),
        "up": (0.0, 0.0, 1.0),
        "fov": 50.0,
    },
    "side": {
        "eye": (0.0, -240.0, 45.0),
        "center": (0.0, 0.0, -10.0),
        "up": (0.0, 0.0, 1.0),
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


def percentile_sorted(ordered, p):
    if not ordered:
        return 0.0
    idx = min(len(ordered) - 1, max(0, int((p / 100.0) * (len(ordered) - 1))))
    return ordered[idx]


def stddev(values):
    n = len(values)
    if n < 2:
        return 0.0
    mean = sum(values) / n
    var = sum((v - mean) * (v - mean) for v in values) / n
    return math.sqrt(var)


def intersect_pool_interior(origin, direction, hw, hh, bottom_z, lip_z):
    """Return (face_id, hit_x, hit_y, hit_z, t) for the closest interior hit of the open-top pool box.

    face_id: 0=floor, 1=south(y=-hh), 2=east(x=+hw), 3=north(y=+hh), 4=west(x=-hw), None=miss.
    Only faces whose inward normal opposes the ray (dot(dir, inward_normal) < 0) are considered.
    """
    best_t = float("inf")
    best_hit = None
    eps = 1e-4

    ox, oy, oz = origin
    dx, dy, dz = direction

    # Floor: z = bottom_z, inward normal (0, 0, +1)
    if dz < -1e-8:
        t = (bottom_z - oz) / dz
        if 0.0 < t < best_t:
            hx = ox + t * dx
            hy = oy + t * dy
            if -hw - eps <= hx <= hw + eps and -hh - eps <= hy <= hh + eps:
                best_t = t
                best_hit = (0, hx, hy, bottom_z, t)

    # South wall: y = -hh, inward normal (0, +1, 0)
    if dy < -1e-8:
        t = (-hh - oy) / dy
        if 0.0 < t < best_t:
            hx = ox + t * dx
            hz = oz + t * dz
            if -hw - eps <= hx <= hw + eps and bottom_z - eps <= hz <= lip_z + eps:
                best_t = t
                best_hit = (1, hx, -hh, hz, t)

    # East wall: x = +hw, inward normal (-1, 0, 0)
    if dx > 1e-8:
        t = (hw - ox) / dx
        if 0.0 < t < best_t:
            hy = oy + t * dy
            hz = oz + t * dz
            if -hh - eps <= hy <= hh + eps and bottom_z - eps <= hz <= lip_z + eps:
                best_t = t
                best_hit = (2, hw, hy, hz, t)

    # North wall: y = +hh, inward normal (0, -1, 0)
    if dy > 1e-8:
        t = (hh - oy) / dy
        if 0.0 < t < best_t:
            hx = ox + t * dx
            hz = oz + t * dz
            if -hw - eps <= hx <= hw + eps and bottom_z - eps <= hz <= lip_z + eps:
                best_t = t
                best_hit = (3, hx, hh, hz, t)

    # West wall: x = -hw, inward normal (+1, 0, 0)
    if dx < -1e-8:
        t = (-hw - ox) / dx
        if 0.0 < t < best_t:
            hy = oy + t * dy
            hz = oz + t * dz
            if -hh - eps <= hy <= hh + eps and bottom_z - eps <= hz <= lip_z + eps:
                best_t = t
                best_hit = (4, -hw, hy, hz, t)

    return best_hit


def parse_args():
    parser = argparse.ArgumentParser(
        description="Verify submerged wall caustics, dry wall cleanliness, and waterline seam on a 24-bit BMP."
    )
    parser.add_argument("frame", help="Path to 24-bit BMP image")
    parser.add_argument(
        "--preset",
        choices=sorted(PRESETS.keys()),
        default="3",
        help="Camera preset matching app --preset (default: 3 = low corner inspection)",
    )
    parser.add_argument("--eye", nargs=3, type=float, metavar=("X", "Y", "Z"), help="Camera eye position")
    parser.add_argument("--center", nargs=3, type=float, metavar=("X", "Y", "Z"), help="Camera target position")
    parser.add_argument("--up", nargs=3, type=float, metavar=("X", "Y", "Z"), help="Camera up vector")
    parser.add_argument("--fov", type=float, default=None, help="Vertical FOV in degrees (default: 50)")
    parser.add_argument("--half-extent", nargs=2, type=float, default=[200.0, 200.0], metavar=("HW", "HH"))
    parser.add_argument("--bottom-z", type=float, default=-30.0)
    parser.add_argument("--lip-z", type=float, default=18.0)
    parser.add_argument("--submerged-z", nargs=2, type=float, default=[-28.0, -2.0], metavar=("ZMIN", "ZMAX"))
    parser.add_argument("--dry-z", nargs=2, type=float, default=[4.0, 16.0], metavar=("ZMIN", "ZMAX"))
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

    hw, hh = args.half_extent[0], args.half_extent[1]
    bottom_z = args.bottom_z
    lip_z = args.lip_z
    sub_zmin, sub_zmax = args.submerged_z[0], args.submerged_z[1]
    dry_zmin, dry_zmax = args.dry_z[0], args.dry_z[1]

    forward = normalize(sub(center, eye))
    right = normalize(cross(forward, up))
    if dot(right, right) < 1e-8:
        right = normalize(cross(forward, (0.0, 1.0, 0.0)))
    cam_up = cross(right, forward)
    focal = 1.0 / math.tan(math.radians(fov) * 0.5)
    aspect = width / float(height)

    white = 0
    submerged_lums = []
    dry_lums = []
    dry_spikes = 0
    seam_total = 0
    seam_sky_bleed = 0

    # Group submerged wall luminance by (face, world_z_bin) so purely horizontal
    # f(z) stone stripes have near-zero within-bin variance while real 2D wall
    # caustics show strong lateral luminance variance/contrast at constant depth.
    const_z_bins = defaultdict(list)

    corner_margin = 2.0

    for y in range(height):
        src_y = (height - 1 - y) if top_down else y
        row_off = pixel_off + src_y * stride
        ndc_y = ((y + 0.5) / height) * 2.0 - 1.0

        for x in range(width):
            b, g, r = data[row_off + x * 3 : row_off + x * 3 + 3]
            if r == 255 and g == 255 and b == 255:
                white += 1

            ndc_x = ((x + 0.5) / width) * 2.0 - 1.0
            vx = ndc_x * aspect / focal
            vy = ndc_y / focal
            direction = (
                right[0] * vx + cam_up[0] * vy + forward[0],
                right[1] * vx + cam_up[1] * vy + forward[1],
                right[2] * vx + cam_up[2] * vy + forward[2],
            )
            hit = intersect_pool_interior(eye, direction, hw, hh, bottom_z, lip_z)
            if hit is None:
                continue
            face, hx, hy, hz, _ = hit
            if face == 0:
                continue

            # Exclude exact wall-wall corners from statistical bands to avoid edge aliasing
            if face in (1, 3) and abs(hx) > hw - corner_margin:
                continue
            if face in (2, 4) and abs(hy) > hh - corner_margin:
                continue

            lum = 0.2126 * r + 0.7152 * g + 0.0722 * b

            if sub_zmin <= hz <= sub_zmax:
                submerged_lums.append(lum)
                z_bin = int(math.floor(hz * 2.0))  # 0.5-world-unit depth slices per wall face
                const_z_bins[(face, z_bin)].append(lum)
            elif dry_zmin <= hz <= dry_zmax:
                dry_lums.append(lum)
                if lum > 235.0 or max(r, g, b) >= 250:
                    dry_spikes += 1

            if -2.0 <= hz <= 4.0:
                seam_total += 1
                # Sky/clear background bleed signature: bright cool sky blue
                if b >= 180 and (b - r) >= 60 and (b - g) >= 15 and r < 140:
                    seam_sky_bleed += 1

    if len(submerged_lums) < 200 or len(dry_lums) < 100:
        print(f"insufficient wall pixels (submerged={len(submerged_lums)}, dry={len(dry_lums)})")
        sys.exit(2)

    sub_sorted = sorted(submerged_lums)
    dry_sorted = sorted(dry_lums)
    sub_std = stddev(submerged_lums)
    sub_p10 = percentile_sorted(sub_sorted, 10.0)
    sub_p95 = percentile_sorted(sub_sorted, 95.0)
    sub_contrast = sub_p95 - sub_p10

    slice_stds = []
    slice_contrasts = []
    for vals in const_z_bins.values():
        if len(vals) >= 25:
            slice_stds.append(stddev(vals))
            ordered = sorted(vals)
            slice_contrasts.append(percentile_sorted(ordered, 95.0) - percentile_sorted(ordered, 10.0))

    const_z_std = (sum(slice_stds) / len(slice_stds)) if slice_stds else 0.0
    const_z_contrast = (sum(slice_contrasts) / len(slice_contrasts)) if slice_contrasts else 0.0

    dry_std = stddev(dry_lums)
    dry_p99 = percentile_sorted(dry_sorted, 99.0)
    dry_spike_frac = dry_spikes / float(len(dry_lums))
    seam_sky_frac = (seam_sky_bleed / float(seam_total)) if seam_total > 0 else 0.0

    print("camera_eye", eye, "center", center, "up", up, "fov", fov)
    print("submerged_pixels", len(submerged_lums))
    print("submerged_std", round(sub_std, 3))
    print("submerged_p10", round(sub_p10, 2), "submerged_p95", round(sub_p95, 2), "contrast", round(sub_contrast, 2))
    print("const_z_slices", len(slice_stds), "const_z_std", round(const_z_std, 3), "const_z_contrast", round(const_z_contrast, 2))
    print("dry_pixels", len(dry_lums))
    print("dry_std", round(dry_std, 3), "dry_p99", round(dry_p99, 2), "dry_spike_frac", round(dry_spike_frac, 5))
    print("seam_pixels", seam_total, "seam_sky_bleed", seam_sky_bleed, "seam_sky_frac", round(seam_sky_frac, 5))
    print("white", white)

    ok = True
    if white != 0:
        print("FAIL: white-clipped pixels detected")
        ok = False
    if sub_std < 4.0 or sub_contrast < 12.0 or const_z_std < 3.5 or const_z_contrast < 10.0:
        print("FAIL: submerged wall lacks real 2D caustic luminance contrast/variance (flat or stripe-only)")
        ok = False
    if dry_spike_frac >= 0.005 or dry_p99 >= 235.0:
        print("FAIL: dry wall exhibits saturated caustic spikes")
        ok = False
    if seam_sky_frac >= 0.005:
        print("FAIL: background sky bleed detected along water-wall seam")
        ok = False

    if ok:
        print("wall caustics check: PASS")
        sys.exit(0)
    print("wall caustics check: FAIL")
    sys.exit(1)


if __name__ == "__main__":
    main()
