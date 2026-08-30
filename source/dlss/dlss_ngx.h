#pragma once

// Driver NGX Super Resolution ABI. Function names and parameter keys match the
// public NVIDIA NGX C interface. We load nvngx.dll from the installed driver
// only — this tree never vendors nvngx_dlss / Frame Gen / Streamline binaries.

#include "dlss_vk.h"

#include <cstdint>

#ifdef _WIN32
#define NVSDK_CONV __cdecl
#else
#define NVSDK_CONV
#endif

typedef enum NVSDK_NGX_Version { NVSDK_NGX_Version_API = 0x0000015 } NVSDK_NGX_Version;

typedef enum NVSDK_NGX_Result {
    NVSDK_NGX_Result_Success = 0x1,
    NVSDK_NGX_Result_Fail = 0xBAD00000
} NVSDK_NGX_Result;

#define NVSDK_NGX_FAILED(value) ((((unsigned int)(value)) & 0xFFF00000u) == (unsigned int)NVSDK_NGX_Result_Fail)
#define NVSDK_NGX_SUCCEED(value) (!NVSDK_NGX_FAILED(value))

typedef enum NVSDK_NGX_Feature {
    NVSDK_NGX_Feature_SuperSampling = 1
} NVSDK_NGX_Feature;

typedef enum NVSDK_NGX_EngineType {
    NVSDK_NGX_ENGINE_TYPE_CUSTOM = 0
} NVSDK_NGX_EngineType;

typedef enum NVSDK_NGX_Resource_VK_Type {
    NVSDK_NGX_RESOURCE_VK_TYPE_VK_IMAGEVIEW = 0,
    NVSDK_NGX_RESOURCE_VK_TYPE_VK_BUFFER = 1
} NVSDK_NGX_Resource_VK_Type;

typedef enum NVSDK_NGX_PerfQuality_Value {
    NVSDK_NGX_PerfQuality_Value_MaxPerf = 0,
    NVSDK_NGX_PerfQuality_Value_Balanced = 1,
    NVSDK_NGX_PerfQuality_Value_MaxQuality = 2,
    NVSDK_NGX_PerfQuality_Value_UltraPerformance = 3,
    NVSDK_NGX_PerfQuality_Value_UltraQuality = 4,
    NVSDK_NGX_PerfQuality_Value_DLAA = 5
} NVSDK_NGX_PerfQuality_Value;

typedef struct NVSDK_NGX_Handle NVSDK_NGX_Handle;
typedef struct NVSDK_NGX_Parameter NVSDK_NGX_Parameter;
typedef struct NVSDK_NGX_FeatureCommonInfo_Internal NVSDK_NGX_FeatureCommonInfo_Internal;

typedef struct NVSDK_NGX_PathListInfo {
    const wchar_t** Path;
    unsigned int Length;
} NVSDK_NGX_PathListInfo;

typedef struct NVSDK_NGX_LoggingInfo {
    void* LoggingCallback;
    unsigned int MinimumLoggingLevel;
    bool DisableOtherLoggingSinks;
} NVSDK_NGX_LoggingInfo;

typedef struct NVSDK_NGX_FeatureCommonInfo {
    NVSDK_NGX_PathListInfo PathListInfo;
    NVSDK_NGX_FeatureCommonInfo_Internal* InternalData;
    NVSDK_NGX_LoggingInfo LoggingInfo;
} NVSDK_NGX_FeatureCommonInfo;

typedef struct NVSDK_NGX_ImageViewInfo_VK {
    VkImageView ImageView;
    VkImage Image;
    VkImageSubresourceRange SubresourceRange;
    VkFormat Format;
    unsigned int Width;
    unsigned int Height;
} NVSDK_NGX_ImageViewInfo_VK;

typedef struct NVSDK_NGX_BufferInfo_VK {
    VkBuffer Buffer;
    unsigned int SizeInBytes;
} NVSDK_NGX_BufferInfo_VK;

typedef struct NVSDK_NGX_Resource_VK {
    union {
        NVSDK_NGX_ImageViewInfo_VK ImageViewInfo;
        NVSDK_NGX_BufferInfo_VK BufferInfo;
    } Resource;
    NVSDK_NGX_Resource_VK_Type Type;
    bool ReadWrite;
} NVSDK_NGX_Resource_VK;

