// Host unit test for FusionFix DLSS math (no Windows / NGX required).
#include "../source/dlss/dlss_math.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

using namespace FusionFixDLSSMath;

static int g_fails = 0;

static void expect(bool cond, const char* msg)
{
    if (!cond)
    {
        std::fprintf(stderr, "FAIL: %s\n", msg);
        ++g_fails;
    }
}

static void expectNear(float a, float b, float eps, const char* msg)
{
    if (std::fabs(a - b) > eps)
    {
        std::fprintf(stderr, "FAIL: %s (got %f, expected %f)\n", msg, a, b);
        ++g_fails;
    }
}

int main()
{
    expectNear(Halton(1, 2), 0.5f, 1e-6f, "Halton(1,2)");
    expectNear(Halton(2, 2), 0.25f, 1e-6f, "Halton(2,2)");
    expectNear(Halton(1, 3), 1.0f / 3.0f, 1e-6f, "Halton(1,3)");

    const Vec2 j0 = Halton23Jitter(0);
    expect(std::fabs(j0.x) <= 0.5f && std::fabs(j0.y) <= 0.5f, "jitter in [-0.5,0.5]");
    const Vec2 j1 = Halton23Jitter(1);
    expect(j0.x != j1.x || j0.y != j1.y, "Halton sequence advances");

    expect(NgxPerfQualityForMode(1) == 1, "Balanced -> NGX Balanced");
    expect(NgxPerfQualityForMode(2) == 2, "Quality -> NGX MaxQuality");

    uint32_t rw = 0, rh = 0;
    RenderSizeForMode(2, 1920, 1080, rw, rh);
    expect(rw < 1920 && rh < 1080, "Quality render size is smaller");
    expect(rw >= 320 && rh >= 240, "Quality render size has a floor");
    RenderSizeForMode(1, 1920, 1080, rw, rh);
    expect(rw < 1920 && rw % 2 == 0, "Balanced render width even");

    Mat4 id = Mat4Identity();
    Mat4 inv = Mat4Inverse(id);
    expectNear(inv.m[0], 1.0f, 1e-5f, "identity inverse _11");
    expectNear(inv.m[15], 1.0f, 1e-5f, "identity inverse _44");

    Mat4 proj = Mat4Identity();
    proj.m[0] = 1.0f;
    proj.m[5] = 1.777f;
    proj.m[11] = 1.0f;
    proj.m[14] = -0.1f;
    expect(LooksLikePerspectiveProjection(proj.m), "perspective detect");

    Mat4 jittered = proj;
    ApplyProjectionJitter(jittered, 0.5f, -0.25f, 1920.0f, 1080.0f);
    expect(jittered.m[8] != proj.m[8] || jittered.m[9] != proj.m[9], "jitter mutates projection");

    // Static camera: reconstructed motion at the image center with identical VP is ~0.
    Mat4 vp = Mat4Identity();
    vp.m[0] = 1.2f;
    vp.m[5] = 1.2f;
    vp.m[10] = 1.0f;
    vp.m[15] = 1.0f;
    const Vec2 mv = ReconstructMotionPixels(0.5f, 0.5f, 0.5f, Mat4Inverse(vp), vp, 1920.0f, 1080.0f);
    expectNear(mv.x, 0.0f, 0.05f, "static camera MV.x ~ 0");
    expectNear(mv.y, 0.0f, 0.05f, "static camera MV.y ~ 0");

    if (g_fails)
    {
        std::fprintf(stderr, "%d test(s) failed\n", g_fails);
        return 1;
    }
    std::puts("dlss_math_test: all passed");
    return 0;
}
