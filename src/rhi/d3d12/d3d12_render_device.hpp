#pragma once

#include <D3D12MemAlloc.h>
#include <d3d12.h>
#include <d3d12shader.h>
#include <dxc/dxcapi.h>
#include <dxgi.h>
#include <spirv_hlsl.hpp>
#include <wrl/client.h>

#include "nova_renderer/rhi/render_device.hpp"

#include "descriptor_allocator.hpp"

namespace nova::renderer::rhi {
    class D3D12RenderDevice final : public RenderDevice {
    public:
        D3D12RenderDevice(NovaSettingsAccessManager& settings, NovaWindow& window, rx::memory::allocator& allocator);

        ~D3D12RenderDevice() override;

#pragma region RenderDevice
        void set_num_renderpasses(uint32_t num_renderpasses) override;

        [[nodiscard]] ntl::Result<RhiRenderpass*> create_renderpass(const renderpack::RenderPassCreateInfo& data,
                                                                    const glm::uvec2& framebuffer_size,
                                                                    rx::memory::allocator& allocator) override;

        [[nodiscard]] RhiFramebuffer* create_framebuffer(const RhiRenderpass* renderpass,
                                                         const rx::vector<RhiImage*>& color_attachments,
                                                         const rx::optional<RhiImage*> depth_attachment,
                                                         const glm::uvec2& framebuffer_size,
                                                         rx::memory::allocator& allocator) override;

        [[nodiscard]] rx::ptr<RhiPipeline> create_surface_pipeline(const RhiGraphicsPipelineState& pipeline_state,
                                                                   rx::memory::allocator& allocator) override;

        [[nodiscard]] rx::ptr<RhiPipeline> create_global_pipeline(const RhiGraphicsPipelineState& pipeline_state,
                                                                  rx::memory::allocator& allocator) override;

        [[nodiscard]] rx::ptr<RhiResourceBinder> create_resource_binder_for_pipeline(const RhiPipeline& pipeline,
                                                                                     rx::memory::allocator& allocator) override;

        [[nodiscard]] RhiBuffer* create_buffer(const RhiBufferCreateInfo& info, rx::memory::allocator& allocator) override;

        void write_data_to_buffer(const void* data, mem::Bytes num_bytes, const RhiBuffer* buffer) override;

        [[nodiscard]] RhiSampler* create_sampler(const RhiSamplerCreateInfo& create_info, rx::memory::allocator& allocator) override;

        [[nodiscard]] RhiImage* create_image(const renderpack::TextureCreateInfo& info, rx::memory::allocator& allocator) override;

        [[nodiscard]] RhiSemaphore* create_semaphore(rx::memory::allocator& allocator) override;

        [[nodiscard]] rx::vector<RhiSemaphore*> create_semaphores(uint32_t num_semaphores, rx::memory::allocator& allocator) override;

        [[nodiscard]] RhiFence* create_fence(bool signaled, rx::memory::allocator& allocator) override;

        [[nodiscard]] rx::vector<RhiFence*> create_fences(uint32_t num_fences, bool signaled, rx::memory::allocator& allocator) override;

        void wait_for_fences(rx::vector<RhiFence*> fences) override;

        void reset_fences(const rx::vector<RhiFence*>& fences) override;

        void destroy_renderpass(RhiRenderpass* pass, rx::memory::allocator& allocator) override;

        void destroy_framebuffer(RhiFramebuffer* framebuffer, rx::memory::allocator& allocator) override;

        void destroy_texture(RhiImage* resource, rx::memory::allocator& allocator) override;

        void destroy_semaphores(rx::vector<RhiSemaphore*>& semaphores, rx::memory::allocator& allocator) override;

        void destroy_fences(const rx::vector<RhiFence*>& fences, rx::memory::allocator& allocator) override;

        RhiRenderCommandList* create_command_list(uint32_t thread_idx,
                                                  QueueType needed_queue_type,
                                                  RhiRenderCommandList::Level level,
                                                  rx::memory::allocator& allocator) override;

