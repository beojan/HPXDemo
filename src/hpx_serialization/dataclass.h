#pragma once

#include <hpx/modules/serialization.hpp>
#include <string>
#include <vector>

/**
 * Can't serialize either of these two classes to another node if we have no default constructor.
 **/

// set with cmake -DUSE_DEFAULT_CONSTRUCTOR
//#define USE_DEFAULT_CONSTRUCTOR 1

using namespace std;

class Event {
    friend class hpx::serialization::access;
    vector<int> data;
    string name;
    template <class Archive> void serialize(Archive& ar, unsigned int version);

  public:
#ifdef USE_DEFAULT_CONSTRUCTOR
    Event() = default;
#else
    Event() = delete;
#endif
    Event(const string& name, size_t n_elem);
    Event(const string& name, const vector<int>& data);

    friend ostream& operator<<(ostream& os, const Event& e);
};

// Simple class with just a double and no extra memory allocated
class Simple {
    double elem;
    friend class hpx::serialization::access;
    template <class Archive> void serialize(Archive& ar, unsigned int version);

  public:
#ifdef USE_DEFAULT_CONSTRUCTOR
    Simple() = default;
#else
    Simple() = delete;
#endif
    Simple(double);

    double getElem() { return elem; }
};

template <class Archive> void Simple::serialize(Archive& ar, unsigned int version) { ar& elem; }

template <class Archive> void Event::serialize(Archive& ar, unsigned int version) {
    ar& name& data;
}

namespace hpx {
namespace serialization {
// We can use serialize member function instead of overriding save_construct_data

// template <class Archive>
// inline void save_construct_data(Archive &ar, Event const *t, unsigned file_version)
// {
// hpx::cout << "in save_construct_data" << hpx::endl;
// ar << t->name << t->data;
// }

#ifndef USE_DEFAULT_CONSTRUCTOR
template <class Archive>
inline void load_construct_data(Archive& ar, Event* t, unsigned file_version) {
    string name;
    vector<int> v;
    ar& name& v;
    ::new (t) Event(name, v);
}
template <class Archive>
inline void load_construct_data(Archive& ar, Simple* t, unsigned file_version) {
    double d;
    ar& d;
    ::new (t) Simple(d);
}
#endif
} // namespace serialization
} // namespace hpx