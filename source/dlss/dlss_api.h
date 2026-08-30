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

// Locked look copy: the Graphics row label is always "DLSS". Never null.
const wchar_t* FusionFixDLSS_GetMenuLabel(void);

// Graphics-tab extra-info GXT key, or null when the row should stay quiet.
const char* FusionFixDLSS_GetExtraInfoKey(void);

// English fallback for extra info. Never null; empty when there is nothing to say.
const wchar_t* FusionFixDLSS_GetExtraInfo(void);

// True when the user selected Balanced/Quality (SMAA/FXAA + FF motion blur must stay off).
int FusionFixDLSS_ShouldForcePostFxOff(void);

// Arm/consume grey draws for the AA and Motion Blur rows.
void FusionFixDLSS_ArmGreyDraw(int count);
int FusionFixDLSS_ConsumeGreyDraw(void);

int FusionFixDLSS_GetMode(void);
int FusionFixDLSS_GetStatus(void);
unsigned int FusionFixDLSS_GetNgxResult(void);

#ifdef __cplusplus
}

// Written by the ASI module; read by the C ABI above.
void FusionFixDLSS_StoreMenuState(int mode, int status, unsigned ngx, const wchar_t* label);
#endif
