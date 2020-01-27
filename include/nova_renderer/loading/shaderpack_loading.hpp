#pragma once

#include "nova_renderer/filesystem/folder_accessor.hpp"
#include "nova_renderer/rhi/rhi_enums.hpp"
#include "nova_renderer/shaderpack_data.hpp"

namespace nova::renderer::shaderpack {
    /*!
     * \brief Loads all the data for a single shaderpack
     *
     * This function reads the shaderpack data from disk (either a folder od a zip file) and performs basic validation
     * to ensure both that the data is well-formatted JSON, but also to ensure that the data has all the fields that
     * Nova requires, e.g. a material must have at least one pass, a texture must have a width and a height, etc. All
     * generated warnings and errors are printed to the Nova logger
     *
     * If the shaderpack can't be loaded, an empty optional is returned
     *
     * Note: This function is NOT thread-safe. It should only be called for a single thread at a time
     *
     * \param shaderpack_name The name of the shaderpack to loads
     * \return The shaderpack, if it can be loaded, or an empty optional if it cannot
     */
    RenderpackData load_shaderpack_data(const rx::string& shaderpack_name);

    rx::vector<uint32_t> load_shader_file(const rx::string& filename,
                                          const filesystem::FolderAccessorBase* folder_access,
                                          rhi::ShaderStage stage,
                                          const rx::vector<rx::string>& defines = {});
} // namespace nova::renderer::shaderpack
