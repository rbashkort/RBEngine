#include <cmath>
#include <cfloat>
#include "components.h"

struct Vec2 { float x, y; };

// === Math ===
inline float dot(Vec2 a, Vec2 b) { return a.x * b.x + a.y * b.y; }
inline float lenSq(Vec2 a) { return a.x*a.x + a.y*a.y; }
inline float len(Vec2 a) { return sqrtf(lenSq(a)); }
inline Vec2 normalize(Vec2 a) { float l = len(a); return l > 1e-8f ? Vec2{a.x/l, a.y/l} : Vec2{0,0}; }
inline Vec2 sub(Vec2 a, Vec2 b) { return {a.x - b.x, a.y - b.y}; }
inline Vec2 add(Vec2 a, Vec2 b) { return {a.x + b.x, a.y + b.y}; }
inline Vec2 mul(Vec2 a, float s) { return {a.x * s, a.y * s}; }
inline Vec2 perp(Vec2 a) { return {-a.y, a.x}; }

inline Vec2 rotate(Vec2 v, float angleDeg) {
    float rad = angleDeg * 0.0174532925f;
    float c = cosf(rad), s = sinf(rad);
    return { v.x*c - v.y*s, v.x*s + v.y*c };
}

struct PolyVerts {
    Vec2 data[4];
    int count;
};

PolyVerts getVertices(const E_Transform& t, const E_Collider& c) {
    PolyVerts result;
    result.count = 0;

    float cx = t.x + c.offsetX;
    float cy = t.y + c.offsetY;

    if (c.type == ColliderType::Rect) {
        float hw = c.width * 0.5f;
        float hh = c.height * 0.5f;
        result.data[0] = add({cx, cy}, rotate({-hw, -hh}, t.angle));
        result.data[1] = add({cx, cy}, rotate({ hw, -hh}, t.angle));
        result.data[2] = add({cx, cy}, rotate({ hw,  hh}, t.angle));
        result.data[3] = add({cx, cy}, rotate({-hw,  hh}, t.angle));
        result.count = 4;
    }
    else if (c.type == ColliderType::Triangle) {
        float hw = c.width * 0.5f;
        float hh = c.height * 0.5f;
        result.data[0] = add({cx, cy}, rotate({   0, -hh}, t.angle));
        result.data[1] = add({cx, cy}, rotate({-hw,  hh}, t.angle));
        result.data[2] = add({cx, cy}, rotate({ hw,  hh}, t.angle));
        result.count = 3;
    }
    return result;
}

void projectPoly(const Vec2* verts, int count, Vec2 axis, float& minOut, float& maxOut) {
    minOut = maxOut = dot(verts[0], axis);
    for (int i = 1; i < count; ++i) {
        float p = dot(verts[i], axis);
        if (p < minOut) minOut = p;
        if (p > maxOut) maxOut = p;
    }
}

bool satPolyPoly(const Vec2* vertsA, int countA, const Vec2* vertsB, int countB, Vec2& mtv) {
    float overlap = FLT_MAX;
    Vec2 smallestAxis = {0, 0};

    auto testAxes = [&](const Vec2* shape, int shapeCount) -> bool {
        for (int i = 0; i < shapeCount; ++i) {
            Vec2 p1 = shape[i];
            Vec2 p2 = shape[(i + 1) % shapeCount];
            Vec2 edge = sub(p2, p1);
            Vec2 axis = normalize(perp(edge));

            if (lenSq(axis) < 1e-8f) continue; // вырожденная грань

            float minA, maxA, minB, maxB;
            projectPoly(vertsA, countA, axis, minA, maxA);
            projectPoly(vertsB, countB, axis, minB, maxB);

            if (maxA < minB || maxB < minA) return false; // разделение найдено

            float o = fminf(maxA, maxB) - fmaxf(minA, minB);
            if (o < overlap) {
                overlap = o;
                smallestAxis = axis;
            }
        }
        return true;
    };

    if (!testAxes(vertsA, countA)) return false;
    if (!testAxes(vertsB, countB)) return false;

    Vec2 centerA = {0,0}; for (int i=0; i<countA; ++i) centerA = add(centerA, vertsA[i]);
    centerA = mul(centerA, 1.0f / countA);

    Vec2 centerB = {0,0}; for (int i=0; i<countB; ++i) centerB = add(centerB, vertsB[i]);
    centerB = mul(centerB, 1.0f / countB);

    Vec2 dir = sub(centerB, centerA);
    if (dot(dir, smallestAxis) < 0) smallestAxis = mul(smallestAxis, -1);

    mtv = mul(smallestAxis, overlap);
    return true;
}

bool satCirclePoly(Vec2 center, float radius, const Vec2* verts, int count, Vec2& mtv) {
    float overlap = FLT_MAX;
    Vec2 smallestAxis = {0, 0};

    for (int i = 0; i < count; ++i) {
        Vec2 p1 = verts[i];
        Vec2 p2 = verts[(i + 1) % count];
        Vec2 axis = normalize(perp(sub(p2, p1)));

        if (lenSq(axis) < 1e-8f) continue;

        float minP, maxP;
        projectPoly(verts, count, axis, minP, maxP);

        float projC = dot(center, axis);
        float minC = projC - radius;
        float maxC = projC + radius;

        if (maxP < minC || maxC < minP) return false;

        float o = fminf(maxP, maxC) - fmaxf(minP, minC);
        if (o < overlap) {
            overlap = o;
            smallestAxis = axis;
        }
    }

    Vec2 closestVert = verts[0];
    float minDst = lenSq(sub(center, verts[0]));
    for (int i = 1; i < count; ++i) {
        float d = lenSq(sub(center, verts[i]));
        if (d < minDst) {
            minDst = d;
            closestVert = verts[i];
        }
    }

    Vec2 axis = normalize(sub(center, closestVert));
    if (lenSq(axis) > 1e-8f) {
        float minP, maxP;
        projectPoly(verts, count, axis, minP, maxP);

        float projC = dot(center, axis);
        float minC = projC - radius;
        float maxC = projC + radius;

        if (maxP < minC || maxC < minP) return false;

        float o = fminf(maxP, maxC) - fmaxf(minP, minC);
        if (o < overlap) {
            overlap = o;
            smallestAxis = axis;
        }
    }

    Vec2 centerPoly = {0,0};
    for (int i = 0; i < count; ++i) centerPoly = add(centerPoly, verts[i]);
    centerPoly = mul(centerPoly, 1.0f / count);

    Vec2 dir = sub(center, centerPoly);
    if (dot(dir, smallestAxis) < 0) smallestAxis = mul(smallestAxis, -1);

    mtv = mul(smallestAxis, overlap);
    return true;
}

bool satCircleCircle(Vec2 c1, float r1, Vec2 c2, float r2, Vec2& mtv) {
    Vec2 d = sub(c2, c1);
    float distSq = lenSq(d);
    float rSum = r1 + r2;

    if (distSq >= rSum * rSum) return false;

    float dist = sqrtf(distSq);
    if (dist < 1e-6f) {
        mtv = {rSum, 0};
        return true;
    }

    float overlap = rSum - dist;
    Vec2 n = mul(d, 1.0f / dist);
    mtv = mul(n, overlap);
    return true;
}

