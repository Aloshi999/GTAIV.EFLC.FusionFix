module;

#include <common.hxx>
#include "dlss/dlss_api.h"
#include "dlss/dlss_math.h"
#include "dlss/dlss_vk.h"
#include "dlss/dlss_ngx.h"
#include "dlss/reconstruct_mv_spv.h"
#include <d3d9.h>
#include <cstring>
#include <mutex>
#include <vector>
#include <string>
#include <algorithm>

export module dlss;

import common;
import comvars;
import settings;
import natives;
import gxtloader;

namespace
{
    using FusionFixDLSSMath::Mat4;
    using FusionFixDLSSMath::Vec2;

    constexpr uint32_t kD3D9CreateDeviceIndex = 16;
    constexpr uint32_t kD3D9ResetIndex = 16;
    constexpr uint32_t kD3D9SetTransformIndex = 44;
    constexpr uint32_t kD3D9SetVSConstFIndex = 94;

    constexpr char kProjectId[] = "f0510ff1-d155-4e51-a001-c0de00000501";

    struct ID3D11Resource;
    struct ID3D12Resource;

    struct NgxParameterVtbl
    {
        virtual void Set(const char* name, unsigned long long value) = 0;
        virtual void Set(const char* name, float value) = 0;
        virtual void Set(const char* name, double value) = 0;
        virtual void Set(const char* name, unsigned int value) = 0;
        virtual void Set(const char* name, int value) = 0;
        virtual void Set(const char* name, ID3D11Resource* value) = 0;
        virtual void Set(const char* name, ID3D12Resource* value) = 0;
        virtual void Set(const char* name, void* value) = 0;
        virtual NVSDK_NGX_Result Get(const char* name, unsigned long long* value) const = 0;
        virtual NVSDK_NGX_Result Get(const char* name, float* value) const = 0;
        virtual NVSDK_NGX_Result Get(const char* name, double* value) const = 0;
        virtual NVSDK_NGX_Result Get(const char* name, unsigned int* value) const = 0;
        virtual NVSDK_NGX_Result Get(const char* name, int* value) const = 0;
        virtual NVSDK_NGX_Result Get(const char* name, ID3D11Resource** value) const = 0;
        virtual NVSDK_NGX_Result Get(const char* name, ID3D12Resource** value) const = 0;
        virtual NVSDK_NGX_Result Get(const char* name, void** value) const = 0;
        virtual void Reset() = 0;
    };

    struct ImageInfo
    {
        VkImage image = 0;
        VkFormat format = VK_FORMAT_UNDEFINED;
        uint32_t width = 0;
        uint32_t height = 0;
        VkImageUsageFlags usage = 0;
        bool depth = false;
    };

    struct SwapchainInfo
    {
        VkSwapchainKHR handle = 0;
        VkFormat format = VK_FORMAT_B8G8R8A8_UNORM;
        uint32_t width = 0;
        uint32_t height = 0;
        std::vector<VkImage> images;
    };

    struct OwnedImage
    {
        VkImage image = 0;
        VkDeviceMemory memory = 0;
        VkImageView view = 0;
        VkFormat format = VK_FORMAT_UNDEFINED;
        uint32_t width = 0;
        uint32_t height = 0;
    };

    struct State
    {
        std::mutex mutex;

        int mode = 0;
        int status = FusionFixDLSSStatus_Idle;
        unsigned ngxResult = 0;
        wchar_t label[96] = L"DLSS";

        Mat4 view = FusionFixDLSSMath::Mat4Identity();
        Mat4 proj = FusionFixDLSSMath::Mat4Identity();
        Mat4 viewProj = FusionFixDLSSMath::Mat4Identity();
        Mat4 prevViewProj = FusionFixDLSSMath::Mat4Identity();
        float jitterX = 0.0f;
        float jitterY = 0.0f;
        float prevJitterX = 0.0f;
        float prevJitterY = 0.0f;
        uint32_t frameIndex = 0;
        int resetHistory = 1;

        uint32_t displayW = 0;
        uint32_t displayH = 0;
        uint32_t renderW = 0;
        uint32_t renderH = 0;

        bool vulkanSeen = false;
        bool deviceHooked = false;

        VkInstance instance = nullptr;
        VkPhysicalDevice physical = nullptr;
        VkDevice device = nullptr;
        VkQueue queue = nullptr;
        uint32_t queueFamily = 0;
        PFN_vkGetInstanceProcAddr gipa = nullptr;
        PFN_vkGetDeviceProcAddr gdpa = nullptr;

        SwapchainInfo swapchain;
        VkImage lastBlitSrc = 0;
        VkImage lastDepth = 0;
        VkFormat lastDepthFormat = VK_FORMAT_UNDEFINED;
        uint32_t lastDepthW = 0;
        uint32_t lastDepthH = 0;
        std::vector<ImageInfo> images;

        OwnedImage mv;
        OwnedImage depthViewSrc; // unused placeholder
        VkImageView cachedDepthView = 0;
        VkImage cachedDepthImage = 0;
        VkSampler depthSampler = 0;
        VkDescriptorSetLayout dsetLayout = 0;
        VkPipelineLayout pipeLayout = 0;
        VkPipeline computePipe = 0;
        VkDescriptorPool dsetPool = 0;
        VkDescriptorSet dset = 0;
        VkCommandPool cmdPool = 0;
        VkCommandBuffer cmd = nullptr;
        VkFence fence = 0;
        VkShaderModule shader = 0;

        HMODULE ngxModule = nullptr;
        bool ngxInited = false;
        NVSDK_NGX_Handle* ngxFeature = nullptr;
        NVSDK_NGX_Parameter* ngxParams = nullptr;
        PFN_NVSDK_NGX_VULKAN_Init ngxInit = nullptr;
        PFN_NVSDK_NGX_VULKAN_Init_with_ProjectID ngxInitProject = nullptr;
        PFN_NVSDK_NGX_VULKAN_Shutdown1 ngxShutdown = nullptr;
        PFN_NVSDK_NGX_VULKAN_GetCapabilityParameters ngxGetCaps = nullptr;
        PFN_NVSDK_NGX_VULKAN_AllocateParameters ngxAllocParams = nullptr;
        PFN_NVSDK_NGX_VULKAN_DestroyParameters ngxDestroyParams = nullptr;
        PFN_NVSDK_NGX_VULKAN_CreateFeature ngxCreate = nullptr;
        PFN_NVSDK_NGX_VULKAN_ReleaseFeature ngxRelease = nullptr;
        PFN_NVSDK_NGX_VULKAN_EvaluateFeature ngxEval = nullptr;
        PFN_NVSDK_NGX_Parameter_SetI pSetI = nullptr;
        PFN_NVSDK_NGX_Parameter_SetUI pSetUI = nullptr;
        PFN_NVSDK_NGX_Parameter_SetF pSetF = nullptr;
        PFN_NVSDK_NGX_Parameter_SetVoidPointer pSetPtr = nullptr;

