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

    float catmullRomDeriv(float p0, float p1, float p2, float p3, float t) {
        float t2 = t * t;
        return 0.5 * (
            (-p0 + p2) +
            2.0 * (2.0 * p0 - 5.0 * p1 + 4.0 * p2 - p3) * t +
            3.0 * (-p0 + 3.0 * p1 - 3.0 * p2 + p3) * t2);
    }

    float tapHeight(float xIndex, float yIndex, vec2 lo, vec2 hi) {
        vec2 tc = vec2((xIndex + 0.5) / gridSize.x, (yIndex + 0.5) / gridSize.y);
        return texture(heightTex, clamp(tc, lo, hi)).r;
    }

    void sampleHeightAndGrad(vec2 xy, out float h, out vec2 grad) {
        vec2 halfTexel = 0.5 / gridSize;
        vec2 norm = clamp((xy + poolHalfExtent) / (poolHalfExtent * 2.0), vec2(0.0), vec2(1.0));
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

        float s00 = tapHeight(x0, y0, lo, hi);
        float s10 = tapHeight(x1, y0, lo, hi);
        float s20 = tapHeight(x2, y0, lo, hi);
        float s30 = tapHeight(x3, y0, lo, hi);
        float r0 = catmullRom(s00, s10, s20, s30, t.x);
        float dr0 = catmullRomDeriv(s00, s10, s20, s30, t.x);

        float s01 = tapHeight(x0, y1, lo, hi);
        float s11 = tapHeight(x1, y1, lo, hi);
        float s21 = tapHeight(x2, y1, lo, hi);
        float s31 = tapHeight(x3, y1, lo, hi);
        float r1 = catmullRom(s01, s11, s21, s31, t.x);
        float dr1 = catmullRomDeriv(s01, s11, s21, s31, t.x);

        float s02 = tapHeight(x0, y2, lo, hi);
        float s12 = tapHeight(x1, y2, lo, hi);
        float s22 = tapHeight(x2, y2, lo, hi);
        float s32 = tapHeight(x3, y2, lo, hi);
        float r2 = catmullRom(s02, s12, s22, s32, t.x);
        float dr2 = catmullRomDeriv(s02, s12, s22, s32, t.x);

        float s03 = tapHeight(x0, y3, lo, hi);
        float s13 = tapHeight(x1, y3, lo, hi);
        float s23 = tapHeight(x2, y3, lo, hi);
        float s33 = tapHeight(x3, y3, lo, hi);
        float r3 = catmullRom(s03, s13, s23, s33, t.x);
        float dr3 = catmullRomDeriv(s03, s13, s23, s33, t.x);

        h = catmullRom(r0, r1, r2, r3, t.y);
        float dhDix = catmullRom(dr0, dr1, dr2, dr3, t.y);
        float dhDiy = catmullRomDeriv(r0, r1, r2, r3, t.y);
        vec2 indexPerWorld = (gridSize - vec2(1.0)) / (poolHalfExtent * 2.0);
        grad = vec2(dhDix, dhDiy) * indexPerWorld;
    }

    float sampleHeight(vec2 xy) {
        vec2 halfTexel = 0.5 / gridSize;
        vec2 norm = clamp((xy + poolHalfExtent) / (poolHalfExtent * 2.0), vec2(0.0), vec2(1.0));
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
        float h;
        vec2 grad;
        sampleHeightAndGrad(xy, h, grad);
        dhdx = grad.x;
        dhdy = grad.y;
    }
)"
