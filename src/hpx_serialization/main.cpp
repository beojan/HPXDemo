
#include <hpx/config.hpp>
#include <hpx/hpx_main.hpp>
#include <hpx/iostream.hpp>
#include <hpx/local/future.hpp>

#include <vector>
#include "dataclass.h"

Event hello_world(Event evt, Simple s)
{
    std::size_t const os_threads = hpx::get_os_thread_count();
    hpx::cout << "Action executed on locality " << hpx::get_locality_name() << "(" << hpx::get_locality_id() << ") with " << os_threads << " threads. Event state: " << evt.print_state() << ", Elem: " << s.getElem() << hpx::endl;
    return evt;
}

HPX_PLAIN_ACTION(hello_world, hello_world_action);

int main()
{
    std::vector<hpx::naming::id_type> localities = hpx::find_all_localities();

    std::vector<hpx::lcos::future<Event>> futures;
    futures.reserve(localities.size());
    std::string name("evtid");
    Event evt(name, 15);
    Simple s(23.4);
    for (hpx::naming::id_type const &node : localities)
    {
        futures.push_back(hpx::async<hello_world_action>(node, evt, s));
    }
    hpx::wait_all(futures);
    
    for (auto &&f: futures){
        auto result = f.get();
        hpx::cout << "Got result from future: " << result.print_state() << hpx::endl;
    }
    return 0;
}