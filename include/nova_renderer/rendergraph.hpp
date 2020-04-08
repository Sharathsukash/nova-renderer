#pragma once

#include <rx/core/log.h>
#include <rx/core/map.h>
#include <rx/core/optional.h>

#include "nova_renderer/frame_context.hpp"
#include "nova_renderer/procedural_mesh.hpp"
#include "nova_renderer/renderables.hpp"
#include "nova_renderer/renderpack_data.hpp"
#include "nova_renderer/rhi/pipeline_create_info.hpp"
#include "nova_renderer/rhi/render_device.hpp"
#include "nova_renderer/rhi/rhi_types.hpp"
#include "nova_renderer/rhi/swapchain.hpp"
#include "nova_renderer/util/container_accessor.hpp"

#include "resource_loader.hpp"

namespace nova::renderer {
    RX_LOG("rendergraph", rg_log);

    class DeviceResources;

    namespace renderpack {
        struct RenderPassCreateInfo;
    }

#pragma region Metadata structs
    struct FullMaterialPassName {
        rx::string material_name;
        rx::string pass_name;

        bool operator==(const FullMaterialPassName& other) const;

        rx_size hash() const;
    };

    struct MaterialPassKey {
        rx::string pipeline_name;
        uint32_t material_pass_index;
    };

    struct MaterialPassMetadata {
        renderpack::MaterialPass data;
    };

    struct PipelineMetadata {
        RhiGraphicsPipelineState data;

        rx::map<FullMaterialPassName, MaterialPassMetadata> material_metadatas{};
    };

    struct RenderpassMetadata {
        renderpack::RenderPassCreateInfo data;
    };
#pragma endregion

#pragma region Structs for rendering
    template <typename RenderCommandType>
    struct MeshBatch {
        size_t num_vertex_attributes{};
        uint32_t num_indices{};

        rhi::RhiBuffer* vertex_buffer = nullptr;
        rhi::RhiBuffer* index_buffer = nullptr;

        /*!
         * \brief A buffer to hold all the per-draw data
         *
         * For example, a non-animated mesh just needs a mat4 for its model matrix
         *
         * This buffer gets re-written to every frame, since the number of renderables in this mesh batch might have changed. If there's
         * more renderables than the buffer can hold, it gets reallocated from the RHI
         */
        rhi::RhiBuffer* per_renderable_data = nullptr;

        rx::vector<RenderCommandType> commands;
    };

    template <typename RenderCommandType>
    struct ProceduralMeshBatch {
        MapAccessor<MeshId, ProceduralMesh> mesh;

        /*!
         * \brief A buffer to hold all the per-draw data
         *
         * For example, a non-animated mesh just needs a mat4 for its model matrix
         *
         * This buffer gets re-written to every frame, since the number of renderables in this mesh batch might have changed. If there's
         * more renderables than the buffer can hold, it gets reallocated from the RHI
         */
        rhi::RhiBuffer* per_renderable_data = nullptr;

        rx::vector<RenderCommandType> commands;

        ProceduralMeshBatch(rx::map<MeshId, ProceduralMesh>* meshes, const MeshId key) : mesh(meshes, key) {}
    };

    struct MaterialPass {
        FullMaterialPassName name;

        rx::vector<MeshBatch<StaticMeshRenderCommand>> static_mesh_draws;
        rx::vector<ProceduralMeshBatch<StaticMeshRenderCommand>> static_procedural_mesh_draws;

        rx::vector<rhi::RhiDescriptorSet*> descriptor_sets;
        const rhi::RhiPipelineInterface* pipeline_interface = nullptr;

        void record(rhi::RhiRenderCommandList& cmds, FrameContext& ctx) const;

        void record_rendering_static_mesh_batch(const MeshBatch<StaticMeshRenderCommand>& batch,
                                                rhi::RhiRenderCommandList& cmds,
                                                FrameContext& ctx) const;

        void record_rendering_static_mesh_batch(const ProceduralMeshBatch<StaticMeshRenderCommand>& batch,
                                                rhi::RhiRenderCommandList& cmds,
                                                FrameContext& ctx) const;
    };

    struct Pipeline {
        rx::ptr<rhi::RhiPipeline> pipeline{};
        rhi::RhiPipelineInterface* pipeline_interface = nullptr;

        void record(rhi::RhiRenderCommandList& cmds, FrameContext& ctx) const;
    };
#pragma endregion

