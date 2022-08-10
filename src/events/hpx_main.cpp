#include <chrono>
#include <deque>
#include <fstream>
#include <iostream>
#include <thread>

#include "HPXSched.h"
#include <fmt/chrono.h>
#include <fmt/format.h>
#include <hpx/wrap_main.hpp>

#include <Eigen/Dense>
using Mtrx = Eigen::Matrix<double, 10, 10>;
constexpr int n_evts_per_block = 300000;
using namespace std::chrono_literals;

template <class R, class P> void busy_wait(std::chrono::duration<R, P> time) {
    // Busy waits for a given length of time.
    auto start = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - start < time) {
    }
}

Mtrx* make_mtrx(long long x) {
    Mtrx* mtrx = new Mtrx();
    mtrx->setRandom() *= x;
    return mtrx;
}

long long plus(Mtrx* x, Mtrx* y) {
    // fmt::print("Running plus({}, {})\n", x, y);
    // std::this_thread::sleep_for(1s);
    double ans = (*x + *y).norm();
    return ans;
}

long long scal_plus(long long x, long long y) {
    fmt::print("Running scal_plus({}, {})\n", x, y);
    // std::this_thread::sleep_for(1s);
    return x + y;
}

long long times(Mtrx* x, Mtrx* y) {
    // fmt::print("Running times({}, {})\n", x, y);
    // std::this_thread::sleep_for(1s);
    return (*x * *y).norm();
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
    long long X = 5;
    long long Y = 10;
};
BOOST_HANA_ADAPT_STRUCT(EvtCtx, X, Y);

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
