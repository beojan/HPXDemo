#include <chrono>
#include <thread>

#include "HPXSched.h"
#include <fmt/format.h>
#include <hpx/wrap_main.hpp>

using namespace std::chrono_literals;

int plus(int x, int y) {
    fmt::print("Running plus({}, {})\n", x, y);
    std::this_thread::sleep_for(1s);
    return x + y;
}

int times(int x, int y) {
    fmt::print("Running times({}, {})\n", x, y);
    std::this_thread::sleep_for(1s);
    return x * y;
}

int square(int x) {
    fmt::print("Running square({})\n", x);
    std::this_thread::sleep_for(1s);
    return x * x;
}

int cube(int x) {
    fmt::print("Running cube({})\n", x);
    std::this_thread::sleep_for(100s);
    return x * x * x;
}

sch::Sched scheduler{
      sch::Define("Cube Plus"_s, hana::make_tuple("Ten plus Five"_s), cube),
      sch::Define("Cube Times"_s, hana::make_tuple("Ten times Five"_s), cube),
      sch::Define("Ten plus Five"_s, hana::make_tuple("Five"_in, "Ten"_in), plus),
      sch::Define("Ten times Five"_s, hana::make_tuple("Five"_in, "Ten"_in), times),
      sch::Define("Square Plus"_s, hana::make_tuple("Ten plus Five"_s), square),
      sch::Define("Square Times"_s, hana::make_tuple("Ten times Five"_s), square),
      sch::Define("Add Squares"_s, hana::make_tuple("Square Plus"_s, "Square Times"_s), plus)};
struct EvtCtx : public decltype(scheduler)::ECBase {
    int Five = 5;
    int Ten = 10;
};
BOOST_HANA_ADAPT_STRUCT(EvtCtx, Five, Ten);

int main(int argc, char* argv[]) {
    EvtCtx ec{};
    ec.Five = 6;
    ec.Ten = 11;
    auto& final_ans = scheduler.retrieve(ec, "Add Squares"_s);
    bool success = scheduler.schedule(ec);
    fmt::print("Final answers are {}\n", final_ans->get());
    return 0;
}
