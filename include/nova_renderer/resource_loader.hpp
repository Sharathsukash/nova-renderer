#pragma once

#include <cstddef>
#include <unordered_map>

#include "nova_renderer/memory/allocators.hpp"
#include "nova_renderer/rhi/device_memory_resource.hpp"
#include "nova_renderer/rhi/forward_decls.hpp"
#include "nova_renderer/rhi/rhi_types.hpp"
#include "nova_renderer/util/container_accessor.hpp"

namespace nova::mem {
    class Bytes;
}

namespace nova::renderer {
    class NovaRenderer;

    namespace rhi {
        enum class PixelFormat;
    }

    namespace shaderpack {
        // yolo
        enum class PixelFormatEnum;
    } // namespace shaderpack

    struct TextureResource {
        std::string name;

        rhi::Image* image = nullptr;

        size_t width;

        size_t height;

        rhi::PixelFormat format;
    };

    struct BufferResource {
        std::string name;

        rhi::Buffer* buffer = nullptr;

        mem::Bytes size = 0;
    };

    using TextureResourceAccessor = MapAccessor<std::string, TextureResource>;

    using BufferResourceAccessor = MapAccessor<std::string, BufferResource>;

    /*!
     * \brief Provides a means to access Nova's resources, and also helps in creating resources? IDK yet but that's fine
     *
     * Basically I need both a high-level API to make resources with, and I want to make those resource easy to access.
     */
    class DeviceResources {
    public:
        explicit DeviceResources(NovaRenderer& renderer);

        [[nodiscard]] std::optional<BufferResourceAccessor> create_uniform_buffer(const std::string& name, mem::Bytes size);

        [[nodiscard]] std::optional<BufferResourceAccessor> get_uniform_buffer(const std::string& name);

        void destroy_uniform_buffer(const std::string& name);

        /*!
         * \brief Creates a new dynamic texture with the provided initial texture data
         *
         * \param name The name of the texture. After the texture can been created, you can use this to refer to it
         * \param width The width of the texture
         * \param height The height of the texture
         * \param pixel_format The format of the pixels in this texture
         * \param data The initial data for this texture. Must be large enough to have all the pixels in the texture
         * \param allocator The allocator to allocate with
         * \return The newly-created image, or nullptr if the image could not be created. Check the Nova logs to find out why
         */
        [[nodiscard]] std::optional<TextureResourceAccessor> create_texture(const std::string& name,
                                                                            std::size_t width,
                                                                            std::size_t height,
                                                                            rhi::PixelFormat pixel_format,
                                                                            const void* data,
                                                                            mem::AllocatorHandle<>& allocator);

        /*!
         * \brief Retrieves the texture with the specified name
         */
        [[nodiscard]] std::optional<TextureResourceAccessor> get_texture(const std::string& name) const;

        /*!
         * \brief Creates a new render target with the specified size and format
         *
         * Render targets reside completely on the GPU and are not accessible from the CPU. If you need a shader-writable, CPU-readable
         * texture, create a readback texture instead
         *
         * By default a render target may not be sampled by a shader
         *
         * \param name The name of the render target
         * \param width The width of the render target, in pixels
         * \param height The height of the render target, in pixels
         * \param pixel_format The format of the render target
         * \param allocator The allocator to use for any host memory this methods needs to allocate
         * \param can_be_sampled If true, the render target may be sampled by a shader. If false, this render target may only be presented
         * to the screen
         *
         * \return The new render target if it could be created, or am empty optional if it could not
         */
        [[nodiscard]] std::optional<TextureResourceAccessor> create_render_target(const std::string& name,
                                                                                  size_t width,
                                                                                  size_t height,
                                                                                  rhi::PixelFormat pixel_format,
                                                                                  mem::AllocatorHandle<>& allocator,
                                                                                  bool can_be_sampled = false);

        /*!
         * \brief Retrieves the render target with the specified name
         */
        [[nodiscard]] std::optional<TextureResourceAccessor> get_render_target(const std::string& name) const;

        void destroy_render_target(const std::string& texture_name, mem::AllocatorHandle<>& allocator);

        NovaRenderer& renderer;

        rhi::RenderDevice& device;

        std::unique_ptr<mem::AllocatorHandle<>> allocator;

        std::unique_ptr<mem::AllocatorHandle<>> staging_buffer_allocator;

        std::unordered_map<std::string, TextureResource> textures;

        std::unordered_map<std::string, TextureResource> render_targets;

        DeviceMemoryResource* staging_buffer_memory;

        std::unordered_map<size_t, std::vector<rhi::Buffer*>> staging_buffers;

        DeviceMemoryResource* uniform_buffer_memory;

        std::unordered_map<std::string, BufferResource> uniform_buffers;

        void allocate_staging_buffer_memory();

        void allocate_uniform_buffer_memory();

        /*!
         * \brief Retrieves a staging buffer at least the specified size
         *
         * The actual buffer returned may be larger than what you need
         */
        std::shared_ptr<rhi::Buffer> get_staging_buffer_with_size(size_t size);
    };
} // namespace nova::renderer
