#include "dataclass.h"
#include <random>

Event::Event(const std::string& name, size_t n_elem) : data(n_elem), name(name) {
    std::mt19937 gen(std::random_device{}());
    std::uniform_int_distribution<> distr(0, 10);
    for (auto it = data.begin(); it != data.end(); ++it) {
        *it = distr(gen);
    }
}

Event::Event(const std::string& name, const std::vector<int>& data) : data(data), name(name) {}

std::ostream& operator<<(std::ostream& os, const Event& e) {
    os << e.name << "(";
    for (auto it = e.data.cbegin(); it != e.data.cend(); ++it) {
        os << *it;
        if ((it + 1) != e.data.cend()) os << ",";
    }
    os << ")";
    return os;
}