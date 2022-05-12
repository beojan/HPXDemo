#pragma once

// also includes hpx/serialization/vector.hpp and hpx/serialization/string.hpp
#include <hpx/modules/serialization.hpp>
/**
 * Segfault when serializing Event to remote nodes:
 * src/tcmalloc.cc:333] Attempt to free invalid pointer
 **/

#ifndef DEFAULT_CONSTRUCTIBLE

class Event;

namespace hpx::serialization {

template <class Archive> inline void save_construct_data(Archive&, Event const*, unsigned int);
} // namespace hpx::serialization
#endif

class Event {
    std::vector<int> data;
    std::string name;
    template <class Archive> void serialize(Archive&, const unsigned int);

  public:
#ifdef DEFAULT_CONSTRUCTIBLE
    Event() = default;
#else
    Event() = delete;
    template <class Archive>
    friend void hpx::serialization::save_construct_data(Archive&, Event const*, unsigned int);
#endif
    Event(const std::string&, size_t);
    Event(const std::string&, const std::vector<int>&);

    friend std::ostream& operator<<(std::ostream&, const Event&);
    friend class hpx::serialization::access;
};

std::ostream& operator<<(std::ostream&, const Event&);

template <class Archive> void Event::serialize(Archive& ar, const unsigned int version) {
#ifdef DEFAULT_CONSTRUCTIBLE
    ar& name& data;
#endif
}

#ifndef DEFAULT_CONSTRUCTIBLE
namespace hpx::serialization {

template <class Archive>
inline void save_construct_data(Archive& ar, Event const* t, const unsigned int file_version) {
    ar & t->name & t->data;
}

template <class Archive>
inline void load_construct_data(Archive& ar, Event* t, const unsigned int file_version) {
    std::string name;
    std::vector<int> data;
    ar & name & data;
    ::new (t) Event(name, data);
}

} // namespace hpx::serialization
#endif