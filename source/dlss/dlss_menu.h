#pragma once

// Host-safe menu copy helpers. No NVIDIA DLLs, no Frame Gen.

enum FusionFixDLSSExtraInfo
{
    FusionFixDLSSExtraInfo_None = 0,
    FusionFixDLSSExtraInfo_NoVulkan = 1,
    FusionFixDLSSExtraInfo_NoNgx = 2,
    FusionFixDLSSExtraInfo_NoSwapchain = 3,
    FusionFixDLSSExtraInfo_NgxError = 4,
};

inline const char* FusionFixDLSS_LookCopyLabel(void)
{
    return "DLSS";
}

inline int FusionFixDLSS_IsLockedValueKey(const char* key)
{
    if (!key)
        return 0;
    return (key[0] == 'M' && key[1] == 'O' && key[2] == '_' && key[3] == 'O' && key[4] == 'F' && key[5] == 'F' && key[6] == 0)
        || (key[0] == 'D' && key[1] == 'L' && key[2] == 'S' && key[3] == 'S' && key[4] == '_' && key[5] == 'B' && key[6] == 'A' && key[7] == 'L' && key[8] == 0)
        || (key[0] == 'D' && key[1] == 'L' && key[2] == 'S' && key[3] == 'S' && key[4] == '_' && key[5] == 'Q' && key[6] == 'U' && key[7] == 'A' && key[8] == 'L' && key[9] == 0);
}

inline int FusionFixDLSS_IsForbiddenValueKey(const char* key)
{
    if (!key)
        return 0;
    // Locked look copy: Off / Balanced / Quality only. No UltraPerf, Auto, FG, DLAA.
    if (key[0] == 'D' && key[1] == 'L' && key[2] == 'A' && key[3] == 'A' && key[4] == 0)
        return 1;
    const char* banned[] = {
        "DLSS_UPERF", "DLSS_ULTRA", "DLSS_PERF", "DLSS_AUTO", "DLSS_FG",
        "DLSS_DLAA", "ULTRAPERF", "UltraPerf", "FrameGen", "FRAMEGEN", "AUTO"
    };
    for (unsigned i = 0; i < sizeof(banned) / sizeof(banned[0]); ++i)
    {
        const char* a = key;
        const char* b = banned[i];
        int same = 1;
        while (*a || *b)
        {
            if (*a != *b)
            {
                same = 0;
                break;
            }
            ++a;
            ++b;
        }
        if (same)
            return 1;
    }
    return 0;
}

inline int FusionFixDLSS_IsGreyedPostFxKey(const char* key)
{
    if (!key)
        return 0;
    if (key[0] == 'A' && key[1] == 'n' && key[2] == 't' && key[3] == 'i' && key[4] == 'a'
        && key[5] == 'l' && key[6] == 'i' && key[7] == 'a' && key[8] == 's' && key[9] == 'i'
        && key[10] == 'n' && key[11] == 'g' && key[12] == 0)
        return 1;
    if (key[0] == 'M' && key[1] == 'o' && key[2] == 't' && key[3] == 'i' && key[4] == 'o'
        && key[5] == 'n' && key[6] == ' ' && key[7] == 'B' && key[8] == 'l' && key[9] == 'u'
        && key[10] == 'r' && key[11] == 0)
        return 1;
    if (key[0] == 'F' && key[1] == 'X' && key[2] == 'A' && key[3] == 'A' && key[4] == 0)
        return 1;
    if (key[0] == 'S' && key[1] == 'M' && key[2] == 'A' && key[3] == 'A' && key[4] == 0)
        return 1;
    return 0;
}

inline const char* FusionFixDLSS_ExtraInfoKeyForStatus(int status)
{
    // FusionFixDLSSStatus_* values from dlss_api.h
    switch (status)
    {
    case 2: return "FF_DLSS_NOVULKAN";
    case 3: return "FF_DLSS_NONGX";
    case 5: return "FF_DLSS_NOSWAP";
    case 4: return "FF_DLSS_NGXERR";
    default: return nullptr;
    }
}

inline const wchar_t* FusionFixDLSS_ExtraInfoFallbackForStatus(int status)
{
    switch (status)
    {
    case 2: return L"~r~DLSS needs Graphics API set to Vulkan, then a restart.";
    case 3: return L"~r~DLSS needs an NVIDIA driver with NGX.";
    case 5: return L"~r~DLSS could not bind the Vulkan swapchain.";
    case 4: return L"~r~DLSS NGX error.";
    default: return L"";
    }
}
