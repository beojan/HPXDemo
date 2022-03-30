// -*-c++-*-
#ifndef SCHED_H
#define SCHED_H
#include <functional>
#include <optional>

#include <fmt/format.h>

#include <boost/callable_traits.hpp>
namespace ct = boost::callable_traits;

// For compile-time string literals
#define BOOST_HANA_CONFIG_ENABLE_STRING_UDL
#include <boost/hana.hpp>
namespace hana = boost::hana;
using namespace hana::literals;

namespace sch {
template <class Key, class Inputs, class Func> auto Define(Key key, Inputs inputs, Func func) {
    BOOST_HANA_CONSTANT_ASSERT_MSG(hana::is_a<hana::string_tag>(key),
                                   "Define's key must be a hana::string");
    static_assert(hana::Sequence<Inputs>::value, "Define's inputs must be a tuple");
    using func_ret_t = ct::return_type_t<Func>;
    // Tuple items are: key, inputs, function to calculate, is_required, result
    return hana::make_pair(key, hana::make_tuple(key, inputs, std::reference_wrapper<Func>{func},
                                                 false, std::optional<func_ret_t>{}));
}

template <class... Defs> class Sched {
  private:
    hana::map<Defs...> definitions;

  public:
    Sched(Defs... defs) : definitions(hana::make_map(defs...)) {}
    template <typename Key> auto& retrieve(Key key) {
        hana::at_c<3>(definitions[key]) = true; // Record that we need to calculate this value
        return hana::at_c<4>(definitions[key]); // Return reference to result
    }

    // This function does the scheduling (and running)
    // For now, lets not use HPX
    bool schedule() {
        constexpr auto is_required = hana::reverse_partial(hana::at, hana::size_c<3>);
        // Given a key, run the computation for that value
        auto run = hana::fix([this](auto self, auto key) {
            if (!hana::contains(this->definitions, key)) {
                fmt::print("run -- Undefined key {}\n", key.c_str());
                std::exit(2);
            }
            auto& item = this->definitions[key];
            auto& res = hana::at_c<4>(item);
            if (res.has_value()) {
                // Already computed
                // Return result to be used as input in downstream calculations
                return *res;
            }
            // Run every input
            auto inputs = hana::at_c<1>(item);
            auto input_res = hana::transform(inputs, self);
            // Run this function
            auto func = hana::at_c<2>(item);
            res = hana::unpack(input_res, func);
            fmt::print("Calculated value of {} to be {}\n", key.c_str(), *res);
            // Return result to be used as input in downstream calculations
            return *res;
        });
        auto run_if_required = [this, &run, &is_required](auto key) {
            auto& item = this->definitions[key];
            if (!is_required(item)) {
                return;
            }
            fmt::print("\n{} is required -- running\n", key.c_str());
            run(key);
        };
        hana::for_each(hana::keys(definitions), run_if_required);
        return true;
    }
};

} // namespace sch
#endif /* SCHED_H */
