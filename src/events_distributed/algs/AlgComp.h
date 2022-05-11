#pragma once

#include <hpx/include/components.hpp>

struct AlgComp : hpx::components::component_base<AlgComp> {

    long long plus_fn(long long x, long long y);
    HPX_DEFINE_COMPONENT_ACTION(AlgComp, plus_fn, plus);

    long long times_fn(long long x, long long y);
    HPX_DEFINE_COMPONENT_ACTION(AlgComp, times_fn, times);

    long long square_fn(long long x);
    HPX_DEFINE_COMPONENT_ACTION(AlgComp, square_fn, square);

    long long cube_fn(long long x);
    HPX_DEFINE_COMPONENT_ACTION(AlgComp, cube_fn, cube);
};

HPX_REGISTER_ACTION_DECLARATION(AlgComp::plus, AlgComp_plus);
HPX_REGISTER_ACTION_DECLARATION(AlgComp::times, AlgComp_times);
HPX_REGISTER_ACTION_DECLARATION(AlgComp::square, AlgComp_square);
HPX_REGISTER_ACTION_DECLARATION(AlgComp::cube, AlgComp_cube);

// docs mention that it should be in the source file however we get a compilation error if not visible in the hpx_main translation unit when using hpx::new_<AlgComp[]>
HPX_REGISTER_COMPONENT(hpx::components::component<AlgComp>, AlgComp);
