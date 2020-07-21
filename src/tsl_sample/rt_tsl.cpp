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

/*
 * This file demonstrate all it needs to integrate TSL in this ray tracer.
 */

#include <assert.h>
#include <memory>
#include "rt_tsl.h"

IMPLEMENT_TSLGLOBAL_BEGIN(TslGlobal)
IMPLEMENT_TSLGLOBAL_VAR(float3, base_color)
IMPLEMENT_TSLGLOBAL_VAR(float3, center)
IMPLEMENT_TSLGLOBAL_VAR(bool, flip_normal)
IMPLEMENT_TSLGLOBAL_END()

IMPLEMENT_CLOSURE_TYPE_BEGIN(ClosureTypeLambert)
IMPLEMENT_CLOSURE_TYPE_VAR(ClosureTypeLambert, float3, base_color)
IMPLEMENT_CLOSURE_TYPE_VAR(ClosureTypeLambert, float3, sphere_center)
IMPLEMENT_CLOSURE_TYPE_VAR(ClosureTypeLambert, bool, flip_normal)
IMPLEMENT_CLOSURE_TYPE_END(ClosureTypeLambert)

// In an ideal world, a sophisticated renderer should have its own memory management system.
// For example, it could pre-allocate a memory pool and claim memory dynamically during bxdf 
// allocation. This way it can avoid the performance overhead of page allocation under the hood.
// In order to stay as simple as possible, the following code does demonstrate a similar idea.
// However, the big limitation is its memory size, once memory runs out, it will crash.
// This is fine for this simple program since it has a hard limit on the depth of recursive rays
// to traverse, meaning there is also a limitation of how much memory it will allocate.

// This is just a random big number that avoids memory running out.
constexpr int BUF_MEM_SIZE = 16866;
// The current buffer offset, needs to be reset before at the beginning of evaluating every single pixel.
static thread_local int  buf_index = 0;
// The pre-allocated buffer.
static thread_local char buf[BUF_MEM_SIZE];

// This is the call back function for handling things like compiling errors and texture loading stuff.
class ShadingSystemInterfaceSimple : public Tsl_Namespace::ShadingSystemInterface {
public:
    // Simply fetch some memory from the memory pool
    void* allocate(unsigned int size) const override {
        assert(buf_index + size < BUF_MEM_SIZE);
        void* ret = buf + buf_index;
        buf_index += size;
        return ret;
    }

    // No error will be output since there are invalid unit tests.
    void catch_debug(const Tsl_Namespace::TSL_DEBUG_LEVEL level, const char* error) const override {
        printf("%s\n", error);
    }

    // Sample texture 2d
    void    sample_2d(const void* texture, float u, float v, Tsl_Namespace::float3& color) const override {
        // not implemented
    }

    void    sample_alpha_2d(const void* texture, float u, float v, float& alpha) const override {
        // not implemented
    }
};

// The raw function pointer of all surface shaders.
using shader_raw_func = void(*)(Tsl_Namespace::ClosureTreeNodeBase**, TslGlobal*);

// This is a very thin layer to wrap TSL related data structures, in a real complex ray tracing algorithm,
// there could be way more members in it. But in this tutorial program, this is good enough, it has everything
// it needs to express the properties of the material.
struct Material {
    // This is the shader unit template
    std::shared_ptr<ShaderUnitTemplate> m_shader_template;

    // This is the resolved shader instance, this is the unit of shader execution.
    std::shared_ptr<ShaderInstance>     m_shader_instance;

    // the resolved raw function pointer
    shader_raw_func                     m_shader_func;
};

// all materials available to use in this program
static Material g_materials[MaterialType::Cnt];

// Although it is possible to have different tsl global registered for different material types, this sample only uses one.
static TslGlobal g_tsl_global;

// The closure ids
static ClosureID g_closure_lambert = INVALID_CLOSURE_ID;