        PFN_vkCreateInstance CreateInstance = nullptr;
        PFN_vkCreateDevice CreateDevice = nullptr;
        PFN_vkCreateSwapchainKHR CreateSwapchainKHR = nullptr;
        PFN_vkDestroySwapchainKHR DestroySwapchainKHR = nullptr;
        PFN_vkGetSwapchainImagesKHR GetSwapchainImagesKHR = nullptr;
        PFN_vkQueuePresentKHR QueuePresentKHR = nullptr;
        PFN_vkQueueSubmit QueueSubmit = nullptr;
        PFN_vkCmdBlitImage CmdBlitImage = nullptr;
        PFN_vkCmdBeginRenderPass CmdBeginRenderPass = nullptr;
        PFN_vkCreateImage CreateImage = nullptr;
        PFN_vkDestroyImage DestroyImage = nullptr;
        PFN_vkGetDeviceQueue GetDeviceQueue = nullptr;
        PFN_vkEnumeratePhysicalDevices EnumeratePhysicalDevices = nullptr;
        PFN_vkGetPhysicalDeviceMemoryProperties GetPhysicalDeviceMemoryProperties = nullptr;
        PFN_vkGetPhysicalDeviceQueueFamilyProperties GetPhysicalDeviceQueueFamilyProperties = nullptr;
        PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR GetPhysicalDeviceSurfaceCapabilitiesKHR = nullptr;
        PFN_vkDeviceWaitIdle DeviceWaitIdle = nullptr;
        PFN_vkCreateCommandPool CreateCommandPool = nullptr;
        PFN_vkDestroyCommandPool DestroyCommandPool = nullptr;
        PFN_vkAllocateCommandBuffers AllocateCommandBuffers = nullptr;
        PFN_vkBeginCommandBuffer BeginCommandBuffer = nullptr;
        PFN_vkEndCommandBuffer EndCommandBuffer = nullptr;
        PFN_vkResetCommandBuffer ResetCommandBuffer = nullptr;
        PFN_vkCmdPipelineBarrier CmdPipelineBarrier = nullptr;
        PFN_vkCmdBindPipeline CmdBindPipeline = nullptr;
        PFN_vkCmdBindDescriptorSets CmdBindDescriptorSets = nullptr;
        PFN_vkCmdPushConstants CmdPushConstants = nullptr;
        PFN_vkCmdDispatch CmdDispatch = nullptr;
        PFN_vkCmdClearColorImage CmdClearColorImage = nullptr;
        PFN_vkGetImageMemoryRequirements GetImageMemoryRequirements = nullptr;
        PFN_vkAllocateMemory AllocateMemory = nullptr;
        PFN_vkFreeMemory FreeMemory = nullptr;
        PFN_vkBindImageMemory BindImageMemory = nullptr;
        PFN_vkCreateImageView CreateImageView = nullptr;
        PFN_vkDestroyImageView DestroyImageView = nullptr;
        PFN_vkCreateSampler CreateSampler = nullptr;
        PFN_vkDestroySampler DestroySampler = nullptr;
        PFN_vkCreateDescriptorSetLayout CreateDescriptorSetLayout = nullptr;
        PFN_vkDestroyDescriptorSetLayout DestroyDescriptorSetLayout = nullptr;
        PFN_vkCreatePipelineLayout CreatePipelineLayout = nullptr;
        PFN_vkDestroyPipelineLayout DestroyPipelineLayout = nullptr;
        PFN_vkCreateShaderModule CreateShaderModule = nullptr;
        PFN_vkDestroyShaderModule DestroyShaderModule = nullptr;
        PFN_vkCreateComputePipelines CreateComputePipelines = nullptr;
        PFN_vkDestroyPipeline DestroyPipeline = nullptr;
        PFN_vkCreateDescriptorPool CreateDescriptorPool = nullptr;
        PFN_vkDestroyDescriptorPool DestroyDescriptorPool = nullptr;
        PFN_vkAllocateDescriptorSets AllocateDescriptorSets = nullptr;
        PFN_vkUpdateDescriptorSets UpdateDescriptorSets = nullptr;
        PFN_vkCreateFence CreateFence = nullptr;
        PFN_vkDestroyFence DestroyFence = nullptr;
        PFN_vkWaitForFences WaitForFences = nullptr;
        PFN_vkResetFences ResetFences = nullptr;
    };

    State g;

    SafetyHookInline shLoadLibraryW{};
    SafetyHookInline shLoadLibraryExW{};
    SafetyHookInline shGipa{};
    SafetyHookInline shDirect3DCreate9{};
    SafetyHookInline shCreateDevice{};
    SafetyHookInline shReset{};
    SafetyHookInline shSetTransform{};
    SafetyHookInline shSetVSConstF{};

    void SetStatus(int status, unsigned ngx = 0)
    {
        g.status = status;
        g.ngxResult = ngx;
        if (status == FusionFixDLSSStatus_Idle || (status == FusionFixDLSSStatus_Ok && g.mode == 0))
            wcsncpy_s(g.label, L"DLSS", _TRUNCATE);
        else if (status == FusionFixDLSSStatus_Ok)
            wcsncpy_s(g.label, L"DLSS", _TRUNCATE);
        else if (status == FusionFixDLSSStatus_NoVulkan)
            wcsncpy_s(g.label, L"DLSS: no Vulkan", _TRUNCATE);
        else if (status == FusionFixDLSSStatus_NoNgx)
            wcsncpy_s(g.label, L"DLSS: no NGX", _TRUNCATE);
        else if (status == FusionFixDLSSStatus_NoSwapchain)
            wcsncpy_s(g.label, L"DLSS: no swapchain", _TRUNCATE);
        else
        {
            swprintf_s(g.label, L"DLSS: NGX 0x%X", ngx ? ngx : 0xBAD00000u);
        }
    }

    void RefreshStatusLocked()
    {
        if (g.mode == FusionFixDLSSMode_Off)
        {
            SetStatus(FusionFixDLSSStatus_Idle);
            return;
        }
        if (!g.vulkanSeen && !GetModuleHandleW(L"vulkan-1.dll") && !GetModuleHandleW(L"winevulkan.dll"))
        {
            SetStatus(FusionFixDLSSStatus_NoVulkan);
            return;
        }
        if (g.status == FusionFixDLSSStatus_Ok)
            SetStatus(FusionFixDLSSStatus_Ok);
    }

    bool IsVulkanApi()
    {
        return FusionFixSettings.Get("PREF_GRAPHICSAPI") != 0
            || GetModuleHandleW(L"vulkan-1.dll") != nullptr
            || GetModuleHandleW(L"winevulkan.dll") != nullptr;
    }

    void ForcePostFxOff()
    {
        if (g.mode == FusionFixDLSSMode_Off)
            return;
        // Get() is overridden to Off while DLSS is on — use the raw stored value.
        if (auto aa = FusionFixSettings.GetRef("PREF_ANTIALIASING"))
        {
            if (aa->get() != FusionFixSettings.AntialiasingText.eMO_OFF)
                FusionFixSettings.Set("PREF_ANTIALIASING", FusionFixSettings.AntialiasingText.eMO_OFF);
        }
        if (auto mb = FusionFixSettings.GetRef("PREF_MOTIONBLUR"))
        {
            if (mb->get() != 0)
                FusionFixSettings.Set("PREF_MOTIONBLUR", 0);
        }
    }

    void WriteD3d9Dlss(int mode)
    {
        CIniReader d3d9cfg(CSettings::d3d9cfgPath);
        d3d9cfg.WriteInteger("MAIN", "DLSS", mode, true);
    }

    Mat4 Transpose(const Mat4& in)
    {
        Mat4 o{};
        for (int r = 0; r < 4; ++r)
            for (int c = 0; c < 4; ++c)
                o.m[r * 4 + c] = in.m[c * 4 + r];
        return o;
    }

    void UpdateCameraMatrices()
    {
        Cam cam = 0;
        Natives::GetRootCam(&cam);
        if (!cam)
            return;

        float px = 0, py = 0, pz = 0, rx = 0, ry = 0, rz = 0, fov = 0;
        Natives::GetCamPos(cam, &px, &py, &pz);
        Natives::GetCamRot(cam, &rx, &ry, &rz);
        Natives::GetCamFov(cam, &fov);

        const float pitch = rx * float(3.14159265 / 180.0);
        const float roll = ry * float(3.14159265 / 180.0);
        const float yaw = rz * float(3.14159265 / 180.0);
        (void)roll;

        // GTA IV: Z-up. Camera forward from heading/pitch.
        const float cy = std::cos(yaw), sy = std::sin(yaw);
        const float cp = std::cos(pitch), sp = std::sin(pitch);
        const float fx = -sy * cp;
        const float fy = cy * cp;
        const float fz = sp;
        float ux = 0.0f, uy = 0.0f, uz = 1.0f;
        float rxv = fy * uz - fz * uy;
        float ryv = fz * ux - fx * uz;
        float rzv = fx * uy - fy * ux;
        float rlen = std::sqrt(rxv * rxv + ryv * ryv + rzv * rzv);
        if (rlen < 1e-6f)
            return;
        rxv /= rlen; ryv /= rlen; rzv /= rlen;
        ux = ryv * fz - rzv * fy;
        uy = rzv * fx - rxv * fz;
        uz = rxv * fy - ryv * fx;

        Mat4 view{};
        view.m[0] = rxv; view.m[1] = ux; view.m[2] = fx; view.m[3] = 0;
        view.m[4] = ryv; view.m[5] = uy; view.m[6] = fy; view.m[7] = 0;
        view.m[8] = rzv; view.m[9] = uz; view.m[10] = fz; view.m[11] = 0;
        view.m[12] = -(rxv * px + ryv * py + rzv * pz);
        view.m[13] = -(ux * px + uy * py + uz * pz);
        view.m[14] = -(fx * px + fy * py + fz * pz);
        view.m[15] = 1.0f;

        const float aspect = (g.renderW && g.renderH) ? float(g.renderW) / float(g.renderH) : (16.0f / 9.0f);
        const float fovRad = (fov > 1.0f ? fov : 45.0f) * float(3.14159265 / 180.0);
        const float yScale = 1.0f / std::tan(fovRad * 0.5f);
        const float xScale = yScale / aspect;
        const float zn = 0.15f, zf = 10000.0f;
        Mat4 proj{};
        proj.m[0] = xScale;
        proj.m[5] = yScale;
        proj.m[10] = zf / (zf - zn);
        proj.m[11] = 1.0f;
        proj.m[14] = -zn * zf / (zf - zn);

        FusionFixDLSSMath::ApplyProjectionJitter(proj, g.jitterX, g.jitterY,
            g.renderW ? float(g.renderW) : 1280.0f, g.renderH ? float(g.renderH) : 720.0f);

        g.view = view;
        g.proj = proj;
        g.prevViewProj = g.viewProj;
        g.viewProj = FusionFixDLSSMath::Mat4Multiply(view, proj);
        g.prevJitterX = g.jitterX;
        g.prevJitterY = g.jitterY;
    }