    /*!
     * \brief Renderpass that's ready to be recorded into a command list
     *
     * Renderpass has two virtual methods: `record` and `record_inside_renderpass`. `record` records the renderpass in its entirety, while
     * `record_inside_renderpass` only records the inside of the renderpass, not the work needed to begin or end it. I expect that most
     * subclasses will only want to override `record_inside_renderpass`
     */
    class Renderpass {
    public:
        explicit Renderpass(rx::string name_in, bool is_builtin_in = false);

        Renderpass(Renderpass&& old) noexcept = default;
        Renderpass& operator=(Renderpass&& old) noexcept = default;

        Renderpass(const Renderpass& other) = delete;
        Renderpass& operator=(const Renderpass& other) = delete;

        virtual ~Renderpass() = default;

        uint32_t id = 0;
        rx::string name;

        bool is_builtin = false;

        rx::ptr<rhi::RhiRenderpass> renderpass;
        rx::ptr<rhi::RhiFramebuffer> framebuffer;

        /*!
         * \brief Names of all the pipelines which are in this renderpass
         */
        rx::vector<rx::string> pipeline_names;

        bool writes_to_backbuffer = false;

        rx::vector<rhi::RhiResourceBarrier> read_texture_barriers;
        rx::vector<rhi::RhiResourceBarrier> write_texture_barriers;

        /*!
         * \brief Performs the rendering work of this renderpass
         *
         * Custom renderpasses can override this method to perform custom rendering. However, I recommend that you override
         * `render_renderpass_contents` instead. A typical renderpass will need to issue barriers for the resource it uses, and
         * the default renderpass implementation calls `render_renderpass_contents` after issuing those barriers
         *
         * \param cmds The command list that this renderpass should record all its rendering commands into. You may record secondary command
         * lists in multiple threads and execute them with this command list, if you want
         *
         * \param ctx The context for the current frame. Contains information about the available resources, the current frame, and
         * everything you should need to render. If there's something you need that isn't in the frame context, submit an issue on the Nova
         * GitHub
         */
        virtual void execute(rhi::RhiRenderCommandList& cmds, FrameContext& ctx);

        /*!
         * \brief Returns the framebuffer that this renderpass should render to
         */
        [[nodiscard]] rhi::RhiFramebuffer& get_framebuffer(const FrameContext& ctx) const;

    protected:
        /*!
         * \brief Records all the resource barriers that need to take place before this renderpass renders anything
         *
         * By default `render` calls this method before calling `setup_renderpass`. If you override `render`, you'll need to call
         * this method yourself before using any of this renderpass's resources
         */
        virtual void record_pre_renderpass_barriers(rhi::RhiRenderCommandList& cmds, FrameContext& ctx) const;

        /*!
         * \brief Allows a renderpass to perform work before the recording of the actual renderpass
         *
         * This is useful for e.g. uploading streamed in vertex data
         *
         * The default `render` method calls this after `record_pre_renderpass_barriers` and before `record_renderpass_contents`
         */
        virtual void setup_renderpass(rhi::RhiRenderCommandList& cmds, FrameContext& ctx);

        /*!
         * \brief Renders the contents of this renderpass
         *
         * The default `render` method calls this method after `record_pre_renderpass_barriers` and before
         * `record_post_renderpass_barriers`. Thus, I recommend that you override this method instead of `render` - you'll have fewer things
         * to worry about
         *
         * \param cmds The command list that this renderpass should record all its rendering commands into. You may record secondary command
         * lists in multiple threads and execute them with this command list, if you want
         *
         * \param ctx The context for the current frame. Contains information about the available resources, the current frame, and
         * everything you should need to render. If there's something you need that isn't in the frame context, submit an issue on the Nova
         * GitHub
         */
        virtual void record_renderpass_contents(rhi::RhiRenderCommandList& cmds, FrameContext& ctx);

        /*!
         * \brief Records all the resource barriers that need to take place after this renderpass renders anything
         *
         * By default `render` calls this method after calling `render_renderpass_contents`. If you override `render`, you'll need to call
         * this method yourself near the end of your `render` method
         */
        virtual void record_post_renderpass_barriers(rhi::RhiRenderCommandList& cmds, FrameContext& ctx) const;
    };

    // Intentionally using a C enum because this is actually a bitmask
    enum ObjectType {
        OpaqueSurface = 0x1,
        TransparentSurface = 0x2,
        Particle = 0x4,
        Volume = 0x8,
    };

    /*!
     * \brief A renderpass that draws objects in a scene
     *
     * Scene renderpasses have some information about which kinds of objects they draw - transparent, opaque, particles, ets
     */
    class SceneRenderpass : public Renderpass {
    public:
        /*!
         * \brief Draws this render pass's objects
         */
        void record_renderpass_contents(rhi::RhiRenderCommandList& cmds, FrameContext& ctx) override;

