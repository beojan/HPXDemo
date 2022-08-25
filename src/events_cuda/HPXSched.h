// -*-c++-*-
#ifndef HPXSCHED_H
#define HPXSCHED_H
#include <functional>
#include <type_traits>

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

#include <cuda.h>

namespace sch {
class input_tag {};
template <class HS> struct Input {
    using hana_tag = sch::input_tag;
    HS name;

    constexpr Input(HS name) : name(name) {}
};

template <class Key, class EC> auto& get_slot_fn(EC& ec) { return ec.slot[Key{}]; }
template <class Key> BOOST_HOF_LIFT_CLASS(get_slot, get_slot_fn<Key>);
template <class Key, class Inputs, class Func> auto Define(Key key, Inputs inputs, Func func) {
    static_assert(hana::is_a<hana::string_tag>(key), "Define's key must be a hana::string");
    static_assert(hana::Sequence<Inputs>::value, "Define's inputs must be a tuple");
    using func_ret_t = ct::return_type_t<Func>;
    using fut_p = hpx::shared_future<func_ret_t>*;
    // Tuple items are: key, inputs tuple, function to calculate, required, func returning
    // ref-to-ptr-to-future, prototype ptr-to-future
    return hana::make_pair(key,
                           hana::make_tuple(key, inputs, func, false, get_slot<Key>{}, fut_p{}));
}

template <class... Defs> class Sched {
  private:
    hana::map<Defs...> definitions;
    using Keys = decltype(hana::keys(definitions));
    using FutTypes = decltype(hana::transform(hana::values(definitions),
                                              hana::reverse_partial(hana::at, hana::size_c<5>)));

  public:
    struct ECBase {
        decltype(hana::to_map(hana::zip_with(hana::make_pair, Keys{}, FutTypes{})))
              slot = hana::to_map(hana::zip_with(hana::make_pair, Keys{}, FutTypes{}));
    };

    Sched(Defs... defs) : definitions(hana::make_map(defs...)) {}
    template <typename Key, typename EC> auto& retrieve(EC& ec, Key key) {
        static_assert(!hana::is_a<sch::input_tag>(key), "Cannot 'retrieve' an input");
        hana::at_c<3>(definitions[key]) = true;     // Record that we need to calculate this value
        return hana::at_c<4>(definitions[key])(ec); // Return reference to pointer to future
    }

    // This function does the scheduling (and running)
    // For now, lets use HPX
    template <typename EC> bool schedule(EC& ec) {
        constexpr auto dataflow = BOOST_HOF_LIFT(hpx::dataflow);
        constexpr auto is_required = hana::reverse_partial(hana::at, hana::size_c<3>);
        // Given a key, schedule the computation for that value
        auto run = hana::fix([this, &dataflow, &ec](auto self, auto key) {
            if constexpr (hana::is_a<sch::input_tag>(key)) {
                hpx::shared_future fut = hpx::make_ready_future(hana::at_key(ec, key.name));
                return fut;
            }
            else {
                auto& item = this->definitions[key];
                auto& res = hana::at_c<4>(item)(ec);
                if (res) {
                    // Already scheduled
                    // Return future for use in downstream calculations
                    return *res;
                }
                auto inputs = hana::at_c<1>(item);
                // Schedule this function
                auto func = hana::at_c<2>(item);
                if constexpr (hana::is_empty(inputs)) {
                    // fmt::print("Scheduling {} with no inputs\n", key.c_str());
                    res = new hpx::shared_future{hpx::async(func)};
                }
                else {
                    // Schedule every input
                    auto input_res = hana::transform(inputs, self);
                    // fmt::print("Scheduling calculation of {} with inputs\n", key.c_str());
                    res = new hpx::shared_future{
                          hana::unpack(input_res, hana::partial(dataflow, hpx::unwrapping(func)))};
                }
                // Return future to be used as input in downstream calculations
                return *res;
            }
        });
        auto run_if_required = [this, &run, &is_required](auto key) {
            auto& item = this->definitions[key];
            if (!is_required(item)) {
                return;
            }
            // fmt::print("\n{} is required -- scheduling\n", key.c_str());
            run(key);
        };
        hana::for_each(hana::keys(definitions), run_if_required);
        return true;
    }

    // Helper to schedule cleanup
    template <typename EC> auto cleanup(EC& ec) {
        return [this, &ec](auto&& /* future */) {
            size_t free_byte;
            size_t total_byte;
            auto cuda_status = cudaMemGetInfo(&free_byte, &total_byte);
            if (cuda_status == cudaSuccess) {
                fmt::print("{} GB free of {} GB total\n", free_byte / 1e9, total_byte / 1e9);
            }
            else {
                // fmt::print("CUDA Failure: {} -- {}\n", cudaGetErrorName(cuda_status), cudaGetErrorString(cuda_status));
            }
            // Delete any intermediate values if they are pointers to free memory
            auto delete_intermediates = [&ec](auto&& item) {
                constexpr auto is_required = hana::reverse_partial(hana::at, hana::size_c<3>);
                if (!is_required(item)) {
                    auto* fut = hana::at_c<4>(item)(ec);
                    if (!fut || !fut->valid()) {
                        return;
                    }
                    auto& val = fut->get();
                    if constexpr (std::is_pointer_v<std::remove_reference_t<decltype(val)>>) {
                        if (val) {
                            delete val;
                        }
                    }
                    delete fut;
                }
            };
            hana::for_each(hana::values(definitions), delete_intermediates);
        };
    }
};

} // namespace sch

template <typename CharT, CharT... str> constexpr auto operator""_in() {
    return sch::Input{hana::string_c<str...>};
}

#endif /* HPXSCHED_H */
