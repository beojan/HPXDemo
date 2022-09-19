#include <chrono>
#include <deque>
#include <fstream>
#include <iostream>
#include <thread>

#include "TBBSched.h"
#include <fmt/chrono.h>
#include <fmt/format.h>

#include "CPUMtrx.h"
using Mtrx = CPUMtrx<1000>;
constexpr int n_evts_per_block = 3000;
constexpr int n_evts_in_flight = 32;
using namespace std::chrono_literals;

template <class R, class P> void busy_wait(std::chrono::duration<R, P> time) {
    // Busy waits for a given length of time.
    auto start = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - start < time) {
    }
}

std::shared_ptr<Mtrx> make_mtrx(long long x) {
    auto mtrx = std::make_shared<Mtrx>(x);
    return mtrx;
}

long long plus(std::shared_ptr<Mtrx> x, std::shared_ptr<Mtrx> y) {
    float ans = (*x + *y).norm();
    return ans;
}

long long scal_plus(long long x, long long y) {
    // fmt::print("Final ans: {}\n", x + y);
    return x + y;
}

long long times(std::shared_ptr<Mtrx> x, std::shared_ptr<Mtrx> y) {
    float ans = (*x * *y).norm();
    return ans;
}

long long square(long long x) {
    // fmt::print("Running square({})\n", x);
    return x * x;
}

long long cube(long long x) {
    // fmt::print("Running cube({})\n", x);
    // std::this_thread::sleep_for(100s);
    return x * x * x;
}

sch::Sched scheduler{
      sch::Define("Matrix X"_s, hana::make_tuple("X"_in), make_mtrx),
      sch::Define("Matrix Y"_s, hana::make_tuple("Y"_in), make_mtrx),
      sch::Define("Cube Plus"_s, hana::make_tuple("Y plus X"_s), cube),
      sch::Define("Cube Times"_s, hana::make_tuple("Y times X"_s), cube),
      sch::Define("Y plus X"_s, hana::make_tuple("Matrix X"_s, "Matrix Y"_s), plus),
      sch::Define("Y times X"_s, hana::make_tuple("Matrix X"_s, "Matrix Y"_s), times),
      sch::Define("Square Plus"_s, hana::make_tuple("Y plus X"_s), square),
      sch::Define("Square Times"_s, hana::make_tuple("Y times X"_s), square),
      sch::Define("Add Squares"_s, hana::make_tuple("Square Plus"_s, "Square Times"_s), scal_plus)};
struct EvtCtx : public decltype(scheduler)::ECBase {
    BOOST_HANA_DEFINE_STRUCT(EvtCtx, (long long, X), (long long, Y));
    decltype(hana::to_map(
          hana::transform(hana::insert_range(decltype(scheduler)::Keys{}, hana::size_c<0>,
                                             hana::make_tuple("X"_s, "Y"_s)),
                          make_key_ptr_pair))) node_slot{};
};

int main(int argc, char* argv[]) {
    auto limit_n_threads = tbb::global_control(tbb::global_control::max_allowed_parallelism,
                                               std::atoi(argv[2]));
    if (argc != 3) {
        fmt::print("Usage: {} input_file num_threads\n", argv[0]);
        return 1;
    }
    std::ifstream in{argv[1]};
    std::deque<EvtCtx> evts{};
    std::deque<EvtCtx*> cleanup_temp{};
    std::deque<std::reference_wrapper<long long>> outputs{};

    long long n_evts = 0;
    std::chrono::duration<float, std::milli> total_time = 0ms;
    while (in.good()) {
        EvtCtx ec_template{};
        in >> ec_template.X >> ec_template.Y;
        if (!in.good()) {
            break;
        }
        auto start_tm = std::chrono::steady_clock::now();
        for (int i = 0; i < n_evts_per_block; ++i) {
            EvtCtx& ec = evts.emplace_back();
            ec = ec_template;
            auto& final_ans = scheduler.retrieve(ec, "Add Squares"_s);
            bool success = scheduler.schedule(ec);
            outputs.emplace_back(final_ans);
            cleanup_temp.push_back(&ec);
            n_evts++;
            if (n_evts % n_evts_in_flight == n_evts_in_flight - 1) {
                // auto start_wait = std::chrono::steady_clock::now();
                tbb::parallel_for_each(cleanup_temp, [](EvtCtx* const& ec) { ec->wait(); });
                // auto end_wait = std::chrono::steady_clock::now() - start_wait;
                // fmt::print("This wait took {}\n",
                //            std::chrono::duration_cast<std::chrono::duration<float, std::milli>>(
                //                  end_wait));
                cleanup_temp.clear();
            }
        }
        auto this_time = std::chrono::duration_cast<std::chrono::duration<float, std::milli>>(
              std::chrono::steady_clock::now() - start_tm);
        total_time += this_time;
        fmt::print("Took {} to schedule {} events\n", this_time, n_evts_per_block);
    }
    fmt::print("Waiting for all events\n");
    auto start_tm = std::chrono::steady_clock::now();
    // hpx::wait_all(outputs.begin(), outputs.end());
    tbb::parallel_for_each(evts, [](const EvtCtx& ec) { const_cast<EvtCtx&>(ec).wait(); });
    auto extra_tm = std::chrono::duration_cast<std::chrono::duration<float, std::milli>>(
          std::chrono::steady_clock::now() - start_tm);
    fmt::print("Took {} ({} average) extra waiting for all events\n", extra_tm, extra_tm / n_evts);
    fmt::print("Took {} total ({} average) scheduling events\n", total_time, total_time / n_evts);
    start_tm = std::chrono::steady_clock::now();
    volatile long long o = 0;
    for (auto&& out : outputs) {
        o = out;
    }
    fmt::print("Took {} reading out data\n",
               std::chrono::duration_cast<std::chrono::duration<float, std::milli>>(
                     std::chrono::steady_clock::now() - start_tm));
    // wait for all cleanup to be done
    // hpx::wait_all(cleanups.begin(), cleanups.end());
    return 0;
}
