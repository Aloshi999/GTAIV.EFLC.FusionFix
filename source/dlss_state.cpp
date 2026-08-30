#include "dlss/dlss_api.h"

#include <cstring>

static wchar_t g_label[96] = L"DLSS";
static int g_mode = 0;
static int g_status = FusionFixDLSSStatus_Idle;
static unsigned g_ngx = 0;

extern "C" const wchar_t* FusionFixDLSS_GetMenuLabel(void)
{
    return g_label;
}

extern "C" int FusionFixDLSS_ShouldForcePostFxOff(void)
{
    return g_mode != FusionFixDLSSMode_Off ? 1 : 0;
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
    if (label && label[0])
    {
        wcsncpy(g_label, label, 95);
        g_label[95] = 0;
    }
}
