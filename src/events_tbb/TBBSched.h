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
#include <boost/hana/ext/std/tuple.hpp>
namespace hana = boost::hana;
using namespace hana::literals;

#include <tbb/tbb.h>
namespace flow = oneapi::tbb::flow;

namespace sch {
class input_tag {};
template <class HS> struct Input {
    using hana_tag = sch::input_tag;
    HS name;

    constexpr Input(HS name) : name(name) {}
};
template <class Key, class Inputs, class Func> auto Define(Key key, Inputs inputs, Func func) {
    static_assert(hana::is_a<hana::string_tag>(key), "Define's key must be a hana::string");
    static_assert(hana::Sequence<Inputs>::value, "Define's inputs must be a tuple");
    // Tuple items are: key, inputs tuple, function to calculate, bool (is final)
    return hana::make_pair(key, hana::make_tuple(key, inputs, std::function{func}, false));
}

template <class... Defs> class Sched {
  private:
    hana::map<Defs...> definitions;
    // Given a definition, get a default-constructed prototype of the function return type
    static constexpr auto get_ret_t = [](auto&& F) {
        return ct::return_type_t<decltype(hana::at_c<2>(F))>{};
    };
    // Given a definition, find out if this key has been retrieved
    static constexpr auto get_key = hana::reverse_partial(hana::at, hana::size_c<0>);
    static constexpr auto get_inputs = hana::reverse_partial(hana::at, hana::size_c<1>);
    static constexpr auto get_fn = hana::reverse_partial(hana::at, hana::size_c<2>);
    static constexpr auto is_final = hana::reverse_partial(hana::at, hana::size_c<3>);

    using Keys = decltype(hana::keys(definitions));
    using ResultTypes = decltype(hana::transform(hana::values(definitions), get_ret_t));

    // Given a key and an event context, get the node
    template <class EC, class Key> static flow::graph_node*& get_node(EC& ec, const Key& k) {
        return ec.node_slot[k];
    }

    // Make input nodes
    template <class EC> void make_input_nodes(EC& ec) {
        auto input_keys = hana::transform(hana::keys(ec), [](auto&& k) { return sch::Input(k); });
        auto input_vals = hana::members(ec);
        auto make_input_node = [&ec](auto&& k, auto&& v) {
            auto& node = *ec.nodes.emplace_back(
                  new flow::continue_node(ec.graph, [&v] { return v; }));
            get_node(ec, k) = &node;
        };
        hana::zip_with(make_input_node, input_keys, input_vals);
    }
    // Makes internal nodes
    template <class EC, class Val> void make_node(EC& ec, const Val& v) {
        using func_t = decltype(get_fn(v));
        using args_t = ct::args_t<func_t>;
        using ret_t = ct::return_type_t<func_t>;

        auto& input = *ec.nodes.emplace_back(new flow::join_node<args_t, flow::queuing>(ec.graph));
        auto& fn = *ec.nodes.emplace_back(
              new flow::function_node(ec.graph, 1, hana::fuse(get_fn(v))));
        flow::make_edge(input, fn);

        if (is_final(v)) {
            auto& write_fn = *ec.nodes.emplace_back(new flow::function_node(
                  ec.graph, 1, [&v, &ec](const ret_t& value) { ec.slot[get_key(v)] = value; }));
            flow::make_edge(fn, write_fn);
        }

        auto& composite = *ec.nodes.emplace_back(
              new flow::composite_node<args_t, std::tuple<ret_t>>(ec.graph));
        composite.set_external_ports(input.input_ports(), fn.output_ports());

        // add to definition
        get_node(ec, get_key(v)) = &composite;
    }

    // Makes connections between nodes
    template <class EC, class Val> void make_connections(EC& ec, const Val& v) {
        auto input_nodes = hana::transform(
              get_inputs(v),
              [&ec](auto&& in_key) -> flow::graph_node& { return *get_node(ec, in_key); });
        hana::zip_with(flow::make_edge, input_nodes, get_node(ec, get_key(v))->input_ports());
    }

    // static auto make_nodes() { return hana::to_map(hana::zip_with(hana::make_pair, Keys{}, )); }

  public:
    struct ECBase {
        decltype(hana::to_map(hana::zip_with(hana::make_pair, Keys{}, ResultTypes{}))) slot{};

        decltype(hana::to_map(hana::transform(
              [](auto&& key) {
                  return hana::make_pair(key, static_cast<flow::graph_node*>(nullptr));
              },
              Keys{}))) node_slot{};
        flow::graph graph{};
        std::vector<std::unique_ptr<flow::graph_node>> nodes{}; // Need to keep this list to destroy
                                                                // them at end
    };

    Sched(Defs... defs) : definitions(hana::make_map(defs...)) {}
    template <typename Key> auto& retrieve(ECBase& ec, Key key) {
        static_assert(!hana::is_a<sch::input_tag>(key), "Cannot 'retrieve' an input");
        is_final(definitions[key]) = true;
        return ec.slot[key]; // Return reference to pointer to future
    }

    // This function does the scheduling (and running)
    // This time, use TBB flow graphs
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
