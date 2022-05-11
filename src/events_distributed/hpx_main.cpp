#include <chrono>
#include <deque>
#include <fstream>
#include <iostream>
#include <thread>
#include <utility>

#include "AlgComp.h"
#include "HPXSched.h"
#include <fmt/chrono.h>
#include <fmt/format.h>
#include <fmt/ostream.h>
#include <hpx/hpx.hpp>
#include <hpx/modules/distribution_policies.hpp>
#include <hpx/wrap_main.hpp>

constexpr int n_evts_per_block = 3000;
using namespace std::chrono_literals;

sch::Sched scheduler{
      sch::Define("Cube Plus"_s, hana::make_tuple("Ten plus Five"_s), AlgComp::cube{}),
      sch::Define("Cube Times"_s, hana::make_tuple("Ten times Five"_s), AlgComp::cube{}),
      sch::Define("Ten plus Five"_s, hana::make_tuple("Five"_in, "Ten"_in), AlgComp::plus{}),
      sch::Define("Ten times Five"_s, hana::make_tuple("Five"_in, "Ten"_in), AlgComp::times{}),
      sch::Define("Square Plus"_s, hana::make_tuple("Ten plus Five"_s), AlgComp::square{}),
      sch::Define("Square Times"_s, hana::make_tuple("Ten times Five"_s), AlgComp::square{}),
      sch::Define("Add Squares"_s, hana::make_tuple("Square Plus"_s, "Square Times"_s),
                  AlgComp::plus{})};
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
    auto n_localities = hpx::get_num_localities();
    std::vector<hpx::id_type> components = hpx::new_<AlgComp[]>(hpx::binpacked(hpx::find_all_localities()), n_localities.get()).get();
    fmt::print("We have {} components\n", components.size());

    long long n_evts = 0;
    std::chrono::duration<double, std::milli> total_time = 0ms;
    int counter = 0;
    while (in.good()) {
        EvtCtx ec_template{};
        in >> ec_template.Five >> ec_template.Ten;
        if (!in.good()) {
            break;
        }
        auto start_tm = std::chrono::steady_clock::now();
        auto loc = components[counter++ % components.size()];
        fmt::print("Scheduling on {}\n", loc);
        for (int i = 0; i < n_evts_per_block; ++i) {
            EvtCtx& ec = evts.emplace_back();
            ec = ec_template;
            auto& final_ans = scheduler.retrieve(ec, "Add Squares"_s);
            bool success = scheduler.schedule(ec, loc);
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