/*
 * The first material, lambert is very simple and straightforward. All of it is driven by one single shader unit template.
 * It is no the simplest form of TSL shader execution. Typically, renderers need to do things more complex than this material
 * since shaders are usually groupped by multiple shader unit templates.
 */
void initialize_lambert_material() {
    const auto shader_source = R"(
        shader lambert_shader(out closure bxdf){
            color  base_color = global_value<base_color>;
            vector center = global_value<center>;
            bool   flip_normal = global_value<flip_normal>;
            bxdf = make_closure<lambert>(base_color, center, flip_normal);
        }
    )";

    auto& shading_system = Tsl_Namespace::ShadingSystem::get_instance();
    auto shading_context = shading_system.make_shading_context();

    auto& mat = g_materials[(int)MaterialType::MT_Lambert];
    mat.m_shader_template = shading_context->begin_shader_unit_template("lambert");

    mat.m_shader_template->register_tsl_global(g_tsl_global.m_var_list);
    mat.m_shader_template->compile_shader_source(shader_source);
    shading_context->end_shader_unit_template(mat.m_shader_template.get());

    // make a shader instance
    mat.m_shader_instance = mat.m_shader_template->make_shader_instance();
    auto ret = mat.m_shader_instance->resolve_shader_instance();

    // get the raw function pointer
    mat.m_shader_func = (shader_raw_func)mat.m_shader_instance->get_function();
}

// Initialize all materials
void initialize_materials() {
    initialize_lambert_material();
}

// Reset the memory pool, this is a pretty cheap operation.
void reset_memory_allocator() {
    buf_index = 0;
}

/*
 * It does several things during TSL initialization.
 *   - Register the call back interface so that the ray tracer can handle some call back events like bxdf allocation.
 *   - Register all closure types used in this program. This needs to happen before shader compliation.
 *   - Create all materials by compiling its shader and cache the raw function pointer to be used later.
 */
void initialize_tsl_system() {
    // get the instance of tsl shading system
    auto& shading_system = Tsl_Namespace::ShadingSystem::get_instance();

    // register the call back functions
    std::unique_ptr<ShadingSystemInterfaceSimple> ssis = std::make_unique< ShadingSystemInterfaceSimple>();
    shading_system.register_shadingsystem_interface(std::move(ssis));

    // register closures
    g_closure_lambert = ClosureTypeLambert::RegisterClosure();

    // initialize all materials
    initialize_materials();
}

/*
 * Get the bxdf based on the sphere object. It basically gets the material based on the material type. With the material
 * located, it can easily access its resolved raw shader function with its compiled shader. It will then execute the shader
 * and parse the returned result from TSL shader to populate the data structure to be returned.
 */
std::unique_ptr<Bxdf> get_bxdf(const Sphere& obj) {
    // setup tsl global data structure
    TslGlobal tsl_global;
    tsl_global.base_color = make_float3(obj.c.x, obj.c.y, obj.c.z);
    tsl_global.center = make_float3( obj.p.x , obj.p.y, obj.p.z );
    tsl_global.flip_normal = obj.fn;

    // get the material
    const auto& mat = g_materials[obj.mt];
    if( !mat.m_shader_func )
        return std::make_unique<Lambert>(Vec(1.0, 0.0, 0.0), obj.p, obj.fn);

    // execute tsl shader
    Tsl_Namespace::ClosureTreeNodeBase* closure = nullptr;
    mat.m_shader_func(&closure, &tsl_global);

    // parse the result
    if (closure->m_id == g_closure_lambert) {
        const ClosureTypeLambert* lambert_param = (const ClosureTypeLambert*)closure->m_params;
        return std::make_unique<Lambert>(obj.c, lambert_param->sphere_center, lambert_param->flip_normal);
    }

    // unrecognized closure type, it shouldn't reach here at all
    return std::make_unique<Lambert>(Vec(1.0, 0.0, 0.0), obj.p, obj.fn);
}