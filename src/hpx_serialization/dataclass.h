#pragma once

#include <hpx/iostream.hpp>
#include <hpx/serialization.hpp>
#include <hpx/serialization/string.hpp>
#include <hpx/serialization/vector.hpp>
#include <string>
#include <vector>

/**
 * Segfault when serializing Event to remote nodes:
 * src/tcmalloc.cc:333] Attempt to free invalid pointer
 **/

#ifndef USE_DEFAULT_CONSTRUCTOR

class Event;
class Simple;

namespace hpx::serialization {

template <class Archive> inline void save_construct_data(Archive&, Event const*, unsigned);

template <class Archive> inline void save_construct_data(Archive&, Simple const*, unsigned);
} // namespace hpx::serialization
#endif

class Event {
    std::vector<int> data;
    std::string name;
    template <class Archive> void serialize(Archive& ar, unsigned int version);

  public:
#ifdef USE_DEFAULT_CONSTRUCTOR
    Event() = default;
#else
    Event() = delete;
#endif
    Event(const std::string& name, size_t n_elem);
    Event(const std::string& name, const std::vector<int>& data);

    friend std::ostream& operator<<(std::ostream& os, const Event& e);
    template <class Archive>
    friend void hpx::serialization::save_construct_data(Archive&, Event const*, unsigned);
    friend class hpx::serialization::access;
};

// Simple class with just a double and no extra memory allocated
class Simple {
    double elem;
    template <class Archive> void serialize(Archive& ar, unsigned int version);

  public:
#ifdef USE_DEFAULT_CONSTRUCTOR
    Simple() = default;
#else
    Simple() = delete;
#endif
    Simple(double);

    decltype(elem) getElem() const { return elem; }
    friend class hpx::serialization::access;
    template <class Archive>
    friend void hpx::serialization::save_construct_data(Archive&, Simple const*, unsigned);
};

template <class Archive> void Simple::serialize(Archive& ar, unsigned int version) {
    // ar& elem;
}

template <class Archive> void Event::serialize(Archive& ar, unsigned int version) {
    // ar& name& data;
}

#ifndef USE_DEFAULT_CONSTRUCTOR
namespace hpx::serialization {

template <class Archive>
inline void save_construct_data(Archive& ar, Event const* t, unsigned file_version) {
    ar & t->name & t->data;
}

template <class Archive>
inline void save_construct_data(Archive& ar, Simple const* t, unsigned file_version) {
    ar & t->elem;
}

template <class Archive>
inline void load_construct_data(Archive& ar, Event* t, unsigned file_version) {
    std::string name;
    std::vector<int> v;
    ar& name& v;
    ::new (t) Event(name, v);
}
template <class Archive>
inline void load_construct_data(Archive& ar, Simple* t, unsigned file_version) {
    double d;
    ar& d;
    ::new (t) Simple(d);
}

} // namespace hpx::serialization
#endif