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

#include "tslversion.h"
#include "closure.h"
#include "export.h"

TSL_NAMESPACE_BEGIN

class ShadingContext;
struct ShadingSystem_Impl;

//! @brief  Shading system is the root interface exposed through TSL system.
/*
 * A shading_system owns the whole TSL compiling system. A ray tracer with TSL integrated should have only one
 * instance of this class. Most of the interfaces, unless explicitly mentioned, should be thread-safe.
 * It owns all memory allocated of the system, it will also deallocate all memory allocated so there is no need
 * manually maintain memory allocated through this interface.
 */
class TSL_INTERFACE ShadingSystem {
public:
    //! @brief  Constructor.
    ShadingSystem();

    //! @brief  Destructor.
    ~ShadingSystem();

    //! @brief  Create a new shading context.
    //!
    //! Caller code doesn't need to release the memory allocated, it will be released automatically as the lifetime
    //! of this class comes to end.
    //!
    //! @return     Allocated memory points to an instance of a newly created shading_context.
    ShadingContext* make_shading_context();

    //! @brief  Register closure id.
    //!
    //! @param  name    Name of the closure.
    //! @return         Allocated closure id for the closure.
    ClosureID register_closure_type(const std::string& name, ClosureVarList& mapping, int closure_size);

private:
    /**< Internal data structure of shading system. */
    ShadingSystem_Impl* m_shading_system_impl = nullptr;
};

TSL_NAMESPACE_END