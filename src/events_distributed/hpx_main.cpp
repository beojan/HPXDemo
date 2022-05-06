#include <chrono>
#include <deque>
#include <fstream>
#include <iostream>
#include <thread>

#include "HPXSched.h"
#include <fmt/chrono.h>
#include <fmt/ostream.h>
#include <fmt/format.h>
#include <hpx/hpx.hpp>
#include <hpx/wrap_main.hpp>

constexpr int n_evts_per_block = 3000;
using namespace std::chrono_literals;

template <class R, class P> void busy_wait(std::chrono::duration<R, P> time) {
    // Busy waits for a given length of time.
    auto start = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - start < time) {
    }
}

long long plus_fn(long long x, long long y) {
    // fmt::print("Running plus({}, {})\n", x, y);
    // std::this_thread::sleep_for(1s);
    return x + y;
}
HPX_PLAIN_ACTION(plus_fn, plus);

long long times_fn(long long x, long long y) {
    // fmt::print("Running times({}, {})\n", x, y);
    // std::this_thread::sleep_for(1s);
    return x * y;
}
HPX_PLAIN_ACTION(times_fn, times);

long long square_fn(long long x) {
    // fmt::print("Running square({})\n", x);
    busy_wait(100us);
    fmt::print("Running on locality {}\n", hpx::find_here().get_gid());
    return x * x;
}
HPX_PLAIN_ACTION(square_fn, square);

long long cube_fn(long long x) {
    // fmt::print("Running cube({})\n", x);
    // std::this_thread::sleep_for(100s);
    return x * x * x;
}
HPX_PLAIN_ACTION(cube_fn, cube);

sch::Sched scheduler{
      sch::Define("Cube Plus"_s, hana::make_tuple("Ten plus Five"_s), cube{}),
      sch::Define("Cube Times"_s, hana::make_tuple("Ten times Five"_s), cube{}),
      sch::Define("Ten plus Five"_s, hana::make_tuple("Five"_in, "Ten"_in), plus{}),
      sch::Define("Ten times Five"_s, hana::make_tuple("Five"_in, "Ten"_in), times{}),
      sch::Define("Square Plus"_s, hana::make_tuple("Ten plus Five"_s), square{}),
      sch::Define("Square Times"_s, hana::make_tuple("Ten times Five"_s), square{}),
      sch::Define("Add Squares"_s, hana::make_tuple("Square Plus"_s, "Square Times"_s), plus{})};
struct EvtCtx : public decltype(scheduler)::ECBase {
    long long Five = 5;
    long long Ten = 10;
};
BOOST_HANA_ADAPT_STRUCT(EvtCtx, Five, Ten);

int main(int argc, char* argv[]) {
    if (argc != 2) {
        fmt::print("Usage: {} input_file\n", argv[0]);
    }
    std::ifstream in{argv[1]};
    std::deque<EvtCtx> evts{};
    std::deque<hpx::shared_future<long long>> outputs{};

    long long n_evts = 0;
    std::chrono::duration<double, std::milli> total_time = 0ms;
    while (in.good()) {
        EvtCtx ec_template{};
        in >> ec_template.Five >> ec_template.Ten;
        if (!in.good()) {
            break;
        }
        auto start_tm = std::chrono::steady_clock::now();
        for (int i = 0; i < n_evts_per_block; ++i) {
            EvtCtx& ec = evts.emplace_back();
            ec = ec_template;
            auto& final_ans = scheduler.retrieve(ec, "Add Squares"_s);
            bool success = scheduler.schedule(ec);
            outputs.push_back(*final_ans);
            n_evts++;
        }
        auto this_time = std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(
              std::chrono::steady_clock::now() - start_tm);
        total_time += this_time;
        fmt::print("Took {} to schedule {} events\n", this_time, n_evts_per_block);
    }
    fmt::print("Waiting for all events\n");
    auto start_tm = std::chrono::steady_clock::now();
    hpx::wait_all(outputs.begin(), outputs.end());
    fmt::print("Took {} extra waiting for all events\n",
               std::chrono::duration_cast<std::chrono::duration<float, std::ratio<1, 1>>>(
                     std::chrono::steady_clock::now() - start_tm));
    fmt::print("Took {} total ({} average) scheduling events\n", total_time, total_time / n_evts);
    start_tm = std::chrono::steady_clock::now();
    volatile long long o = 0;
    for (auto&& out : outputs) {
        o = out.get();
    }
    fmt::print("Took {} reading out futures\n",
               std::chrono::duration_cast<std::chrono::duration<float, std::milli>>(
                     std::chrono::steady_clock::now() - start_tm));
    return 0;
}
