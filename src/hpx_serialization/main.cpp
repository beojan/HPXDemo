
#include <hpx/config.hpp>
#include <hpx/future.hpp>
#include <hpx/hpx_main.hpp>
#include <hpx/iostream.hpp>
#include <hpx/runtime.hpp>

#include "dataclass.h"
#include <utility>
#include <vector>

Event print_event(Event evt) {
    std::size_t const os_threads = hpx::get_os_thread_count();
    hpx::cout << "Action executed on locality " << hpx::get_locality_name() << "("
              << hpx::get_locality_id() << ") with " << os_threads << " threads. Event: " << evt
              << std::endl;
    return evt;
}

HPX_PLAIN_ACTION(print_event, print_event_action);

int main() {
    std::vector<hpx::id_type> localities = hpx::find_all_localities();

    std::vector<hpx::future<Event>> futures;
    futures.reserve(localities.size());

    std::string name("evtid");
    for (auto& node : localities) {
        Event evt(name, 15);
        futures.push_back(hpx::dataflow<print_event_action>(node, evt));
    }
    hpx::wait_all(futures.begin(), futures.end());

    for (auto&& f : futures) {
        auto result = f.get();
        hpx::cout << "Got result from future: " << result << std::endl;
    }
    return 0;
}