    private:
        ObjectType drawn_objects;
    };

    /*!
     * \brief A renderpass that doesn't operate on a specific object, but rather on data that's accessible for the whole scene
     *
     * Examples: light culling in a forward+ renderer, lighting in a deferred renderer, or post-processing
     *
     * Global renderpasses typically only execute one graphics pipeline, and they do it across the entire scene. They operate on render
     * targets like the absolute chads they are
     */
    class GlobalRenderpass : public Renderpass {
    public:
        /*!
         * \brief Creates a new global render pass that will use the provided pipeline
         *
         * We use shader reflection to figure out which render targets the pipeline wants to use, then cache them from the device resources
         * object. This means that a renderpack's dynamic resources _MUST_ be created before its render graph
         *
         * \param name_in The name of this renderpass
         * \param pipeline_in The graphics pipeline state to use when executing this renderpass
         * \param mesh_in The mesh to execute this renderpass over. Will usually be the fullscreen triangle
         * \param is_builtin_in Whether this render pass is built in to Nova or comes from a renderpack
         */
        explicit GlobalRenderpass(const rx::string& name_in, rx::ptr<rhi::RhiPipeline> pipeline_in, MeshId mesh_in, bool is_builtin_in = false);

    protected:
        rx::ptr<rhi::RhiPipeline> pipeline;

        rx::ptr<RhiResourceBinder> resource_binder;

        MeshId mesh;

        /*!
         * \brief Issues a fullscreen drawcall that uses its resource binder and pipeline state
         */
        void record_renderpass_contents(rhi::RhiRenderCommandList& cmds, FrameContext& ctx) override;
    };

    /*!
     * \brief Represents Nova's rendergraph
     *
     * The rendergraph can change a lot over the runtime of Nova. Loading or unloading a renderpack will change the available passes, and
     * the order they're executed in
     */
    class Rendergraph {
    public:
        /*!
         * \brief Constructs a Rendergraph which will allocate its internal memory from the provided allocator, and which will execute on
         * the provided device
         */
        Rendergraph(rx::memory::allocator& allocator_in, rhi::RenderDevice& device_in);

        /*!
         * \brief Creates a new renderpass of the specified type using it's own create info
         *
         * This method calls a static method `RenderpassType::get_create_info` to get the renderpass's create info, and it allocates the new
         * renderpass from the rendergraph's internal allocator. Intended usage is adding renderpasses from C++ code - this method makes it
         * easy to define all your renderpass data in your C++ renderpass class
         *
         * This method creates all the GPU resources needed for the renderpass and it's framebuffer. It does not create any pipelines or
         * materials that may be rendered as part of this renderpass. You may create them through the rendergraph's JSON files, or through
         * the renderpass's constructor
         *
         * This method returns a pointer to the newly-created renderpass if everything went according to plan, or `nullptr` if it didn't
         *
         * Exact name and usage are still under revision, this is the alpha version of this method
         */
        template <typename RenderpassType, typename... Args>
        [[nodiscard]] RenderpassType* create_renderpass(DeviceResources& resource_storage, Args&&... args);

        /*!
         * \brief Adds an already-created renderpass with a specific create info
         *
         * This method initializes all the GPU resources needed for this renderpass and the framebuffer it renders to. It then adds the
         * renderpass to the appropriate places, returning a pointer to the renderpass you provided
         *
         * This method returns `nullptr` if the renderpass's GPU resources can't be initialized
         */
        template <typename RenderpassType>
        [[nodiscard]] RenderpassType* add_renderpass(RenderpassType* renderpass,
                                                     const renderpack::RenderPassCreateInfo& create_info,
                                                     DeviceResources& resource_storage);

        void destroy_renderpass(const rx::string& name);

        [[nodiscard]] rx::vector<rx::string> calculate_renderpass_execution_order();

        [[nodiscard]] Renderpass* get_renderpass(const rx::string& name) const;

        [[nodiscard]] rx::optional<RenderpassMetadata> get_metadata_for_renderpass(const rx::string& name) const;

    private:
        bool is_dirty = false;

        rx::memory::allocator& allocator;

        rhi::RenderDevice& device;

        rx::map<rx::string, Renderpass*> renderpasses;

        rx::vector<rx::string> cached_execution_order;
        rx::map<rx::string, RenderpassMetadata> renderpass_metadatas;
    };

