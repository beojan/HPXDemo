#include <chrono>
#include <deque>
#include <fstream>
#include <iostream>
#include <thread>

#include "HPXSched.h"
#include <fmt/chrono.h>
#include <fmt/format.h>
#include <hpx/wrap_main.hpp>

// using Mtrx = Eigen::Matrix<double, 10, 10>;

#include "CUDAMtrx.h"
using Mtrx = CUDAMtrx<100>;
constexpr int n_evts_per_block = 30;
using namespace std::chrono_literals;

// setup later
cublasHandle_t cublas_hndl;
curandGenerator_t curand_gen;

template <class R, class P> void busy_wait(std::chrono::duration<R, P> time) {
    // Busy waits for a given length of time.
    auto start = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - start < time) {
    }
}

Mtrx* make_mtrx(long long x) {
    Mtrx* mtrx = new Mtrx(x);
    return mtrx;
}

long long plus(Mtrx* x, Mtrx* y) {
    float ans = (*x + *y).norm();
    return ans;
}

long long scal_plus(long long x, long long y) {
    return x + y;
}

long long times(Mtrx* x, Mtrx* y) {
    float ans = (*x * *y).norm();
    return ans;
}

long long square(long long x) {
    return x * x;
}

long long cube(long long x) {
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

    // setup CUDA
    setup();
    std::ifstream in{argv[1]};
    std::deque<EvtCtx> evts{};
    std::deque<hpx::shared_future<long long>> outputs{};
    std::deque<hpx::future<void>> cleanups{};

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
            cleanups.emplace_back(final_ans->then(scheduler.cleanup(ec)));
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
    auto extra_tm = std::chrono::duration_cast<std::chrono::duration<float, std::milli>>(
          std::chrono::steady_clock::now() - start_tm);
    fmt::print("Took {} ({} average) extra waiting for all events\n", extra_tm, extra_tm / n_evts);
    fmt::print("Took {} total ({} average) scheduling events\n", total_time, total_time / n_evts);
    start_tm = std::chrono::steady_clock::now();
    volatile long long o = 0;
    for (auto&& out : outputs) {
        o = out.get();
    }
    fmt::print("Took {} reading out futures\n",
               std::chrono::duration_cast<std::chrono::duration<float, std::milli>>(
                     std::chrono::steady_clock::now() - start_tm));
    // wait for all cleanup to be done
    hpx::wait_all(cleanups.begin(), cleanups.end());

    // cleanup CUDA
    // teardown();
    return 0;
}
