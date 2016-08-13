/*******************************************************************************
* Copyright 2016 Intel Corporation
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*******************************************************************************/

#ifndef PRIMITIVE_HPP
#define PRIMITIVE_HPP

#include "mkldnn.h"

#include "c_types_map.hpp"
#include "nstl.hpp"

// TODO: consider using smart pointers for storing primitives. External handles
// then would have to be cast to smart pointers. This would ensure that
// accedentally deleting a primitive that is a dependency for another one does
// not cause a segfault.

struct mkldnn_primitive: public mkldnn::impl::c_compatible {
public:
    enum exec_state { done, busy, not_ready, error };

private:
    // TODO: copy, equality and assignment -- all must be banned...

protected:
    const mkldnn::impl::primitive_desc_t _primitive_desc;
    mkldnn::impl::engine *_engine;
    exec_state _exec_state;
    mkldnn::impl::nstl::vector<mkldnn::impl::primitive_at_t> _input;
    mkldnn::impl::nstl::vector<const mkldnn::impl::primitive*> _output;

    virtual mkldnn::impl::status_t execute_impl() = 0;

    mkldnn_primitive(const mkldnn::impl::primitive_desc_t& primitive_desc,
            mkldnn::impl::engine *engine, exec_state state = not_ready)
        : _primitive_desc(primitive_desc)
        , _engine(engine)
        , _exec_state(state) {}

public:
    virtual ~mkldnn_primitive() {}

    mkldnn::impl::primitive_desc_t primitive_desc() const
    { return _primitive_desc; }
    mkldnn::impl::engine *engine() const { return _engine; }
    mkldnn::impl::primitive_kind_t kind() const
    { return _primitive_desc.base.primitive_kind; }

    virtual bool own_memory() const { return false; }

    virtual exec_state get_exec_state() const { return _exec_state; }
    virtual mkldnn::impl::status_t reset_exec_state(exec_state state) {
        // TODO: some checks here?
        _exec_state = state;
        return mkldnn::impl::status::success;
    }

    bool inputs_ready() const {
        for (auto i = 0UL; i < _input.size(); i++)
            if (_input[i].primitive->get_exec_state() != done)
                return false;
        return true;
    }
    mkldnn::impl::status_t execute() {
        if (!inputs_ready())
            return mkldnn::impl::status::not_ready;
        return execute_impl();
    }

    size_t input_count() const { return _input.size(); }
    const decltype(_input) &input() const { return _input; }

    size_t output_count() const { return _output.size(); }
    const decltype(_output) &output() const { return _output; }

    virtual char* memory(size_t index = 0) const
    { return output()[index]->memory(); }
    virtual const char* memory_const(size_t index = 0) const
    { return output()[index]->memory_const(); }
};

namespace mkldnn { namespace impl {

typedef status_t (*primitive_desc_init_f)(primitive_desc_t *primitive_desc,
        const op_desc_t &op_desc, const engine &aengine);
typedef status_t (*primitive_create_f)(primitive **aprimitive,
        const primitive_desc_t *primitive_desc, const primitive_at_t inputs[],
        const primitive *outputs[]);

struct primitive_impl /* : public c_compatible */ {
    const primitive_create_f primitive_create;
};

status_t primitive_desc_init(primitive_desc_t *primitive_desc,
        const op_desc_t &op_desc, const engine &aengine);

status_t inline check_inputs_array(size_t n, const primitive_at_t inputs[]) {
    for (size_t i = 0; i < n; i++)
        if (inputs[i].primitive->output_count() <= inputs[i].output_index)
            return status::invalid_arguments;
    return status::success;
}

}}

#endif

// vim: et ts=4 sw=4 cindent cino^=l0,\:0,N-s
