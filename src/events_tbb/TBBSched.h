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
} // namespace sch
// has to be outside namespace
template <> struct hana::hash_impl<sch::input_tag, hana::when<true>> {
    template <class X> static constexpr auto apply(const X& x) { return hana::hash(x.name); }
};

namespace sch {
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

  public:
    using Keys = decltype(hana::keys(definitions));
    using ResultTypes = decltype(hana::transform(hana::values(definitions), get_ret_t));

  private:
    // Given a key and an event context, get the node
    template <bool Assign = false, class EC, class Key>
    static decltype(auto) get_node(EC& ec, const Key& k) {
        if constexpr (Assign) {
            if constexpr (hana::is_a<sch::input_tag, decltype(k)>()) {
                return ec.node_slot[k.name];
            }
            else {
                return ec.node_slot[k];
            }
        }
        else {
            // Need to figure out the correct type for this node
            if constexpr (hana::is_a<sch::input_tag, decltype(k)>()) {
                using in_t = std::remove_reference_t<decltype(hana::at_key(ec, k.name))>;
                using node_t = flow::continue_node<in_t>;
                return dynamic_cast<node_t*>(ec.node_slot[k.name]);
            }
            else {
                using func_t = decltype(get_fn(definitions[k]));
                using args_t = ct::args_t<func_t>;
                using ret_t = ct::return_type_t<func_t>;
                using node_t = flow::composite_node<args_t, std::tuple<ret_t>>;
                return dynamic_cast<node_t*>(ec.node_slot[k]);
            }
        }
    }

    // Make input nodes
    template <class EC> static void make_input_nodes(EC& ec) {
        auto input_keys = hana::transform(hana::keys(ec), [](auto&& k) { return sch::Input(k); });
        auto make_input_node = [&ec](auto&& k) {
            auto& v = hana::at_key(ec, k.name);
            auto& node = ec.add_node(new flow::continue_node<std::remove_reference_t<decltype(v)>>(
                  ec.graph, [&v](const flow::continue_msg&) { return v; }));
            make_edge(*ec.start_node, node);
            get_node<true>(ec, k) = &node;
        };
        hana::for_each(input_keys, make_input_node);
    }
    // Makes internal nodes
    template <class EC, class Val> static void make_node(EC& ec, const Val& v) {
        using func_t = decltype(get_fn(v));
        using args_t = ct::args_t<func_t>;
        using ret_t = ct::return_type_t<func_t>;

        auto& input = ec.add_node(new flow::join_node<args_t, flow::queueing>(ec.graph));
        auto& fn = ec.add_node(
              new flow::function_node<args_t, ret_t>(ec.graph, 1, hana::fuse(get_fn(v))));
        flow::make_edge(input, fn);

        if (is_final(v)) {
            auto& write_fn = ec.add_node(
                  new flow::function_node<ret_t, bool>(ec.graph, 1, [&v, &ec](const ret_t& value) {
                      ec.slot[get_key(v)] = value;
                      return true;
                  }));
            flow::make_edge(fn, write_fn);
        }

        auto& composite = ec.add_node(
              new flow::composite_node<args_t, std::tuple<ret_t>>(ec.graph));
        composite.set_external_ports(std::apply(BOOST_HOF_LIFT(std::tie), input.input_ports()), std::tie(fn));

        // add to definition
        get_node<true>(ec, get_key(v)) = &composite;
    }

    template <class HanaTuple, class TBBNode, std::size_t... I>
    static void make_edges_impl(HanaTuple&& sources, TBBNode&& target, std::index_sequence<I...>) {
        (flow::make_edge(*hana::at_c<I>(sources), flow::input_port<I>(target)), ...);
    }
    // Makes connections between nodes
    template <class EC, class Val> static void make_connections(EC& ec, const Val& v) {
        auto input_nodes = hana::transform(
              get_inputs(v), [&ec](auto&& in_key) { return get_node(ec, in_key); });
        make_edges_impl(input_nodes, *get_node(ec, get_key(v)),
                        std::make_index_sequence<hana::size(input_nodes)>());
    }

  public:
    struct ECBase {
        decltype(hana::to_map(hana::zip_with(hana::make_pair, Keys{}, ResultTypes{}))) slot{};
        static constexpr auto make_key_ptr_pair = [](auto&& key) {
            return hana::make_pair(key, static_cast<flow::graph_node*>(nullptr));
        };
        flow::graph graph{};
        bool done{false};
        std::unique_ptr<flow::continue_node<flow::continue_msg>> start_node{nullptr};
        std::vector<std::unique_ptr<flow::graph_node>> nodes{}; // Need to keep this list to destroy
                                                                // them at end
                                                                //
        ECBase& operator=(const ECBase&) {
            start_node.reset(new flow::continue_node<flow::continue_msg>(
                  graph, 1, [](const flow::continue_msg&) { return flow::continue_msg(); }));
            return *this;
        }

        // Helper to add a node
        template <class T> T& add_node(T* node_ptr) {
            nodes.emplace_back(node_ptr);
            return *node_ptr;
        }

        void wait() {
            // simple helper so we can wait for all events to complete
            if (done) return;
            graph.wait_for_all();
            done = true;
            nodes.clear();
            delete start_node.release();
        }
    };

    Sched(Defs... defs) : definitions(hana::make_map(defs...)) {}
    template <class EC, class Key> auto& retrieve(EC& ec, Key key) {
        static_assert(!hana::is_a<sch::input_tag>(key), "Cannot 'retrieve' an input");
        is_final(definitions[key]) = true;
        return ec.slot[key]; // Return reference to slot
    }

    // This function does the scheduling (and running)
    // This time, use TBB flow graphs
    template <class EC> bool schedule(EC& ec) {
        make_input_nodes(ec);
        auto this_make_node = [&ec](auto&& v) { return make_node(ec, v); };
        auto this_make_connections = [&ec](auto&& v) { return make_connections(ec, v); };
        hana::for_each(hana::values(definitions), this_make_node);
        hana::for_each(hana::values(definitions), this_make_connections);
        ec.start_node->try_put(flow::continue_msg());
        return true;
    }

    // // Helper to schedule cleanup
    // template <class EC> auto cleanup(EC& ec) {
    //     return [this, &ec](auto&& /* future */) {
    //         // Delete any intermediate values if they are pointers to free memory
    //         auto delete_intermediates = [&ec](auto&& item) {
    //             constexpr auto is_required = hana::reverse_partial(hana::at, hana::size_c<3>);
    //             if (!is_required(item)) {
    //                 auto* fut = hana::at_c<4>(item)(ec);
    //                 if (!fut || !fut->valid()) {
    //                     return;
    //                 }
    //                 auto& val = fut->get();
    //                 if constexpr (std::is_pointer_v<std::remove_reference_t<decltype(val)>>) {
    //                     if (val) {
    //                         delete val;
    //                     }
    //                 }
    //             }
    //         };
    //         hana::for_each(hana::values(definitions), delete_intermediates);
    //     };
    // }
};

} // namespace sch

template <class CharT, CharT... str> constexpr auto operator""_in() {
    return sch::Input{hana::string_c<str...>};
}

#endif /* HPXSCHED_H */
