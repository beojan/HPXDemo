#include "dataclass.h"
#include <sstream>
#include <random>

Event::Event(std::string &name, size_t n_elem) : data(n_elem), name(name)
{
    std::mt19937 gen(std::random_device{}());
    std::uniform_int_distribution<> distr(0, 10);
    for (auto it = data.begin(); it != data.end(); ++it)
    {
        *it = distr(gen);
    }
}

Event::Event(std::string const &name, std::vector<int> const &data) : data(data), name(name)
{
}

Simple::Simple(double elem) : elem(elem) {}

std::string Event::print_state()
{
    std::stringstream ss;
    ss << name << "(";
    for (auto it = data.cbegin(); it != data.cend(); ++it)
    {
        ss << *it;
        if ((it + 1) != data.cend())
            ss << ",";
    }
    ss << ")";
    return ss.str();
}