#ifdef _MSC_VER
struct NVSDK_NGX_Parameter; // opaque; driver C helpers or vtable in the ASI
#endif

#define NVSDK_NGX_Parameter_Width "Width"
#define NVSDK_NGX_Parameter_Height "Height"
#define NVSDK_NGX_Parameter_OutWidth "OutWidth"
#define NVSDK_NGX_Parameter_OutHeight "OutHeight"
#define NVSDK_NGX_Parameter_Color "Color"
#define NVSDK_NGX_Parameter_Output "Output"
#define NVSDK_NGX_Parameter_Depth "Depth"
#define NVSDK_NGX_Parameter_MotionVectors "MotionVectors"
#define NVSDK_NGX_Parameter_Reset "Reset"
#define NVSDK_NGX_Parameter_MV_Scale_X "MV.Scale.X"
#define NVSDK_NGX_Parameter_MV_Scale_Y "MV.Scale.Y"
#define NVSDK_NGX_Parameter_Jitter_Offset_X "Jitter.Offset.X"
#define NVSDK_NGX_Parameter_Jitter_Offset_Y "Jitter.Offset.Y"
#define NVSDK_NGX_Parameter_PerfQualityValue "PerfQualityValue"
#define NVSDK_NGX_Parameter_DLSS_Feature_Create_Flags "DLSS.Create.Flags"
#define NVSDK_NGX_Parameter_FreeMemOnReleaseFeature "FreeMemOnReleaseFeature"
#define NVSDK_NGX_Parameter_CreationNodeMask "CreationNodeMask"
#define NVSDK_NGX_Parameter_VisibilityNodeMask "VisibilityNodeMask"
#define NVSDK_NGX_Parameter_DLSSOptimalRenderSize "DLSSOptimalRenderSize"
#define NVSDK_NGX_Parameter_Sharpness "Sharpness"
#define NVSDK_NGX_Parameter_Hdr "Hdr"
#define NVSDK_NGX_Parameter_DLSS_Input_Bias_Current_Color_Mask "DLSS.Input.Bias.Current.Color.Mask"
#define NVSDK_NGX_Parameter_TransparencyMask "TransparencyMask"
#define NVSDK_NGX_Parameter_ExposureTexture "ExposureTexture"
#define NVSDK_NGX_Parameter_DLSS_Enable_Output_Subrects "DLSS.Enable.Output.Subrects"

#define NVSDK_NGX_DLSS_Feature_Flags_IsInvalidated 1 << 0
#define NVSDK_NGX_DLSS_Feature_Flags_DoSharpening 1 << 1
#define NVSDK_NGX_DLSS_Feature_Flags_AutoExposure 1 << 6
#define NVSDK_NGX_DLSS_Feature_Flags_MVLowRes 1 << 5

typedef NVSDK_NGX_Result(NVSDK_CONV* PFN_NVSDK_NGX_VULKAN_Init)(
    unsigned long long InApplicationId, const wchar_t* InApplicationDataPath,
    VkInstance InInstance, VkPhysicalDevice InPD, VkDevice InDevice,
    PFN_vkGetInstanceProcAddr InGIPA, PFN_vkGetDeviceProcAddr InGDPA,
    const NVSDK_NGX_FeatureCommonInfo* InFeatureInfo, NVSDK_NGX_Version InSDKVersion);

typedef NVSDK_NGX_Result(NVSDK_CONV* PFN_NVSDK_NGX_VULKAN_Init_with_ProjectID)(
    const char* InProjectId, NVSDK_NGX_EngineType InEngineType, const char* InEngineVersion,
    const wchar_t* InApplicationDataPath, VkInstance InInstance, VkPhysicalDevice InPD, VkDevice InDevice,
    PFN_vkGetInstanceProcAddr InGIPA, PFN_vkGetDeviceProcAddr InGDPA,
    const NVSDK_NGX_FeatureCommonInfo* InFeatureInfo, NVSDK_NGX_Version InSDKVersion);

