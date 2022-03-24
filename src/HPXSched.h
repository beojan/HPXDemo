// -*-c++-*-
#ifndef HPXSCHED_H
#define HPXSCHED_H
#include <functional>

#include <fmt/format.h>

#include <boost/callable_traits.hpp>
namespace ct = boost::callable_traits;

#include <boost/hof/lift.hpp>

// For compile-time string literals
#define BOOST_HANA_CONFIG_ENABLE_STRING_UDL
#include <boost/hana.hpp>
namespace hana = boost::hana;
using namespace hana::literals;

#include <hpx/async_base/async.hpp>
#include <hpx/async_base/dataflow.hpp>
#include <hpx/local/future.hpp>
#include <hpx/pack_traversal/unwrap.hpp>

namespace sch {
template <class Key, class Inputs, class Func> auto Define(Key key, Inputs inputs, Func func) {
    static_assert(hana::is_a<hana::string_tag>(key), "Define's key must be a hana::string");
    static_assert(hana::Sequence<Inputs>::value, "Define's inputs must be a tuple");
    using func_ret_t = ct::return_type_t<Func>;
    // Tuple items are: key, inputs, function to calculate, is_required, result
    return hana::make_pair(key,
                           hana::make_tuple(key, inputs, std::reference_wrapper<Func>{func}, false,
                                            (hpx::shared_future<func_ret_t>*)(nullptr)));
}

template <class... Defs> class Sched {
  private:
    hana::map<Defs...> definitions;

  public:
    Sched(Defs... defs) : definitions(hana::make_map(defs...)) {}
    template <typename Key> auto& retrieve(Key key) {
        hana::at_c<3>(definitions[key]) = true; // Record that we need to calculate this value
        return hana::at_c<4>(definitions[key]); // Return reference to pointer to future
    }

    // This function does the scheduling (and running)
    // For now, lets use HPX
    bool schedule() {
        constexpr auto dataflow = BOOST_HOF_LIFT(hpx::dataflow);
        constexpr auto is_required = hana::reverse_partial(hana::at, hana::size_c<3>);
        // Given a key, schedule the computation for that value
        auto run = hana::fix([this, &dataflow](auto self, auto key) {
            if (!hana::contains(this->definitions, key)) {
                fmt::print("run -- Undefined key {}\n", key.c_str());
                std::exit(2);
            }
            auto& item = this->definitions[key];
            auto& res = hana::at_c<4>(item);
            if (res) {
                // Already scheduled
                // Return future for use in downstream calculations
                return *res;
            }
            auto inputs = hana::at_c<1>(item);
            // Schedule this function
            auto func = hana::at_c<2>(item);
            if constexpr (hana::is_empty(inputs)) {
                res = new hpx::shared_future{hpx::async(func.get())};
            }
            else {
                // Schedule every input
                auto input_res = hana::transform(inputs, self);
                res = new hpx::shared_future{hana::unpack(
                      input_res, hana::partial(dataflow, hpx::unwrapping(func.get())))};
            }
            fmt::print("Scheduled calculation of {}\n", key.c_str());
            // Return future to be used as input in downstream calculations
            return *res;
        });
        auto run_if_required = [this, &run, &is_required](auto key) {
            auto& item = this->definitions[key];
            if (!is_required(item)) {
                return;
            }
            fmt::print("\n{} is required -- scheduling\n", key.c_str());
            run(key);
        };
        hana::for_each(hana::keys(definitions), run_if_required);
        return true;
    }
};

} // namespace sch
#endif /* HPXSCHED_H */
