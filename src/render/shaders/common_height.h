#pragma once

#define WAVE_HEIGHT_SAMPLE R"(
    float catmullRom(float p0, float p1, float p2, float p3, float t) {
        float t2 = t * t;
        float t3 = t2 * t;
        return 0.5 * ((2.0 * p1) +
            (-p0 + p2) * t +
            (2.0 * p0 - 5.0 * p1 + 4.0 * p2 - p3) * t2 +
            (-p0 + 3.0 * p1 - 3.0 * p2 + p3) * t3);
    }

    float tapHeight(float xIndex, float yIndex, vec2 lo, vec2 hi) {
        vec2 tc = vec2((xIndex + 0.5) / gridSize.x, (yIndex + 0.5) / gridSize.y);
        return texture(heightTex, clamp(tc, lo, hi)).r;
    }

    float sampleHeight(vec2 xy) {
        vec2 halfTexel = 0.5 / gridSize;
        vec2 norm = (xy + poolHalfExtent) / (poolHalfExtent * 2.0);
        vec2 index = norm * (gridSize - vec2(1.0));
        vec2 cell = floor(index);
        vec2 t = index - cell;
        vec2 lo = halfTexel;
        vec2 hi = vec2(1.0) - halfTexel;
        float x0 = cell.x - 1.0;
        float x1 = cell.x;
        float x2 = cell.x + 1.0;
        float x3 = cell.x + 2.0;
        float y0 = cell.y - 1.0;
        float y1 = cell.y;
        float y2 = cell.y + 1.0;
        float y3 = cell.y + 2.0;
        float r0 = catmullRom(
            tapHeight(x0, y0, lo, hi), tapHeight(x1, y0, lo, hi),
            tapHeight(x2, y0, lo, hi), tapHeight(x3, y0, lo, hi), t.x);
        float r1 = catmullRom(
            tapHeight(x0, y1, lo, hi), tapHeight(x1, y1, lo, hi),
            tapHeight(x2, y1, lo, hi), tapHeight(x3, y1, lo, hi), t.x);
        float r2 = catmullRom(
            tapHeight(x0, y2, lo, hi), tapHeight(x1, y2, lo, hi),
            tapHeight(x2, y2, lo, hi), tapHeight(x3, y2, lo, hi), t.x);
        float r3 = catmullRom(
            tapHeight(x0, y3, lo, hi), tapHeight(x1, y3, lo, hi),
            tapHeight(x2, y3, lo, hi), tapHeight(x3, y3, lo, hi), t.x);
        return catmullRom(r0, r1, r2, r3, t.y);
    }

    void heightSlope(vec2 xy, out float dhdx, out float dhdy) {
        float hxp = sampleHeight(xy + vec2(0.5, 0.0));
        float hxm = sampleHeight(xy - vec2(0.5, 0.0));
        float hyp = sampleHeight(xy + vec2(0.0, 0.5));
        float hym = sampleHeight(xy - vec2(0.0, 0.5));
        dhdx = (hxp - hxm) / 1.0;
        dhdy = (hyp - hym) / 1.0;
    }
)"
