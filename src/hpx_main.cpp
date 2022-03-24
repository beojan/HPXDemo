#include <chrono>
#include <thread>

#include "HPXSched.h"
#include <fmt/format.h>
#include <hpx/wrap_main.hpp>

using namespace std::chrono_literals;

int main(int argc, char* argv[]) {
    constexpr auto plus = [](int x, int y) {
        std::this_thread::sleep_for(1s);
        return x + y;
    };
    constexpr auto times = [](int x, int y) {
        std::this_thread::sleep_for(1s);
        return x * y;
    };
    constexpr auto square = [](int x) {
        std::this_thread::sleep_for(1s);
        return x * x;
    };
    constexpr auto cube = [](int x) {
        std::this_thread::sleep_for(100s);
        return x * x * x;
    };
    sch::Sched scheduler{
          sch::Define("Cube Plus"_s, hana::make_tuple("Ten plus Five"_s), cube),
          sch::Define("Cube Times"_s, hana::make_tuple("Ten times Five"_s), cube),
          sch::Define("Five"_s, hana::make_tuple(), []() { return 5; }),
          sch::Define("Ten"_s, hana::make_tuple(), []() { return 10; }),
          sch::Define("Ten plus Five"_s, hana::make_tuple("Five"_s, "Ten"_s), plus),
          sch::Define("Ten times Five"_s, hana::make_tuple("Five"_s, "Ten"_s), times),
          sch::Define("Square Plus"_s, hana::make_tuple("Ten plus Five"_s), square),
          sch::Define("Square Times"_s, hana::make_tuple("Ten times Five"_s), square),
          sch::Define("Add Squares"_s, hana::make_tuple("Square Plus"_s, "Square Times"_s), plus)};
    auto& final_ans = scheduler.retrieve("Add Squares"_s);
    auto& final_ans2 = scheduler.retrieve("Square Plus"_s);
    auto& final_ans3 = scheduler.retrieve("Cube Times"_s);
    bool success = scheduler.schedule();
    if (!success) {
        fmt::print("Failed to schedule!\n");
        return 1;
    }
    fmt::print("Waiting on final_ans\n");
    final_ans->wait();
    fmt::print("Waiting on final_ans2\n");
    final_ans2->wait();
    fmt::print("Waiting on final_ans2\n");
    final_ans3->wait();
    fmt::print("Final answers are {}, {}, {}\n", final_ans->get(), final_ans2->get(),
               final_ans3->get());
    return 0;
}
