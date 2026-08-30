#pragma once

// C ABI shared by the FusionFix ASI, d3d9 wrapper, and menu GXT hook.
// No NVIDIA feature DLLs are shipped. NGX is loaded from the installed driver.

#ifdef __cplusplus
extern "C" {
#endif

enum FusionFixDLSSMode
{
    FusionFixDLSSMode_Off = 0,
    FusionFixDLSSMode_Balanced = 1,
    FusionFixDLSSMode_Quality = 2,
};

enum FusionFixDLSSStatus
{
    FusionFixDLSSStatus_Idle = 0,
    FusionFixDLSSStatus_Ok = 1,
    FusionFixDLSSStatus_NoVulkan = 2,
    FusionFixDLSSStatus_NoNgx = 3,
    FusionFixDLSSStatus_NgxError = 4,
    FusionFixDLSSStatus_NoSwapchain = 5,
};

// Menu label for the DLSS row. Never null. May be "DLSS" or "DLSS: no Vulkan" / "DLSS: NGX 0x…".
const wchar_t* FusionFixDLSS_GetMenuLabel(void);

// True when the user selected Balanced/Quality (SMAA/FXAA + FF motion blur must stay off).
int FusionFixDLSS_ShouldForcePostFxOff(void);

int FusionFixDLSS_GetMode(void);
int FusionFixDLSS_GetStatus(void);
unsigned int FusionFixDLSS_GetNgxResult(void);

#ifdef __cplusplus
}

// Written by the ASI module; read by the C ABI above.
void FusionFixDLSS_StoreMenuState(int mode, int status, unsigned ngx, const wchar_t* label);
#endif
