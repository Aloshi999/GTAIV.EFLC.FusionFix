#pragma once

// Shared DLSS math (Halton jitter, matrices, first-cut motion-vector reconstruct).
// Header-only so it can be unit-tested with host g++ and compiled into the ASI.

#ifdef __cplusplus

#include <cmath>
#include <cstdint>
#include <cstring>

namespace FusionFixDLSSMath
{
    struct Vec2
    {
        float x, y;
    };

    struct Vec4
    {
        float x, y, z, w;
    };

    struct Mat4
    {
        float m[16]; // row-major, D3D v' = v * M
    };

    inline float Halton(int index, int base)
    {
        float f = 1.0f;
        float result = 0.0f;
        int i = index;
        while (i > 0)
        {
            f /= static_cast<float>(base);
            result += f * static_cast<float>(i % base);
            i /= base;
        }
        return result;
    }

    // DLSS / TAA Halton(2,3) sequence, centered in [-0.5, 0.5] pixels.
    inline Vec2 Halton23Jitter(uint32_t frameIndex)
    {
        const int index = static_cast<int>((frameIndex % 16u) + 1u);
        Vec2 j;
        j.x = Halton(index, 2) - 0.5f;
        j.y = Halton(index, 3) - 0.5f;
        return j;
    }

    inline Mat4 Mat4Identity()
    {
        Mat4 r{};
        r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.0f;
        return r;
    }

    inline Mat4 Mat4Multiply(const Mat4& a, const Mat4& b)
    {
        Mat4 r{};
        for (int row = 0; row < 4; ++row)
        {
            for (int col = 0; col < 4; ++col)
            {
                float s = 0.0f;
                for (int k = 0; k < 4; ++k)
                    s += a.m[row * 4 + k] * b.m[k * 4 + col];
                r.m[row * 4 + col] = s;
            }
        }
        return r;
    }

    inline Mat4 Mat4Inverse(const Mat4& in)
    {
        const float* m = in.m;
        Mat4 out{};
        float* o = out.m;

        o[0] = m[5] * m[10] * m[15] - m[5] * m[11] * m[14] - m[9] * m[6] * m[15] + m[9] * m[7] * m[14] + m[13] * m[6] * m[11] - m[13] * m[7] * m[10];
        o[4] = -m[4] * m[10] * m[15] + m[4] * m[11] * m[14] + m[8] * m[6] * m[15] - m[8] * m[7] * m[14] - m[12] * m[6] * m[11] + m[12] * m[7] * m[10];
        o[8] = m[4] * m[9] * m[15] - m[4] * m[11] * m[13] - m[8] * m[5] * m[15] + m[8] * m[7] * m[13] + m[12] * m[5] * m[11] - m[12] * m[7] * m[9];
        o[12] = -m[4] * m[9] * m[14] + m[4] * m[10] * m[13] + m[8] * m[5] * m[14] - m[8] * m[6] * m[13] - m[12] * m[5] * m[10] + m[12] * m[6] * m[9];
        o[1] = -m[1] * m[10] * m[15] + m[1] * m[11] * m[14] + m[9] * m[2] * m[15] - m[9] * m[3] * m[14] - m[13] * m[2] * m[11] + m[13] * m[3] * m[10];
        o[5] = m[0] * m[10] * m[15] - m[0] * m[11] * m[14] - m[8] * m[2] * m[15] + m[8] * m[3] * m[14] + m[12] * m[2] * m[11] - m[12] * m[3] * m[10];
        o[9] = -m[0] * m[9] * m[15] + m[0] * m[11] * m[13] + m[8] * m[1] * m[15] - m[8] * m[3] * m[13] - m[12] * m[1] * m[11] + m[12] * m[3] * m[9];
        o[13] = m[0] * m[9] * m[14] - m[0] * m[10] * m[13] - m[8] * m[1] * m[14] + m[8] * m[2] * m[13] + m[12] * m[1] * m[10] - m[12] * m[2] * m[9];
        o[2] = m[1] * m[6] * m[15] - m[1] * m[7] * m[14] - m[5] * m[2] * m[15] + m[5] * m[3] * m[14] + m[13] * m[2] * m[7] - m[13] * m[3] * m[6];
        o[6] = -m[0] * m[6] * m[15] + m[0] * m[7] * m[14] + m[4] * m[2] * m[15] - m[4] * m[3] * m[14] - m[12] * m[2] * m[7] + m[12] * m[3] * m[6];
        o[10] = m[0] * m[5] * m[15] - m[0] * m[7] * m[13] - m[4] * m[1] * m[15] + m[4] * m[3] * m[13] + m[12] * m[1] * m[7] - m[12] * m[3] * m[5];
        o[14] = -m[0] * m[5] * m[14] + m[0] * m[6] * m[13] + m[4] * m[1] * m[14] - m[4] * m[2] * m[13] - m[12] * m[1] * m[6] + m[12] * m[2] * m[5];
        o[3] = -m[1] * m[6] * m[11] + m[1] * m[7] * m[10] + m[5] * m[2] * m[11] - m[5] * m[3] * m[10] - m[9] * m[2] * m[7] + m[9] * m[3] * m[6];
        o[7] = m[0] * m[6] * m[11] - m[0] * m[7] * m[10] - m[4] * m[2] * m[11] + m[4] * m[3] * m[10] + m[8] * m[2] * m[7] - m[8] * m[3] * m[6];
        o[11] = -m[0] * m[5] * m[11] + m[0] * m[7] * m[9] + m[4] * m[1] * m[11] - m[4] * m[3] * m[9] - m[8] * m[1] * m[7] + m[8] * m[3] * m[5];
        o[15] = m[0] * m[5] * m[10] - m[0] * m[6] * m[9] - m[4] * m[1] * m[10] + m[4] * m[2] * m[9] + m[8] * m[1] * m[6] - m[8] * m[2] * m[5];

        const float det = m[0] * o[0] + m[1] * o[4] + m[2] * o[8] + m[3] * o[12];
        if (std::fabs(det) < 1e-12f)
            return Mat4Identity();

        const float invDet = 1.0f / det;
        for (int i = 0; i < 16; ++i)
            o[i] *= invDet;
        return out;
    }