typedef NVSDK_NGX_Result(NVSDK_CONV* PFN_NVSDK_NGX_VULKAN_Shutdown1)(VkDevice InDevice);
typedef NVSDK_NGX_Result(NVSDK_CONV* PFN_NVSDK_NGX_VULKAN_GetCapabilityParameters)(NVSDK_NGX_Parameter** OutParameters);
typedef NVSDK_NGX_Result(NVSDK_CONV* PFN_NVSDK_NGX_VULKAN_AllocateParameters)(NVSDK_NGX_Parameter** OutParameters);
typedef NVSDK_NGX_Result(NVSDK_CONV* PFN_NVSDK_NGX_VULKAN_DestroyParameters)(NVSDK_NGX_Parameter* InParameters);
typedef NVSDK_NGX_Result(NVSDK_CONV* PFN_NVSDK_NGX_VULKAN_CreateFeature)(VkCommandBuffer InCmdList, NVSDK_NGX_Feature InFeatureID, NVSDK_NGX_Parameter* InParameters, NVSDK_NGX_Handle** OutHandle);
typedef NVSDK_NGX_Result(NVSDK_CONV* PFN_NVSDK_NGX_VULKAN_ReleaseFeature)(NVSDK_NGX_Handle* InHandle);
typedef NVSDK_NGX_Result(NVSDK_CONV* PFN_NVSDK_NGX_VULKAN_EvaluateFeature)(VkCommandBuffer InCmdList, const NVSDK_NGX_Handle* InFeatureHandle, const NVSDK_NGX_Parameter* InParameters, void* InCallback);

typedef void(NVSDK_CONV* PFN_NVSDK_NGX_Parameter_SetI)(NVSDK_NGX_Parameter* p, const char* name, int value);
typedef void(NVSDK_CONV* PFN_NVSDK_NGX_Parameter_SetUI)(NVSDK_NGX_Parameter* p, const char* name, unsigned int value);
typedef void(NVSDK_CONV* PFN_NVSDK_NGX_Parameter_SetF)(NVSDK_NGX_Parameter* p, const char* name, float value);
typedef void(NVSDK_CONV* PFN_NVSDK_NGX_Parameter_SetD)(NVSDK_NGX_Parameter* p, const char* name, double value);
typedef void(NVSDK_CONV* PFN_NVSDK_NGX_Parameter_SetULL)(NVSDK_NGX_Parameter* p, const char* name, unsigned long long value);
typedef void(NVSDK_CONV* PFN_NVSDK_NGX_Parameter_SetVoidPointer)(NVSDK_NGX_Parameter* p, const char* name, void* value);
typedef NVSDK_NGX_Result(NVSDK_CONV* PFN_NVSDK_NGX_Parameter_GetI)(const NVSDK_NGX_Parameter* p, const char* name, int* value);
typedef NVSDK_NGX_Result(NVSDK_CONV* PFN_NVSDK_NGX_Parameter_GetVoidPointer)(const NVSDK_NGX_Parameter* p, const char* name, void** value);

inline NVSDK_NGX_Resource_VK FusionFixDLSS_MakeNgxImage(VkImage image, VkImageView view, VkFormat format, unsigned w, unsigned h, bool readWrite, bool depth)
{
    NVSDK_NGX_Resource_VK r{};
    r.Type = NVSDK_NGX_RESOURCE_VK_TYPE_VK_IMAGEVIEW;
    r.ReadWrite = readWrite;
    r.Resource.ImageViewInfo.Image = image;
    r.Resource.ImageViewInfo.ImageView = view;
    r.Resource.ImageViewInfo.Format = format;
    r.Resource.ImageViewInfo.Width = w;
    r.Resource.ImageViewInfo.Height = h;
    r.Resource.ImageViewInfo.SubresourceRange.aspectMask = depth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    r.Resource.ImageViewInfo.SubresourceRange.baseMipLevel = 0;
    r.Resource.ImageViewInfo.SubresourceRange.levelCount = 1;
    r.Resource.ImageViewInfo.SubresourceRange.baseArrayLayer = 0;
    r.Resource.ImageViewInfo.SubresourceRange.layerCount = 1;
    return r;
}