    void AdvanceJitter()
    {
        const Vec2 j = FusionFixDLSSMath::Halton23Jitter(g.frameIndex++);
        g.jitterX = j.x;
        g.jitterY = j.y;
    }

    bool LooksLikeMainProjection(const float* m)
    {
        if (!FusionFixDLSSMath::LooksLikePerspectiveProjection(m))
            return false;
        const float aspect = (g.renderW && g.renderH) ? float(g.renderW) / float(g.renderH) : 0.0f;
        if (aspect <= 0.0f)
            return true;
        const float mx = std::fabs(m[0]);
        const float my = std::fabs(m[5]);
        if (mx < 1e-4f || my < 1e-4f)
            return false;
        const float matAspect = my / mx;
        return std::fabs(matAspect - aspect) < 0.45f;
    }

    void ApplyJitterToConstants(float* data, UINT count)
    {
        if (g.mode == FusionFixDLSSMode_Off || !data)
            return;
        const float w = g.renderW ? float(g.renderW) : 0.0f;
        const float h = g.renderH ? float(g.renderH) : 0.0f;
        if (w <= 0.0f || h <= 0.0f)
            return;
        for (UINT i = 0; i + 4 <= count; i += 4)
        {
            float* m = data + i * 4;
            if (!LooksLikeMainProjection(m))
                continue;
            m[8] += (g.jitterX * 2.0f) / w;
            m[9] += (g.jitterY * 2.0f) / h;
        }
    }

    uint32_t FindMemoryType(uint32_t typeBits, VkMemoryPropertyFlags flags)
    {
        if (!g.GetPhysicalDeviceMemoryProperties || !g.physical)
            return 0;
        VkPhysicalDeviceMemoryProperties props{};
        g.GetPhysicalDeviceMemoryProperties(g.physical, &props);
        for (uint32_t i = 0; i < props.memoryTypeCount; ++i)
        {
            if ((typeBits & (1u << i)) && (props.memoryTypes[i].propertyFlags & flags) == flags)
                return i;
        }
        return 0;
    }

    void DestroyOwned(OwnedImage& img)
    {
        if (!g.device)
            return;
        if (img.view && g.DestroyImageView)
            g.DestroyImageView(g.device, img.view, nullptr);
        if (img.image && g.DestroyImage)
            g.DestroyImage(g.device, img.image, nullptr);
        if (img.memory && g.FreeMemory)
            g.FreeMemory(g.device, img.memory, nullptr);
        img = {};
    }

    bool CreateOwnedImage(OwnedImage& img, uint32_t w, uint32_t h, VkFormat format, VkImageUsageFlags usage, bool depth)
    {
        DestroyOwned(img);
        VkImageCreateInfo ci{};
        ci.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        ci.imageType = VK_IMAGE_TYPE_2D;
        ci.format = format;
        ci.extent = { w, h, 1 };
        ci.mipLevels = 1;
        ci.arrayLayers = 1;
        ci.samples = 1;
        ci.tiling = VK_IMAGE_TILING_OPTIMAL;
        ci.usage = usage;
        ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        if (g.CreateImage(g.device, &ci, nullptr, &img.image) != VK_SUCCESS)
            return false;
        VkMemoryRequirements req{};
        g.GetImageMemoryRequirements(g.device, img.image, &req);
        VkMemoryAllocateInfo ai{};
        ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        ai.allocationSize = req.size;
        ai.memoryTypeIndex = FindMemoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if (g.AllocateMemory(g.device, &ai, nullptr, &img.memory) != VK_SUCCESS)
            return false;
        if (g.BindImageMemory(g.device, img.image, img.memory, 0) != VK_SUCCESS)
            return false;
        VkImageViewCreateInfo vi{};
        vi.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        vi.image = img.image;
        vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
        vi.format = format;
        vi.subresourceRange.aspectMask = depth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
        vi.subresourceRange.levelCount = 1;
        vi.subresourceRange.layerCount = 1;
        if (g.CreateImageView(g.device, &vi, nullptr, &img.view) != VK_SUCCESS)
            return false;
        img.format = format;
        img.width = w;
        img.height = h;
        return true;
    }

    void DestroyCompute()
    {
        if (!g.device)
            return;
        if (g.computePipe && g.DestroyPipeline)
            g.DestroyPipeline(g.device, g.computePipe, nullptr);
        if (g.pipeLayout && g.DestroyPipelineLayout)
            g.DestroyPipelineLayout(g.device, g.pipeLayout, nullptr);
        if (g.dsetLayout && g.DestroyDescriptorSetLayout)
            g.DestroyDescriptorSetLayout(g.device, g.dsetLayout, nullptr);
        if (g.dsetPool && g.DestroyDescriptorPool)
            g.DestroyDescriptorPool(g.device, g.dsetPool, nullptr);
        if (g.shader && g.DestroyShaderModule)
            g.DestroyShaderModule(g.device, g.shader, nullptr);
        if (g.depthSampler && g.DestroySampler)
            g.DestroySampler(g.device, g.depthSampler, nullptr);
        if (g.cachedDepthView && g.DestroyImageView)
            g.DestroyImageView(g.device, g.cachedDepthView, nullptr);
        if (g.fence && g.DestroyFence)
            g.DestroyFence(g.device, g.fence, nullptr);
        if (g.cmdPool && g.DestroyCommandPool)
            g.DestroyCommandPool(g.device, g.cmdPool, nullptr);
        g.computePipe = 0;
        g.pipeLayout = 0;
        g.dsetLayout = 0;
        g.dsetPool = 0;
        g.dset = 0;
        g.shader = 0;
        g.depthSampler = 0;
        g.cachedDepthView = 0;
        g.cachedDepthImage = 0;
        g.fence = 0;
        g.cmdPool = 0;
        g.cmd = nullptr;
        DestroyOwned(g.mv);
    }

    bool EnsureDeviceProcs()
    {
        if (!g.device || !g.gdpa)
            return false;
#define FF_VK(name) g.name = (PFN_vk##name)g.gdpa(g.device, "vk" #name)
        FF_VK(GetDeviceQueue);
        FF_VK(CreateSwapchainKHR);
        FF_VK(DestroySwapchainKHR);
        FF_VK(GetSwapchainImagesKHR);
        FF_VK(QueuePresentKHR);
        FF_VK(QueueSubmit);
        FF_VK(CmdBlitImage);
        FF_VK(CmdBeginRenderPass);
        FF_VK(CreateImage);
        FF_VK(DestroyImage);
        FF_VK(DeviceWaitIdle);
        FF_VK(CreateCommandPool);
        FF_VK(DestroyCommandPool);
        FF_VK(AllocateCommandBuffers);
        FF_VK(BeginCommandBuffer);
        FF_VK(EndCommandBuffer);
        FF_VK(ResetCommandBuffer);
        FF_VK(CmdPipelineBarrier);
        FF_VK(CmdBindPipeline);
        FF_VK(CmdBindDescriptorSets);
        FF_VK(CmdPushConstants);
        FF_VK(CmdDispatch);
        FF_VK(CmdClearColorImage);
        FF_VK(GetImageMemoryRequirements);
        FF_VK(AllocateMemory);
        FF_VK(FreeMemory);
        FF_VK(BindImageMemory);
        FF_VK(CreateImageView);
        FF_VK(DestroyImageView);
        FF_VK(CreateSampler);
        FF_VK(DestroySampler);
        FF_VK(CreateDescriptorSetLayout);
        FF_VK(DestroyDescriptorSetLayout);
        FF_VK(CreatePipelineLayout);
        FF_VK(DestroyPipelineLayout);
        FF_VK(CreateShaderModule);
        FF_VK(DestroyShaderModule);
        FF_VK(CreateComputePipelines);
        FF_VK(DestroyPipeline);
        FF_VK(CreateDescriptorPool);
        FF_VK(DestroyDescriptorPool);
        FF_VK(AllocateDescriptorSets);
        FF_VK(UpdateDescriptorSets);
        FF_VK(CreateFence);
        FF_VK(DestroyFence);
        FF_VK(WaitForFences);
        FF_VK(ResetFences);
#undef FF_VK
        if (g.instance && g.gipa)
        {
            g.EnumeratePhysicalDevices = (PFN_vkEnumeratePhysicalDevices)g.gipa(g.instance, "vkEnumeratePhysicalDevices");
            g.GetPhysicalDeviceMemoryProperties = (PFN_vkGetPhysicalDeviceMemoryProperties)g.gipa(g.instance, "vkGetPhysicalDeviceMemoryProperties");
            g.GetPhysicalDeviceQueueFamilyProperties = (PFN_vkGetPhysicalDeviceQueueFamilyProperties)g.gipa(g.instance, "vkGetPhysicalDeviceQueueFamilyProperties");
            g.GetPhysicalDeviceSurfaceCapabilitiesKHR = (PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR)g.gipa(g.instance, "vkGetPhysicalDeviceSurfaceCapabilitiesKHR");
        }
        return g.QueueSubmit && g.CreateImage;
    }

