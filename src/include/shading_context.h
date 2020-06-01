/*
    This file is a part of Tiny-Shading-Language or TSL, an open-source cross
    platform programming shading language.

    Copyright (c) 2020-2020 by Jiayin Cao - All rights reserved.

    TSL is a free software written for educational purpose. Anyone can distribute
    or modify it under the the terms of the GNU General Public License Version 3 as
    published by the Free Software Foundation. However, there is NO warranty that
    all components are functional in a perfect manner. Without even the implied
    warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
    General Public License for more details.

    You should have received a copy of the GNU General Public License along with
    this program. If not, see <http://www.gnu.org/licenses/gpl-3.0.html>.
 */

#pragma once

#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <vector>
#include <string>
#include "tslversion.h"
#include "closure.h"
#include "shader_arg_types.h"
#include "export.h"

TSL_NAMESPACE_BEGIN

class ShadingSystem;
class TslCompiler;
class ShaderUnitTemplate;
struct ShaderUnitTemplate_Pvt;
struct ShaderInstance_Pvt;
struct ShadingSystem_Impl;
struct ShaderUnitTemplate_Impl;
struct ShaderGroupTemplate_Impl;
struct ShadingContext_Impl;

//! @brief  ShaderInstance is the very basic unit of shader execution.
/**
 * A shader instance keeps track of the raw function pointer for shader execution.
 * Shader instances made from a same thread can't be resolved in multiple threads simultaneously.
 * But shader instance can be executed by multiple threads simultaneously once constructed and 
 * resolved.
 */
class TSL_INTERFACE ShaderInstance {
public:
    //! @brief  Private constructor to limit the construction of shader instance through ShaderUnitTemplate.
    //!
    //! @param  sut     Shader unit template that is used to construct this shader instance.
    ShaderInstance(const ShaderUnitTemplate& sut);

    //! @brief  Destructor
    ~ShaderInstance();

    //! @brief  Get the function pointer to execute the shader.
    //!
    //! @return     A function pointer points to code memory.
    uint64_t        get_function() const;

    //! @brief  Get private shader instance data.
    //!
    //! @return     It returns a pointer to shader instance private data.
    ShaderInstance_Pvt* get_shader_instance_data() {
        return m_shader_instance_data;
    }

    //! @brief  Get shader unit template.
    //!
    //! @return     The shader template.
    const ShaderUnitTemplate& get_shader_template() const {
        return m_shader_unit_template;
    }

private:
    /**< Shader unit template that creates this shader instance. */
    const ShaderUnitTemplate& m_shader_unit_template;

    /**< Private data inside shader instance. */
    ShaderInstance_Pvt* m_shader_instance_data = nullptr;

    // making sure ShaderUnitTemplate can access private method of this class so that it can create an instance
    // of it.
    friend class ShaderUnitTemplate;
};

//! @brief  ShaderUnitTemplate defines the shader of a single shader unit.
/**
 * A shader unit template defines the basic behavior of a shader unit.
 * Multiple shader units can be groupped into a shader group template.
 * A shader unit template can't be executed, it needs to instance a shader instance for shader execution.
 */
class TSL_INTERFACE ShaderUnitTemplate {
public:
    //! @brief  Constructor.
    //!
    //! @param  name            Name of the shader unit.
    ShaderUnitTemplate(const std::string& name);

    //! @brief  Destructor.
    virtual ~ShaderUnitTemplate();

    //! @brief          Get name of the shader unit.
    const std::string& get_name() const;

    //! @brief  Make a shader instance
    //!
    //! @return         Make a new shader instance.
    std::unique_ptr<ShaderInstance>     make_shader_instance();

    //! @brief  Parse shader dependencies.
    //!
    //! @param sut      Dependencies of this module.
    virtual void        parse_dependencies(ShaderUnitTemplate_Pvt* sut) const;

protected:
    ShaderUnitTemplate_Impl* m_shader_unit_template_impl = nullptr;

    // make sure shader instance can access private data of shader_unit_template
    friend class ShaderInstance;
    friend class TslCompiler_Impl;
};

