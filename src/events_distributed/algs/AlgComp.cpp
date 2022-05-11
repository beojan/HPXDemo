#include "AlgComp.h"
#include <chrono>

HPX_REGISTER_ACTION(AlgComp::plus, AlgComp_plus);
HPX_REGISTER_ACTION(AlgComp::times, AlgComp_times);
HPX_REGISTER_ACTION(AlgComp::square, AlgComp_square);
HPX_REGISTER_ACTION(AlgComp::cube, AlgComp_cube);

using namespace std::chrono_literals;

template <class R, class P>
void busy_wait(std::chrono::duration<R, P> time) {
    // Busy waits for a given length of time.
    auto start = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - start < time) {
    }
}

long long AlgComp::plus_fn(long long x, long long y) {
    // fmt::print("Running plus({}, {})\n", x, y);
    // std::this_thread::sleep_for(1s);
    return x + y;
}

long long AlgComp::times_fn(long long x, long long y) {
    // fmt::print("Running times({}, {})\n", x, y);
    // std::this_thread::sleep_for(1s);
    return x * y;
}

long long AlgComp::square_fn(long long x) {
    // fmt::print("Running square({})\n", x);
    busy_wait(100us);
    // fmt::print("Running on locality {}\n", hpx::find_here().get_gid());
    return x * x;
}

long long AlgComp::cube_fn(long long x) {
    // fmt::print("Running cube({})\n", x);
    // std::this_thread::sleep_for(100s);
    return x * x * x;
}