    bool EnsureCompute()
    {
        if (g.computePipe)
            return true;
        if (!EnsureDeviceProcs())
            return false;

        if (!g.cmdPool)
        {
            VkCommandPoolCreateInfo pci{};
            pci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
            pci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
            pci.queueFamilyIndex = g.queueFamily;
            if (g.CreateCommandPool(g.device, &pci, nullptr, &g.cmdPool) != VK_SUCCESS)
                return false;
            VkCommandBufferAllocateInfo ai{};
            ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            ai.commandPool = g.cmdPool;
            ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            ai.commandBufferCount = 1;
            if (g.AllocateCommandBuffers(g.device, &ai, &g.cmd) != VK_SUCCESS)
                return false;
            VkFenceCreateInfo fi{};
            fi.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
            if (g.CreateFence(g.device, &fi, nullptr, &g.fence) != VK_SUCCESS)
                return false;
        }

        VkSamplerCreateInfo si{};
        si.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        si.magFilter = VK_FILTER_NEAREST;
        si.minFilter = VK_FILTER_NEAREST;
        si.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        si.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        si.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        if (g.CreateSampler(g.device, &si, nullptr, &g.depthSampler) != VK_SUCCESS)
            return false;

        VkDescriptorSetLayoutBinding binds[2]{};
        binds[0].binding = 0;
        binds[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        binds[0].descriptorCount = 1;
        binds[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        binds[1].binding = 1;
        binds[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        binds[1].descriptorCount = 1;
        binds[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        VkDescriptorSetLayoutCreateInfo lci{};
        lci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        lci.bindingCount = 2;
        lci.pBindings = binds;
        if (g.CreateDescriptorSetLayout(g.device, &lci, nullptr, &g.dsetLayout) != VK_SUCCESS)
            return false;

        VkPushConstantRange pcr{};
        pcr.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        pcr.size = 128;
        VkPipelineLayoutCreateInfo plci{};
        plci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        plci.setLayoutCount = 1;
        plci.pSetLayouts = &g.dsetLayout;
        plci.pushConstantRangeCount = 1;
        plci.pPushConstantRanges = &pcr;
        if (g.CreatePipelineLayout(g.device, &plci, nullptr, &g.pipeLayout) != VK_SUCCESS)
            return false;

        VkShaderModuleCreateInfo smci{};
        smci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        smci.codeSize = kFusionFixDLSS_ReconstructMvSpvBytes;
        smci.pCode = kFusionFixDLSS_ReconstructMvSpv;
        if (g.CreateShaderModule(g.device, &smci, nullptr, &g.shader) != VK_SUCCESS)
            return false;

        VkComputePipelineCreateInfo cpci{};
        cpci.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        cpci.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        cpci.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        cpci.stage.module = g.shader;
        cpci.stage.pName = "main";
        cpci.layout = g.pipeLayout;
        if (g.CreateComputePipelines(g.device, 0, 1, &cpci, nullptr, &g.computePipe) != VK_SUCCESS)
            return false;

        VkDescriptorPoolSize sizes[2] = {
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4 },
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 4 }
        };
        VkDescriptorPoolCreateInfo dpci{};
        dpci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        dpci.maxSets = 4;
        dpci.poolSizeCount = 2;
        dpci.pPoolSizes = sizes;
        if (g.CreateDescriptorPool(g.device, &dpci, nullptr, &g.dsetPool) != VK_SUCCESS)
            return false;
        VkDescriptorSetAllocateInfo dsai{};
        dsai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        dsai.descriptorPool = g.dsetPool;
        dsai.descriptorSetCount = 1;
        dsai.pSetLayouts = &g.dsetLayout;
        if (g.AllocateDescriptorSets(g.device, &dsai, &g.dset) != VK_SUCCESS)
            return false;
        return true;
    }

    bool LoadNgx()
    {
        if (g.ngxModule)
            return true;
        wchar_t path[MAX_PATH]{};
        UINT n = GetSystemDirectoryW(path, MAX_PATH);
        if (n && n < MAX_PATH - 16)
        {
            lstrcatW(path, L"\\nvngx.dll");
            g.ngxModule = LoadLibraryW(path);
        }
        if (!g.ngxModule)
            g.ngxModule = LoadLibraryW(L"nvngx.dll");
        if (!g.ngxModule)
            return false;

        g.ngxInit = (PFN_NVSDK_NGX_VULKAN_Init)GetProcAddress(g.ngxModule, "NVSDK_NGX_VULKAN_Init");
        g.ngxInitProject = (PFN_NVSDK_NGX_VULKAN_Init_with_ProjectID)GetProcAddress(g.ngxModule, "NVSDK_NGX_VULKAN_Init_with_ProjectID");
        g.ngxShutdown = (PFN_NVSDK_NGX_VULKAN_Shutdown1)GetProcAddress(g.ngxModule, "NVSDK_NGX_VULKAN_Shutdown1");
        g.ngxGetCaps = (PFN_NVSDK_NGX_VULKAN_GetCapabilityParameters)GetProcAddress(g.ngxModule, "NVSDK_NGX_VULKAN_GetCapabilityParameters");
        g.ngxAllocParams = (PFN_NVSDK_NGX_VULKAN_AllocateParameters)GetProcAddress(g.ngxModule, "NVSDK_NGX_VULKAN_AllocateParameters");
        g.ngxDestroyParams = (PFN_NVSDK_NGX_VULKAN_DestroyParameters)GetProcAddress(g.ngxModule, "NVSDK_NGX_VULKAN_DestroyParameters");
        g.ngxCreate = (PFN_NVSDK_NGX_VULKAN_CreateFeature)GetProcAddress(g.ngxModule, "NVSDK_NGX_VULKAN_CreateFeature");
        g.ngxRelease = (PFN_NVSDK_NGX_VULKAN_ReleaseFeature)GetProcAddress(g.ngxModule, "NVSDK_NGX_VULKAN_ReleaseFeature");
        g.ngxEval = (PFN_NVSDK_NGX_VULKAN_EvaluateFeature)GetProcAddress(g.ngxModule, "NVSDK_NGX_VULKAN_EvaluateFeature");
        if (!g.ngxEval)
            g.ngxEval = (PFN_NVSDK_NGX_VULKAN_EvaluateFeature)GetProcAddress(g.ngxModule, "NVSDK_NGX_VULKAN_EvaluateFeature_C");
        g.pSetI = (PFN_NVSDK_NGX_Parameter_SetI)GetProcAddress(g.ngxModule, "NVSDK_NGX_Parameter_SetI");
        g.pSetUI = (PFN_NVSDK_NGX_Parameter_SetUI)GetProcAddress(g.ngxModule, "NVSDK_NGX_Parameter_SetUI");
        g.pSetF = (PFN_NVSDK_NGX_Parameter_SetF)GetProcAddress(g.ngxModule, "NVSDK_NGX_Parameter_SetF");
        g.pSetPtr = (PFN_NVSDK_NGX_Parameter_SetVoidPointer)GetProcAddress(g.ngxModule, "NVSDK_NGX_Parameter_SetVoidPointer");
        return g.ngxInit || g.ngxInitProject;
    }

    void ParamSetI(NVSDK_NGX_Parameter* p, const char* n, int v)
    {
        if (g.pSetI) { g.pSetI(p, n, v); return; }
        if (p) reinterpret_cast<NgxParameterVtbl*>(p)->Set(n, v);
    }
    void ParamSetUI(NVSDK_NGX_Parameter* p, const char* n, unsigned v)
    {
        if (g.pSetUI) { g.pSetUI(p, n, v); return; }
        if (p) reinterpret_cast<NgxParameterVtbl*>(p)->Set(n, v);
    }
    void ParamSetF(NVSDK_NGX_Parameter* p, const char* n, float v)
    {
        if (g.pSetF) { g.pSetF(p, n, v); return; }
        if (p) reinterpret_cast<NgxParameterVtbl*>(p)->Set(n, v);
    }
    void ParamSetPtr(NVSDK_NGX_Parameter* p, const char* n, void* v)
    {
        if (g.pSetPtr) { g.pSetPtr(p, n, v); return; }
        if (p) reinterpret_cast<NgxParameterVtbl*>(p)->Set(n, v);
    }

    void ShutdownNgx()
    {
        if (g.ngxFeature && g.ngxRelease)
            g.ngxRelease(g.ngxFeature);
        g.ngxFeature = nullptr;
        if (g.ngxParams && g.ngxDestroyParams)
            g.ngxDestroyParams(g.ngxParams);
        g.ngxParams = nullptr;
        if (g.ngxInited && g.ngxShutdown)
            g.ngxShutdown(g.device);
        g.ngxInited = false;
    }

    bool InitNgx()
    {
        if (g.ngxInited)
            return true;
        if (!g.device || !g.instance || !g.physical)
            return false;
        if (!LoadNgx())
        {
            SetStatus(FusionFixDLSSStatus_NoNgx);
            return false;
        }

        wchar_t dataPath[MAX_PATH]{};
        GetTempPathW(MAX_PATH, dataPath);

        NVSDK_NGX_FeatureCommonInfo info{};
        NVSDK_NGX_Result r = NVSDK_NGX_Result_Fail;
        if (g.ngxInitProject)
        {
            r = g.ngxInitProject(kProjectId, NVSDK_NGX_ENGINE_TYPE_CUSTOM, "5.0.1",
                dataPath, g.instance, g.physical, g.device, g.gipa, g.gdpa, &info, NVSDK_NGX_Version_API);
        }
        else if (g.ngxInit)
        {
            r = g.ngxInit(0x2440A52Bull, dataPath, g.instance, g.physical, g.device, g.gipa, g.gdpa, &info, NVSDK_NGX_Version_API);
        }
        if (NVSDK_NGX_FAILED(r))
        {
            SetStatus(FusionFixDLSSStatus_NgxError, (unsigned)r);
            return false;
        }
        g.ngxInited = true;

        if (g.ngxAllocParams)
            g.ngxAllocParams(&g.ngxParams);
        else if (g.ngxGetCaps)
            g.ngxGetCaps(&g.ngxParams);
        if (!g.ngxParams)
        {
            SetStatus(FusionFixDLSSStatus_NgxError, 0xBAD00007);
            return false;
        }
        return true;
    }

    bool CreateNgxFeature(VkCommandBuffer cmd, uint32_t renderW, uint32_t renderH, uint32_t outW, uint32_t outH)
    {
        if (g.ngxFeature)
            return true;
        if (!InitNgx() || !g.ngxCreate)
            return false;

        ParamSetUI(g.ngxParams, NVSDK_NGX_Parameter_Width, renderW);
        ParamSetUI(g.ngxParams, NVSDK_NGX_Parameter_Height, renderH);
        ParamSetUI(g.ngxParams, NVSDK_NGX_Parameter_OutWidth, outW);
        ParamSetUI(g.ngxParams, NVSDK_NGX_Parameter_OutHeight, outH);
        ParamSetI(g.ngxParams, NVSDK_NGX_Parameter_PerfQualityValue, FusionFixDLSSMath::NgxPerfQualityForMode(g.mode));
        ParamSetI(g.ngxParams, NVSDK_NGX_Parameter_DLSS_Feature_Create_Flags, NVSDK_NGX_DLSS_Feature_Flags_AutoExposure);
        ParamSetUI(g.ngxParams, NVSDK_NGX_Parameter_CreationNodeMask, 1);
        ParamSetUI(g.ngxParams, NVSDK_NGX_Parameter_VisibilityNodeMask, 1);

        NVSDK_NGX_Result r = g.ngxCreate(cmd, NVSDK_NGX_Feature_SuperSampling, g.ngxParams, &g.ngxFeature);
        if (NVSDK_NGX_FAILED(r) || !g.ngxFeature)
        {
            SetStatus(FusionFixDLSSStatus_NgxError, (unsigned)r);
            g.ngxFeature = nullptr;
            return false;
        }
        SetStatus(FusionFixDLSSStatus_Ok);
        return true;
    }

    void Transition(VkCommandBuffer cmd, VkImage image, VkImageLayout oldL, VkImageLayout newL, VkImageAspectFlags aspect)
    {
        VkImageMemoryBarrier b{};
        b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        b.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT;
        b.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
        b.oldLayout = oldL;
        b.newLayout = newL;
        b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.image = image;
        b.subresourceRange.aspectMask = aspect;
        b.subresourceRange.levelCount = 1;
        b.subresourceRange.layerCount = 1;
        g.CmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &b);
    }

    bool EvaluateDlss(VkImage color, VkFormat colorFmt, VkImage depth, VkFormat depthFmt, VkImage output, VkFormat outFmt,
                      uint32_t renderW, uint32_t renderH, uint32_t outW, uint32_t outH)
    {
        if (!color || !output || !g.queue || !g.cmd)
            return false;
        if (!EnsureCompute())
            return false;
        if (g.mv.width != renderW || g.mv.height != renderH)
        {
            if (!CreateOwnedImage(g.mv, renderW, renderH, VK_FORMAT_R16G16_SFLOAT,
                VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, false))
                return false;
        }

        if (depth && depth != g.cachedDepthImage)
        {
            if (g.cachedDepthView)
                g.DestroyImageView(g.device, g.cachedDepthView, nullptr);
            g.cachedDepthView = 0;
            VkImageViewCreateInfo vi{};
            vi.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            vi.image = depth;
            vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
            vi.format = depthFmt;
            vi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
            vi.subresourceRange.levelCount = 1;
            vi.subresourceRange.layerCount = 1;
            if (g.CreateImageView(g.device, &vi, nullptr, &g.cachedDepthView) == VK_SUCCESS)
                g.cachedDepthImage = depth;
        }

        g.ResetCommandBuffer(g.cmd, 0);
        VkCommandBufferBeginInfo bi{};
        bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        g.BeginCommandBuffer(g.cmd, &bi);

        Transition(g.cmd, g.mv.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT);
        if (depth)
            Transition(g.cmd, depth, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT);

        if (g.cachedDepthView && g.computePipe)
        {
            VkDescriptorImageInfo depthInfo{ g.depthSampler, g.cachedDepthView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
            VkDescriptorImageInfo mvInfo{ 0, g.mv.view, VK_IMAGE_LAYOUT_GENERAL };
            VkWriteDescriptorSet writes[2]{};
            writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[0].dstSet = g.dset;
            writes[0].dstBinding = 0;
            writes[0].descriptorCount = 1;
            writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writes[0].pImageInfo = &depthInfo;
            writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[1].dstSet = g.dset;
            writes[1].dstBinding = 1;
            writes[1].descriptorCount = 1;
            writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            writes[1].pImageInfo = &mvInfo;
            g.UpdateDescriptorSets(g.device, 2, writes, 0, nullptr);
            g.CmdBindPipeline(g.cmd, VK_PIPELINE_BIND_POINT_COMPUTE, g.computePipe);
            g.CmdBindDescriptorSets(g.cmd, VK_PIPELINE_BIND_POINT_COMPUTE, g.pipeLayout, 0, 1, &g.dset, 0, nullptr);
            Mat4 inv = Transpose(FusionFixDLSSMath::Mat4Inverse(g.viewProj));
            Mat4 prev = Transpose(g.prevViewProj);
            float pc[32];
            std::memcpy(pc, inv.m, 64);
            std::memcpy(pc + 16, prev.m, 64);
            g.CmdPushConstants(g.cmd, g.pipeLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, 128, pc);
            g.CmdDispatch(g.cmd, (renderW + 7) / 8, (renderH + 7) / 8, 1);
        }
        else
        {
            float clear[4] = { 0, 0, 0, 0 };
            VkImageSubresourceRange rng{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
            g.CmdClearColorImage(g.cmd, g.mv.image, VK_IMAGE_LAYOUT_GENERAL, clear, 1, &rng);
        }

        if (!CreateNgxFeature(g.cmd, renderW, renderH, outW, outH) || !g.ngxEval)
        {
            g.EndCommandBuffer(g.cmd);
            return false;
        }

        Transition(g.cmd, color, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
        Transition(g.cmd, output, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT);

        VkImageView colorView = 0, outView = 0;
        VkImageViewCreateInfo vci{};
        vci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        vci.viewType = VK_IMAGE_VIEW_TYPE_2D;
        vci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        vci.subresourceRange.levelCount = 1;
        vci.subresourceRange.layerCount = 1;
        vci.image = color;
        vci.format = colorFmt;
        g.CreateImageView(g.device, &vci, nullptr, &colorView);
        vci.image = output;
        vci.format = outFmt;
        g.CreateImageView(g.device, &vci, nullptr, &outView);

        NVSDK_NGX_Resource_VK colorRes = FusionFixDLSS_MakeNgxImage(color, colorView, colorFmt, renderW, renderH, false, false);
        NVSDK_NGX_Resource_VK outRes = FusionFixDLSS_MakeNgxImage(output, outView, outFmt, outW, outH, true, false);
        NVSDK_NGX_Resource_VK mvRes = FusionFixDLSS_MakeNgxImage(g.mv.image, g.mv.view, g.mv.format, renderW, renderH, false, false);
        NVSDK_NGX_Resource_VK depthRes{};
        if (depth && g.cachedDepthView)
            depthRes = FusionFixDLSS_MakeNgxImage(depth, g.cachedDepthView, depthFmt, renderW, renderH, false, true);

        ParamSetUI(g.ngxParams, NVSDK_NGX_Parameter_Width, renderW);
        ParamSetUI(g.ngxParams, NVSDK_NGX_Parameter_Height, renderH);
        ParamSetUI(g.ngxParams, NVSDK_NGX_Parameter_OutWidth, outW);
        ParamSetUI(g.ngxParams, NVSDK_NGX_Parameter_OutHeight, outH);
        ParamSetF(g.ngxParams, NVSDK_NGX_Parameter_Jitter_Offset_X, g.jitterX);
        ParamSetF(g.ngxParams, NVSDK_NGX_Parameter_Jitter_Offset_Y, g.jitterY);
        ParamSetF(g.ngxParams, NVSDK_NGX_Parameter_MV_Scale_X, 1.0f);
        ParamSetF(g.ngxParams, NVSDK_NGX_Parameter_MV_Scale_Y, 1.0f);
        ParamSetI(g.ngxParams, NVSDK_NGX_Parameter_Reset, g.resetHistory);
        ParamSetPtr(g.ngxParams, NVSDK_NGX_Parameter_Color, &colorRes);
        ParamSetPtr(g.ngxParams, NVSDK_NGX_Parameter_Output, &outRes);
        ParamSetPtr(g.ngxParams, NVSDK_NGX_Parameter_MotionVectors, &mvRes);
        if (depth && g.cachedDepthView)
            ParamSetPtr(g.ngxParams, NVSDK_NGX_Parameter_Depth, &depthRes);

        NVSDK_NGX_Result r = g.ngxEval(g.cmd, g.ngxFeature, g.ngxParams, nullptr);

        Transition(g.cmd, output, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_ASPECT_COLOR_BIT);
        if (depth)
            Transition(g.cmd, depth, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT);

        g.EndCommandBuffer(g.cmd);
        VkSubmitInfo si{};
        si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        si.commandBufferCount = 1;
        si.pCommandBuffers = &g.cmd;
        g.ResetFences(g.device, 1, &g.fence);
        if (g.QueueSubmit(g.queue, 1, &si, g.fence) == VK_SUCCESS)
            g.WaitForFences(g.device, 1, &g.fence, VK_TRUE, 1ull << 32);

        if (colorView)
            g.DestroyImageView(g.device, colorView, nullptr);
        if (outView)
            g.DestroyImageView(g.device, outView, nullptr);

        g.resetHistory = 0;
        if (NVSDK_NGX_FAILED(r))
        {
            SetStatus(FusionFixDLSSStatus_NgxError, (unsigned)r);
            return false;
        }
        SetStatus(FusionFixDLSSStatus_Ok);
        return true;
    }

    PFN_vkVoidFunction VKAPI_PTR WrapGipa(VkInstance instance, const char* name);
    PFN_vkVoidFunction VKAPI_PTR WrapGdpa(VkDevice device, const char* name);

    VkResult VKAPI_PTR HkCreateInstance(const VkInstanceCreateInfo* pCreateInfo, const void* pAllocator, VkInstance* pInstance)
    {
        auto r = g.CreateInstance ? g.CreateInstance(pCreateInfo, pAllocator, pInstance) : VK_ERROR_INITIALIZATION_FAILED;
        if (r == VK_SUCCESS && pInstance)
        {
            g.instance = *pInstance;
            g.vulkanSeen = true;
        }
        return r;
    }

    VkResult VKAPI_PTR HkCreateDevice(VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo* pCreateInfo, const void* pAllocator, VkDevice* pDevice)
    {
        g.physical = physicalDevice;
        auto r = g.CreateDevice ? g.CreateDevice(physicalDevice, pCreateInfo, pAllocator, pDevice) : VK_ERROR_INITIALIZATION_FAILED;
        if (r == VK_SUCCESS && pDevice)
        {
            g.device = *pDevice;
            g.gdpa = (PFN_vkGetDeviceProcAddr)(g.gipa ? g.gipa(g.instance, "vkGetDeviceProcAddr") : nullptr);
            EnsureDeviceProcs();
            if (g.GetDeviceQueue && pCreateInfo && pCreateInfo->queueCreateInfoCount)
            {
                g.queueFamily = pCreateInfo->pQueueCreateInfos[0].queueFamilyIndex;
                g.GetDeviceQueue(g.device, g.queueFamily, 0, &g.queue);
            }
        }
        return r;
    }

    VkResult VKAPI_PTR HkCreateSwapchainKHR(VkDevice device, const VkSwapchainCreateInfoKHR* pCreateInfo, const void* pAllocator, VkSwapchainKHR* pSwapchain)
    {
        VkSwapchainCreateInfoKHR ci = pCreateInfo ? *pCreateInfo : VkSwapchainCreateInfoKHR{};
        if (g.mode != FusionFixDLSSMode_Off && g.GetPhysicalDeviceSurfaceCapabilitiesKHR && g.physical && pCreateInfo)
        {
            VkSurfaceCapabilitiesKHR caps{};
            if (g.GetPhysicalDeviceSurfaceCapabilitiesKHR(g.physical, pCreateInfo->surface, &caps) == VK_SUCCESS
                && caps.currentExtent.width != 0xFFFFFFFFu)
            {
                ci.imageExtent = caps.currentExtent;
                g.displayW = caps.currentExtent.width;
                g.displayH = caps.currentExtent.height;
            }
        }
        auto r = g.CreateSwapchainKHR ? g.CreateSwapchainKHR(device, &ci, pAllocator, pSwapchain) : VK_ERROR_INITIALIZATION_FAILED;
        if (r == VK_SUCCESS && pSwapchain)
        {
            g.swapchain.handle = *pSwapchain;
            g.swapchain.format = ci.imageFormat;
            g.swapchain.width = ci.imageExtent.width;
            g.swapchain.height = ci.imageExtent.height;
            g.swapchain.images.clear();
            if (g.GetSwapchainImagesKHR)
            {
                uint32_t n = 0;
                g.GetSwapchainImagesKHR(device, *pSwapchain, &n, nullptr);
                g.swapchain.images.resize(n);
                if (n)
                    g.GetSwapchainImagesKHR(device, *pSwapchain, &n, g.swapchain.images.data());
            }
        }
        return r;
    }

    void VKAPI_PTR HkDestroySwapchainKHR(VkDevice device, VkSwapchainKHR swapchain, const void* pAllocator)
    {
        if (g.swapchain.handle == swapchain)
        {
            g.swapchain = {};
            if (g.ngxFeature && g.ngxRelease)
            {
                g.ngxRelease(g.ngxFeature);
                g.ngxFeature = nullptr;
            }
        }
        if (g.DestroySwapchainKHR)
            g.DestroySwapchainKHR(device, swapchain, pAllocator);
    }

    void VKAPI_PTR HkCmdBlitImage(VkCommandBuffer cmd, VkImage src, VkImageLayout srcL, VkImage dst, VkImageLayout dstL, uint32_t n, const VkImageBlit* r, VkFilter f)
    {
        if (g.mode != FusionFixDLSSMode_Off)
        {
            for (auto img : g.swapchain.images)
            {
                if (img == dst)
                {
                    g.lastBlitSrc = src;
                    return; // skip linear blit; NGX evaluate replaces it
                }
            }
        }
        if (g.CmdBlitImage)
            g.CmdBlitImage(cmd, src, srcL, dst, dstL, n, r, f);
    }

    void VKAPI_PTR HkCmdBeginRenderPass(VkCommandBuffer cmd, const VkRenderPassBeginInfo* info, uint32_t contents)
    {
        if (info && info->renderArea.width >= 320 && info->renderArea.height >= 240)
        {
            // Depth is resolved via tracked vkCreateImage records matching this size.
            for (auto& im : g.images)
            {
                if (im.depth && im.width == info->renderArea.width && im.height == info->renderArea.height)
                {
                    g.lastDepth = im.image;
                    g.lastDepthFormat = im.format;
                    g.lastDepthW = im.width;
                    g.lastDepthH = im.height;
                }
            }
        }
        if (g.CmdBeginRenderPass)
            g.CmdBeginRenderPass(cmd, info, contents);
    }

    VkResult VKAPI_PTR HkCreateImage(VkDevice device, const VkImageCreateInfo* ci, const void* a, VkImage* out)
    {
        auto r = g.CreateImage ? g.CreateImage(device, ci, a, out) : VK_ERROR_INITIALIZATION_FAILED;
        if (r == VK_SUCCESS && ci && out)
        {
            ImageInfo info{};
            info.image = *out;
            info.format = ci->format;
            info.width = ci->extent.width;
            info.height = ci->extent.height;
            info.usage = ci->usage;
            info.depth = FusionFixDLSS_FormatIsDepth(ci->format) || (ci->usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
            g.images.push_back(info);
            if (g.images.size() > 256)
                g.images.erase(g.images.begin(), g.images.begin() + 64);
        }
        return r;
    }

    VkResult VKAPI_PTR HkQueuePresentKHR(VkQueue queue, const VkPresentInfoKHR* info)
    {
        if (g.mode != FusionFixDLSSMode_Off && info && info->swapchainCount)
        {
            if (!g.queue)
                g.queue = queue;
            const uint32_t idx = info->pImageIndices ? info->pImageIndices[0] : 0;
            VkImage out = 0;
            if (idx < g.swapchain.images.size())
                out = g.swapchain.images[idx];

            VkImage color = g.lastBlitSrc;
            if (!color)
            {
                // Same-res path: use a recent color image matching render/display size.
                for (auto it = g.images.rbegin(); it != g.images.rend(); ++it)
                {
                    if (!it->depth && (it->usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) && it->width >= 320)
                    {
                        color = it->image;
                        break;
                    }
                }
            }
            if (!color && out)
                color = out;

            const uint32_t outW = g.swapchain.width ? g.swapchain.width : g.displayW;
            const uint32_t outH = g.swapchain.height ? g.swapchain.height : g.displayH;
            uint32_t rw = g.renderW ? g.renderW : outW;
            uint32_t rh = g.renderH ? g.renderH : outH;

            VkFormat colorFmt = VK_FORMAT_B8G8R8A8_UNORM;
            for (auto& im : g.images)
            {
                if (im.image == color)
                {
                    colorFmt = im.format;
                    if (im.width && im.height)
                    {
                        rw = im.width;
                        rh = im.height;
                    }
                    break;
                }
            }

            if (out && color)
            {
                if (!EvaluateDlss(color, colorFmt, g.lastDepth, g.lastDepthFormat, out, g.swapchain.format, rw, rh, outW, outH))
                {
                    if (g.status != FusionFixDLSSStatus_NgxError && g.status != FusionFixDLSSStatus_NoNgx)
                        SetStatus(FusionFixDLSSStatus_NoSwapchain);
                }
            }
            else
            {
                SetStatus(FusionFixDLSSStatus_NoSwapchain);
            }
        }
        return g.QueuePresentKHR ? g.QueuePresentKHR(queue, info) : VK_ERROR_UNKNOWN;
    }

    PFN_vkVoidFunction VKAPI_PTR WrapGipa(VkInstance instance, const char* name)
    {
        auto p = g.gipa ? g.gipa(instance, name) : nullptr;
        if (!name)
            return p;
        if (std::strcmp(name, "vkCreateInstance") == 0)
        {
            g.CreateInstance = (PFN_vkCreateInstance)p;
            return (PFN_vkVoidFunction)HkCreateInstance;
        }
        if (std::strcmp(name, "vkCreateDevice") == 0)
        {
            g.CreateDevice = (PFN_vkCreateDevice)p;
            return (PFN_vkVoidFunction)HkCreateDevice;
        }
        if (std::strcmp(name, "vkGetDeviceProcAddr") == 0)
            return (PFN_vkVoidFunction)WrapGdpa;
        if (std::strcmp(name, "vkCreateSwapchainKHR") == 0)
        {
            g.CreateSwapchainKHR = (PFN_vkCreateSwapchainKHR)p;
            return (PFN_vkVoidFunction)HkCreateSwapchainKHR;
        }
        if (std::strcmp(name, "vkDestroySwapchainKHR") == 0)
        {
            g.DestroySwapchainKHR = (PFN_vkDestroySwapchainKHR)p;
            return (PFN_vkVoidFunction)HkDestroySwapchainKHR;
        }
        if (std::strcmp(name, "vkGetSwapchainImagesKHR") == 0)
        {
            g.GetSwapchainImagesKHR = (PFN_vkGetSwapchainImagesKHR)p;
            return p;
        }
        if (std::strcmp(name, "vkQueuePresentKHR") == 0)
        {
            g.QueuePresentKHR = (PFN_vkQueuePresentKHR)p;
            return (PFN_vkVoidFunction)HkQueuePresentKHR;
        }
        if (std::strcmp(name, "vkCmdBlitImage") == 0)
        {
            g.CmdBlitImage = (PFN_vkCmdBlitImage)p;
            return (PFN_vkVoidFunction)HkCmdBlitImage;
        }
        if (std::strcmp(name, "vkCmdBeginRenderPass") == 0)
        {
            g.CmdBeginRenderPass = (PFN_vkCmdBeginRenderPass)p;
            return (PFN_vkVoidFunction)HkCmdBeginRenderPass;
        }
        if (std::strcmp(name, "vkCreateImage") == 0)
        {
            g.CreateImage = (PFN_vkCreateImage)p;
            return (PFN_vkVoidFunction)HkCreateImage;
        }
        return p;
    }

    PFN_vkVoidFunction VKAPI_PTR WrapGdpa(VkDevice device, const char* name)
    {
        PFN_vkGetDeviceProcAddr real = g.gdpa;
        if (!real && g.gipa)
            real = (PFN_vkGetDeviceProcAddr)g.gipa(g.instance, "vkGetDeviceProcAddr");
        auto p = real ? real(device, name) : nullptr;
        if (!name)
            return p;
        if (std::strcmp(name, "vkCreateSwapchainKHR") == 0)
        {
            g.CreateSwapchainKHR = (PFN_vkCreateSwapchainKHR)p;
            return (PFN_vkVoidFunction)HkCreateSwapchainKHR;
        }
        if (std::strcmp(name, "vkDestroySwapchainKHR") == 0)
        {
            g.DestroySwapchainKHR = (PFN_vkDestroySwapchainKHR)p;
            return (PFN_vkVoidFunction)HkDestroySwapchainKHR;
        }
        if (std::strcmp(name, "vkQueuePresentKHR") == 0)
        {
            g.QueuePresentKHR = (PFN_vkQueuePresentKHR)p;
            return (PFN_vkVoidFunction)HkQueuePresentKHR;
        }
        if (std::strcmp(name, "vkCmdBlitImage") == 0)
        {
            g.CmdBlitImage = (PFN_vkCmdBlitImage)p;
            return (PFN_vkVoidFunction)HkCmdBlitImage;
        }
        if (std::strcmp(name, "vkCmdBeginRenderPass") == 0)
        {
            g.CmdBeginRenderPass = (PFN_vkCmdBeginRenderPass)p;
            return (PFN_vkVoidFunction)HkCmdBeginRenderPass;
        }
        if (std::strcmp(name, "vkCreateImage") == 0)
        {
            g.CreateImage = (PFN_vkCreateImage)p;
            return (PFN_vkVoidFunction)HkCreateImage;
        }
        return p;
    }

    PFN_vkVoidFunction VKAPI_PTR HkGetInstanceProcAddr(VkInstance instance, const char* name)
    {
        if (!g.gipa && shGipa)
            g.gipa = (PFN_vkGetInstanceProcAddr)shGipa.unsafe_get();
        return WrapGipa(instance, name);
    }

    void HookVulkanModule(HMODULE mod)
    {
        if (!mod || shGipa)
            return;
        auto fn = GetProcAddress(mod, "vkGetInstanceProcAddr");
        if (!fn)
            return;
        g.gipa = (PFN_vkGetInstanceProcAddr)fn;
        shGipa = safetyhook::create_inline(fn, HkGetInstanceProcAddr);
        g.vulkanSeen = true;
    }

    void ApplyRenderScale(D3DPRESENT_PARAMETERS* pp)
    {
        if (!pp || g.mode == FusionFixDLSSMode_Off || !IsVulkanApi())
            return;
        uint32_t dw = pp->BackBufferWidth;
        uint32_t dh = pp->BackBufferHeight;
        if ((!dw || !dh) && pp->hDeviceWindow)
        {
            RECT rc{};
            GetClientRect(pp->hDeviceWindow, &rc);
            dw = uint32_t(rc.right - rc.left);
            dh = uint32_t(rc.bottom - rc.top);
        }
        if (dw < 320 || dh < 240)
            return;
        g.displayW = dw;
        g.displayH = dh;
        FusionFixDLSSMath::RenderSizeForMode(g.mode, dw, dh, g.renderW, g.renderH);
        pp->BackBufferWidth = g.renderW;
        pp->BackBufferHeight = g.renderH;
        g.resetHistory = 1;
        if (g.ngxFeature && g.ngxRelease)
        {
            g.ngxRelease(g.ngxFeature);
            g.ngxFeature = nullptr;
        }
    }

    HRESULT WINAPI HkCreateDevice(IDirect3D9* self, UINT adapter, D3DDEVTYPE type, HWND focus, DWORD flags, D3DPRESENT_PARAMETERS* pp, IDirect3DDevice9** out)
    {
        ApplyRenderScale(pp);
        auto hr = shCreateDevice.unsafe_stdcall<HRESULT>(self, adapter, type, focus, flags, pp, out);
        if (SUCCEEDED(hr) && out && *out && !shReset)
        {
            auto* vtbl = *reinterpret_cast<void***>(*out);
            shReset = safetyhook::create_inline(vtbl[kD3D9ResetIndex], +[](IDirect3DDevice9* dev, D3DPRESENT_PARAMETERS* p) -> HRESULT {
                ApplyRenderScale(p);
                return shReset.unsafe_stdcall<HRESULT>(dev, p);
            });
            shSetTransform = safetyhook::create_inline(vtbl[kD3D9SetTransformIndex], +[](IDirect3DDevice9* dev, D3DTRANSFORMSTATETYPE state, const D3DMATRIX* m) -> HRESULT {
                if (m && g.mode != FusionFixDLSSMode_Off)
                {
                    if (state == D3DTS_VIEW)
                        std::memcpy(g.view.m, m, 64);
                    if (state == D3DTS_PROJECTION)
                    {
                        D3DMATRIX j = *m;
                        if (g.renderW && g.renderH)
                            FusionFixDLSSMath::ApplyProjectionJitter(*reinterpret_cast<Mat4*>(&j), g.jitterX, g.jitterY, float(g.renderW), float(g.renderH));
                        return shSetTransform.unsafe_stdcall<HRESULT>(dev, state, &j);
                    }
                }
                return shSetTransform.unsafe_stdcall<HRESULT>(dev, state, m);
            });
            shSetVSConstF = safetyhook::create_inline(vtbl[kD3D9SetVSConstFIndex], +[](IDirect3DDevice9* dev, UINT start, const float* data, UINT count) -> HRESULT {
                if (g.mode != FusionFixDLSSMode_Off && data && count >= 4)
                {
                    thread_local std::vector<float> tmp;
                    tmp.assign(data, data + count * 4);
                    ApplyJitterToConstants(tmp.data(), count);
                    return shSetVSConstF.unsafe_stdcall<HRESULT>(dev, start, tmp.data(), count);
                }
                return shSetVSConstF.unsafe_stdcall<HRESULT>(dev, start, data, count);
            });
            g.deviceHooked = true;
        }
        return hr;
    }

    IDirect3D9* WINAPI HkDirect3DCreate9(UINT sdk)
    {
        auto* d3d = shDirect3DCreate9.unsafe_stdcall<IDirect3D9*>(sdk);
        if (d3d && !shCreateDevice)
        {
            auto* vtbl = *reinterpret_cast<void***>(d3d);
            shCreateDevice = safetyhook::create_inline(vtbl[kD3D9CreateDeviceIndex], HkCreateDevice);
        }
        return d3d;
    }

    void HookD3D9Module(HMODULE mod)
    {
        if (!mod || shDirect3DCreate9)
            return;
        auto fn = GetProcAddress(mod, "Direct3DCreate9");
        if (!fn)
            return;
        shDirect3DCreate9 = safetyhook::create_inline(fn, HkDirect3DCreate9);
    }

    bool NameIs(const wchar_t* path, const wchar_t* file)
    {
        if (!path || !file)
            return false;
        const wchar_t* base = path;
        for (const wchar_t* p = path; *p; ++p)
            if (*p == L'\\' || *p == L'/')
                base = p + 1;
        return _wcsicmp(base, file) == 0;
    }

    HMODULE WINAPI HkLoadLibraryW(LPCWSTR name)
    {
        auto m = shLoadLibraryW.stdcall<HMODULE>(name);
        if (m && name)
        {
            if (NameIs(name, L"vulkan-1.dll") || NameIs(name, L"winevulkan.dll"))
                HookVulkanModule(m);
            if (NameIs(name, L"d3d9.dll") || NameIs(name, L"vulkan.dll"))
                HookD3D9Module(m);
        }
        return m;
    }

    HMODULE WINAPI HkLoadLibraryExW(LPCWSTR name, HANDLE h, DWORD flags)
    {
        auto m = shLoadLibraryExW.stdcall<HMODULE>(name, h, flags);
        if (m && name)
        {
            if (NameIs(name, L"vulkan-1.dll") || NameIs(name, L"winevulkan.dll"))
                HookVulkanModule(m);
            if (NameIs(name, L"d3d9.dll") || NameIs(name, L"vulkan.dll"))
                HookD3D9Module(m);
        }
        return m;
    }

    void OnModeChanged(int32_t value)
    {
        std::lock_guard lock(g.mutex);
        g.mode = value;
        g.resetHistory = 1;
        WriteD3d9Dlss(value);
        if (g.ngxFeature && g.ngxRelease)
        {
            g.ngxRelease(g.ngxFeature);
            g.ngxFeature = nullptr;
        }
        if (value == FusionFixDLSSMode_Off)
            SetStatus(FusionFixDLSSStatus_Idle);
        else
        {
            ForcePostFxOff();
            if (!IsVulkanApi())
                SetStatus(FusionFixDLSSStatus_NoVulkan);
            else if (!LoadNgx())
                SetStatus(FusionFixDLSSStatus_NoNgx);
            else
                SetStatus(g.ngxInited ? FusionFixDLSSStatus_Ok : FusionFixDLSSStatus_Idle);
        }
    }
}

extern "C"
{
    const wchar_t* FusionFixDLSS_GetMenuLabel(void)
    {
        return g.label;
    }

    int FusionFixDLSS_ShouldForcePostFxOff(void)
    {
        return g.mode != FusionFixDLSSMode_Off ? 1 : 0;
    }

    int FusionFixDLSS_GetMode(void)
    {
        return g.mode;
    }

    int FusionFixDLSS_GetStatus(void)
    {
        return g.status;
    }

    unsigned int FusionFixDLSS_GetNgxResult(void)
    {
        return g.ngxResult;
    }
}

class FusionFixDLSS
{
public:
    FusionFixDLSS()
    {
        FusionFix::onInitEvent() += []()
        {
            auto k32 = GetModuleHandleW(L"kernel32.dll");
            if (k32)
            {
                if (auto p = GetProcAddress(k32, "LoadLibraryW"))
                    shLoadLibraryW = safetyhook::create_inline(p, HkLoadLibraryW);
                if (auto p = GetProcAddress(k32, "LoadLibraryExW"))
                    shLoadLibraryExW = safetyhook::create_inline(p, HkLoadLibraryExW);
            }
            if (auto vk = GetModuleHandleW(L"vulkan-1.dll"))
                HookVulkanModule(vk);
            if (auto d3d = GetModuleHandleW(L"d3d9.dll"))
                HookD3D9Module(d3d);
            if (auto dxvk = GetModuleHandleW(L"vulkan.dll"))
                HookD3D9Module(dxvk);
        };

        FusionFix::onInitEventAsync() += []()
        {
            FusionFixSettings.SetCallback("PREF_DLSS", OnModeChanged);
            g.mode = FusionFixSettings.Get("PREF_DLSS");
            OnModeChanged(g.mode);
        };

        FusionFix::onGameProcessEvent() += []()
        {
            if (g.mode == FusionFixDLSSMode_Off)
                return;
            ForcePostFxOff();
            AdvanceJitter();
            UpdateCameraMatrices();
            std::lock_guard lock(g.mutex);
            RefreshStatusLocked();
        };

        FusionFix::onMenuDrawingEvent() += []()
        {
            std::lock_guard lock(g.mutex);
            if (g.mode != FusionFixDLSSMode_Off)
            {
                ForcePostFxOff();
                if (!IsVulkanApi())
                    SetStatus(FusionFixDLSSStatus_NoVulkan);
                else if (g.status == FusionFixDLSSStatus_Idle)
                    RefreshStatusLocked();
            }
            else
                SetStatus(FusionFixDLSSStatus_Idle);
        };

        FusionFix::onBeforeReset() += []()
        {
            g.resetHistory = 1;
        };

        FusionFix::onShutdownEvent() += []()
        {
            ShutdownNgx();
            DestroyCompute();
        };
    }
} FusionFixDLSS;