//! @brief  Shader group is a basic unit of shader execution.
/**
 * A shader group is composed of multiple shader units with all of them connected with each other.
 * A shader group itself is also a shader unit, which is a quite useful feature to get recursive
 * node supported in certain material editors.
 */
class TSL_INTERFACE ShaderGroupTemplate : public ShaderUnitTemplate{
public:
    //! @brief  Constructor.
    //!
    //! @param  name            The name fo the shader group.
    ShaderGroupTemplate(const std::string& name);

    //! @brief  Destructor.
    ~ShaderGroupTemplate();

    //! @brief  Add a shader unit in the group.
    //!
    //! @param  name            Name of the shader unit added in the group.
    //! @param  shader_unit     A shader unit to be added in the group.
    //! @param  is_root         Whether the shader unit is the root of the group, there has to be exactly one root in each shader group.
    //! @return                 Whether the shader unit is added in the group.
    bool add_shader_unit(const std::string& name, ShaderUnitTemplate* shader_unit, const bool is_root = false);

    //! @brief  Connect shader unit in the shader group.
    //!
    //! @param  ssu     source shader unit
    //! @param  sspn    source shader parameter name
    //! @param  tsu     target shader unit
    //! @param  tspn    target shader parameter name
    void connect_shader_units(const std::string& ssu, const std::string& sspn, const std::string& tsu, const std::string& tspn);

    //! @brief  Setup shader group output
    //!
    //! @param  su      source shader unit
    //! @param  spn     source shader parameter name
    void expose_shader_argument(const std::string& su, const std::string& spn, const ArgDescriptor& arg_desc );

    //! @brief  Setup default shader argument init value
    //!
    //! @param  su      source shader unit
    //! @param  spn     source shader parameter name
    void init_shader_input(const std::string& su, const std::string& spn, const ShaderUnitInputDefaultValue& val);

    //! @brief  Parse shader group dependencies.
    //!
    //! @param sut      Dependencies of this module.
    void parse_dependencies(ShaderUnitTemplate_Pvt* sut) const override;

private:
    /**< Shader group template implementation. */
    ShaderGroupTemplate_Impl* m_shader_group_template_impl = nullptr;

    friend class TslCompiler_Impl;
};

//! @brief  Shading context should be a per-thread resource that is for shader related stuff.
/**
 * Unlike shading_system, shading_context is not designed to be thread-safe, meaning each thread
 * should have their own copy of a shading_system.
 * shading_context is used for shader related operations, like shader compilation, shader execution.
 * Since shading_context is available in each thread, things like shader compilation and shader 
 * execution could be exectued in multi-threaded too.
 */
class TSL_INTERFACE ShadingContext {
public:
    //! @brief  Destructor.
    ~ShadingContext();

    //! @brief  Make a new shader group.
    //!
    //! @param  name    Name of the shader group.
    //! @return         A pointer to the newly allocated shader group.
    ShaderGroupTemplate* begin_shader_group_template(const std::string& name);

    //! @brief  Resolve a shader group template before using it.
    //!
    //! @param sg       The shader group to be resolved.
    //! @return         Whether the shader is resolved successfully.
    bool                 end_shader_group_template(ShaderGroupTemplate* sg) const;

    //! @brief  Compile shader unit with source code.
    //!
    //! The function will return nullptr if for any reason the shader unit is failed to created,
    //! like invalid shader code or the name is already existed.
    //!
    //! @param name     Name of the shader unit
    //! @param source   Source code of the shader.
    //! @return         A pointer to shader unit.
    ShaderUnitTemplate*  compile_shader_unit_template(const std::string& name, const char* source) const;

    //! @brief  Resolve a shader instance before using it.
    //!
    //! @param si       The shader instance to be resolved.
    //! @return         Whether the shader is resolved successfully.
    bool                 resolve_shader_instance(ShaderInstance* si) const;

private:
    //! @brief  Constructor.
    //!
    //! shading_context can only be allocated through shading_system. Making the constructor private
    //! will make sure users of TSL won't create instance of this class manually.
    ShadingContext(ShadingSystem_Impl* shading_system_impl);

    /**< Shading context implementation. */
    ShadingContext_Impl* m_shading_context_impl = nullptr;

    friend class ShadingSystem;
};

TSL_NAMESPACE_END