    template <typename RenderpassType, typename... Args>
    RenderpassType* Rendergraph::create_renderpass(DeviceResources& resource_storage, Args&&... args) {

        auto* renderpass = allocator.create<RenderpassType>(rx::utility::forward<Args>(args)...);
        const auto& create_info = RenderpassType::get_create_info();

        return add_renderpass(renderpass, create_info, resource_storage);
    }

    template <typename RenderpassType>
    RenderpassType* Rendergraph::add_renderpass(RenderpassType* renderpass,
                                                const renderpack::RenderPassCreateInfo& create_info,
                                                DeviceResources& resource_storage) {
        RenderpassMetadata metadata;
        metadata.data = create_info;

        rx::vector<rhi::RhiImage*> color_attachments;
        color_attachments.reserve(create_info.texture_outputs.size());

        glm::uvec2 framebuffer_size(0);

        const auto num_attachments = create_info.depth_texture ? create_info.texture_outputs.size() + 1 :
                                                                 create_info.texture_outputs.size();
        rx::vector<rx::string> attachment_errors;
        attachment_errors.reserve(num_attachments);

        bool missing_render_targets = false;
        create_info.texture_outputs.each_fwd([&](const renderpack::TextureAttachmentInfo& attachment_info) {
            if(attachment_info.name == BACKBUFFER_NAME) {
                if(create_info.texture_outputs.size() == 1) {
                    renderpass->writes_to_backbuffer = true;
                    renderpass->framebuffer = nullptr; // Will be resolved when rendering

                } else {
                    attachment_errors.push_back(rx::string::format(
                        "Pass %s writes to the backbuffer and %zu other textures, but that's not allowed. If a pass writes to the backbuffer, it can't write to any other textures",
                        create_info.name,
                        create_info.texture_outputs.size() - 1));
                }

                framebuffer_size = device.get_swapchain()->get_size();

            } else {
                const auto render_target_opt = resource_storage.get_render_target(attachment_info.name);
                if(render_target_opt) {
                    const auto& render_target = *render_target_opt;

                    color_attachments.push_back(render_target->image.get());

                    const glm::uvec2 attachment_size = {render_target->width, render_target->height};
                    if(framebuffer_size.x > 0) {
                        if(attachment_size.x != framebuffer_size.x || attachment_size.y != framebuffer_size.y) {
                            attachment_errors.push_back(rx::string::format(
                                "Attachment %s has a size of %dx%d, but the framebuffer for pass %s has a size of %dx%d - these must match! All attachments of a single renderpass must have the same size",
                                attachment_info.name,
                                attachment_size.x,
                                attachment_size.y,
                                create_info.name,
                                framebuffer_size.x,
                                framebuffer_size.y));
                        }

                    } else {
                        framebuffer_size = attachment_size;
                    }

                } else {
                    rg_log->error("No render target named %s", attachment_info.name);
                    missing_render_targets = true;
                }
            }
        });

        if(missing_render_targets) {
            return nullptr;
        }

        const auto depth_attachment = [&]() -> rx::ptr<rhi::RhiImage> {
            if(create_info.depth_texture) {
                if(auto depth_tex = resource_storage.get_render_target(create_info.depth_texture->name); depth_tex) {
                    return rx::utility::move((*depth_tex)->image);
                }
            }

            return {};
        }();

        if(!attachment_errors.is_empty()) {
            attachment_errors.each_fwd([&](const rx::string& err) { rg_log->error("%s", err); });

            rg_log->error(
                "Could not create renderpass %s because there were errors in the attachment specification. Look above this message for details",
                create_info.name);
            return nullptr;
        }

        rx::ptr<rhi::RhiRenderpass> renderpass_result = device.create_renderpass(create_info, framebuffer_size, allocator);
        if(renderpass_result) {
            renderpass->renderpass = rx::utility::move(renderpass_result);

        } else {
            rg_log->error("Could not create renderpass %s", create_info.name);
            return nullptr;
        }

        // Backbuffer framebuffers are owned by the swapchain, not the renderpass that writes to them, so if the
        // renderpass writes to the backbuffer then we don't need to create a framebuffer for it
        if(!renderpass->writes_to_backbuffer) {
            renderpass->framebuffer = device.create_framebuffer(*renderpass->renderpass,
                                                                color_attachments,
                                                                depth_attachment.get(),
                                                                framebuffer_size,
                                                                allocator);
        }

        renderpass->pipeline_names = create_info.pipeline_names;
        renderpass->id = static_cast<uint32_t>(renderpass_metadatas.size());

        destroy_renderpass(create_info.name);

        renderpasses.insert(create_info.name, renderpass);
        renderpass_metadatas.insert(create_info.name, metadata);

        is_dirty = true;

        return renderpass;
    }
} // namespace nova::renderer