    inline Vec4 Mat4MulVec4(const Mat4& a, const Vec4& v)
    {
        Vec4 r;
        r.x = a.m[0] * v.x + a.m[1] * v.y + a.m[2] * v.z + a.m[3] * v.w;
        r.y = a.m[4] * v.x + a.m[5] * v.y + a.m[6] * v.z + a.m[7] * v.w;
        r.z = a.m[8] * v.x + a.m[9] * v.y + a.m[10] * v.z + a.m[11] * v.w;
        r.w = a.m[12] * v.x + a.m[13] * v.y + a.m[14] * v.z + a.m[15] * v.w;
        return r;
    }

    // Apply Halton pixel jitter to a D3D row-major perspective projection (v' = v * M).
    inline void ApplyProjectionJitter(Mat4& proj, float jitterX, float jitterY, float renderWidth, float renderHeight)
    {
        if (renderWidth <= 0.0f || renderHeight <= 0.0f)
            return;
        proj.m[8] += (jitterX * 2.0f) / renderWidth;   // _31
        proj.m[9] += (jitterY * 2.0f) / renderHeight;  // _32
    }

    inline bool LooksLikePerspectiveProjection(const float* m16)
    {
        if (!m16)
            return false;
        // Perspective: W row carries a non-zero Z term; last column typically (0,0,1,0) or (0,0,-1,0).
        const float wFromZ = m16[11];
        const float zFromW = m16[14];
        if (std::fabs(wFromZ) < 0.5f && std::fabs(m16[15]) > 0.5f)
            return false; // ortho-ish
        if (std::fabs(m16[0]) < 1e-6f || std::fabs(m16[5]) < 1e-6f)
            return false;
        return (std::fabs(wFromZ) > 0.5f) || (std::fabs(zFromW) > 1e-4f);
    }

    // Reconstruct world position from D3D-style depth (0..1) and current view-proj, then
    // project with the previous view-proj. Motion is in pixel space (NGX default).
    inline Vec2 ReconstructMotionPixels(float uvX, float uvY, float depth,
                                        const Mat4& invViewProj, const Mat4& prevViewProj,
                                        float renderWidth, float renderHeight)
    {
        Vec4 clip;
        clip.x = uvX * 2.0f - 1.0f;
        clip.y = 1.0f - uvY * 2.0f; // D3D Y-down UV -> clip Y-up
        clip.z = depth;
        clip.w = 1.0f;

        Vec4 world = Mat4MulVec4(invViewProj, clip);
        if (std::fabs(world.w) < 1e-8f)
            return Vec2{ 0.0f, 0.0f };
        world.x /= world.w;
        world.y /= world.w;
        world.z /= world.w;
        world.w = 1.0f;

        Vec4 prev = Mat4MulVec4(prevViewProj, world);
        if (std::fabs(prev.w) < 1e-8f)
            return Vec2{ 0.0f, 0.0f };
        prev.x /= prev.w;
        prev.y /= prev.w;

        const float curPx = (clip.x * 0.5f + 0.5f) * renderWidth;
        const float curPy = (1.0f - (clip.y * 0.5f + 0.5f)) * renderHeight;
        const float prevPx = (prev.x * 0.5f + 0.5f) * renderWidth;
        const float prevPy = (1.0f - (prev.y * 0.5f + 0.5f)) * renderHeight;
        return Vec2{ prevPx - curPx, prevPy - curPy };
    }

    // Quality = 1.50x, Balanced = 1.70x (NVIDIA Super Resolution scales).
    inline void RenderSizeForMode(int mode, uint32_t displayW, uint32_t displayH, uint32_t& renderW, uint32_t& renderH)
    {
        float scale = 1.0f;
        if (mode == 1)      // Balanced
            scale = 1.0f / 1.7f;
        else if (mode == 2) // Quality
            scale = 1.0f / 1.5f;

        renderW = static_cast<uint32_t>(std::floor(static_cast<float>(displayW) * scale + 0.5f));
        renderH = static_cast<uint32_t>(std::floor(static_cast<float>(displayH) * scale + 0.5f));
        if (renderW < 320)
            renderW = 320;
        if (renderH < 240)
            renderH = 240;
        if (renderW > displayW)
            renderW = displayW;
        if (renderH > displayH)
            renderH = displayH;
        renderW &= ~1u;
        renderH &= ~1u;
    }

    inline int NgxPerfQualityForMode(int mode)
    {
        // NVSDK_NGX_PerfQuality_Value_Balanced = 1, MaxQuality = 2
        if (mode == 1)
            return 1;
        if (mode == 2)
            return 2;
        return 1;
    }
}

#endif