        void submit_command_list(RhiRenderCommandList* cmds,
                                 QueueType queue,
                                 RhiFence* fence_to_signal,
                                 const rx::vector<RhiSemaphore*>& wait_semaphores,
                                 const rx::vector<RhiSemaphore*>& signal_semaphores) override;

        void end_frame(FrameContext& ctx) override;
#pragma endregion

    private:
        Microsoft::WRL::ComPtr<IDXGIFactory> factory;

        Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;

        Microsoft::WRL::ComPtr<ID3D12Device> device;

        Microsoft::WRL::ComPtr<ID3D12Debug> debug_controller;

        Microsoft::WRL::ComPtr<ID3D12InfoQueue> info_queue;

        Microsoft::WRL::ComPtr<ID3D12CommandQueue> graphics_queue;

        Microsoft::WRL::ComPtr<ID3D12CommandQueue> dma_queue;

        rx::vector<spirv_cross::HLSLResourceBinding> standard_hlsl_bindings;
        Microsoft::WRL::ComPtr<ID3D12RootSignature> standard_root_signature;

        rx::ptr<DescriptorAllocator> shader_resource_descriptors;
        rx::ptr<DescriptorAllocator> render_target_descriptors;
        rx::ptr<DescriptorAllocator> depth_stencil_descriptors;

        D3D12MA::Allocator* dma_allocator;

        Microsoft::WRL::ComPtr<IDxcLibrary> dxc_library;

        Microsoft::WRL::ComPtr<IDxcCompiler> dxc_compiler;

        /*!
         * \brief Indicates whether this device has a Unified Memory Architecture
         *
         * UMA devices don't need to use a transfer queue to upload data, they can map a pointer directly to all resources
         */
        bool is_uma = false;

        /*!
         * \brief Indicates the level of hardware and driver support for render passes
         *
         * Tier 0 - No support, don't use renderpasses
         * Tier 1 - render targets and depth/stencil writes should use renderpasses, but UAV writes are not supported
         * Tire 2 - render targets, depth/stencil, and UAV writes should use renderpasses
         */
        D3D12_RENDER_PASS_TIER render_pass_tier = D3D12_RENDER_PASS_TIER_0;

        /*!
         * \brief Indicates support the the DXR API
         *
         * If this is `false`, the user will be unable to use any DXR shaderpacks
         */
        bool has_raytracing = false;

#pragma region Initialization
        void enable_validation_layer();

        void initialize_dxgi();

        void select_adapter();

        void create_queues();

        void create_standard_root_signature();

        [[nodiscard]] rx::ptr<DescriptorAllocator> create_descriptor_allocator(D3D12_DESCRIPTOR_HEAP_TYPE type, UINT num_descriptors) const;

        void create_descriptor_heaps();

        void initialize_dma();

        void initialize_standard_resource_binding_mappings();

        void create_shader_compiler();
#pragma endregion

#pragma region Helpers
        [[nodiscard]] Microsoft::WRL::ComPtr<IDxcBlob> compile_spirv_to_dxil(const rx::vector<uint32_t>& spirv,
                                                                             LPCWSTR target_profile,
                                                                             const rx::string& pipeline_name);

        using RootSignatureWithDescriptors = rx::pair<rx::map<rx::string, D3D12_CPU_DESCRIPTOR_HANDLE>,
                                                      Microsoft::WRL::ComPtr<ID3D12RootSignature>>;

        [[nodiscard]] RootSignatureWithDescriptors create_root_signature(const rx::map<rx::string, D3D12_SHADER_INPUT_BIND_DESC>& bindings,
                                                                         rx::memory::allocator& allocator);

        [[nodiscard]] Microsoft::WRL::ComPtr<ID3D12RootSignature> compile_root_signature(
            const D3D12_ROOT_SIGNATURE_DESC& root_signature_desc) const;
#pragma endregion
    };
} // namespace nova::renderer::rhi
