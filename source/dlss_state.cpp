#include "dlss/dlss_api.h"
#include "dlss/dlss_menu.h"

#include <cstring>
#include <cwchar>

static wchar_t g_label[96] = L"DLSS";
static int g_mode = 0;
static int g_status = FusionFixDLSSStatus_Idle;
static unsigned g_ngx = 0;
static int g_greyDraws = 0;

extern "C" const wchar_t* FusionFixDLSS_GetMenuLabel(void)
{
    return L"DLSS";
}

extern "C" const char* FusionFixDLSS_GetExtraInfoKey(void)
{
    return FusionFixDLSS_ExtraInfoKeyForStatus(g_status);
}

extern "C" const wchar_t* FusionFixDLSS_GetExtraInfo(void)
{
    return FusionFixDLSS_ExtraInfoFallbackForStatus(g_status);
}

extern "C" int FusionFixDLSS_ShouldForcePostFxOff(void)
{
    return g_mode != FusionFixDLSSMode_Off ? 1 : 0;
}

extern "C" void FusionFixDLSS_ArmGreyDraw(int count)
{
    g_greyDraws = count;
}

extern "C" int FusionFixDLSS_ConsumeGreyDraw(void)
{
    if (g_greyDraws <= 0)
        return 0;
    --g_greyDraws;
    return 1;
}

extern "C" int FusionFixDLSS_GetMode(void)
{
    return g_mode;
}

extern "C" int FusionFixDLSS_GetStatus(void)
{
    return g_status;
}

extern "C" unsigned int FusionFixDLSS_GetNgxResult(void)
{
    return g_ngx;
}

void FusionFixDLSS_StoreMenuState(int mode, int status, unsigned ngx, const wchar_t* label)
{
    g_mode = mode;
    g_status = status;
    g_ngx = ngx;
    (void)label;
    wcsncpy(g_label, L"DLSS", 95);
    g_label[95] = 0;
}
