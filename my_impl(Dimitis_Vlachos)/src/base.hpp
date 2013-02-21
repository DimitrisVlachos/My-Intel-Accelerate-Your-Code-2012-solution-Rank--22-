
#ifndef _base_hpp_
#define _base_hpp_
/*
    Author : Dimitris Vlachos ( dimitrisv22@gmail.com )
*/
#include "types.hpp"
#include "static_strings.hpp"

typedef uint32_t flight_indice_t;
typedef uint64_t travel_indice_t;
typedef uint64_t travel_flight_indice_pair_t;
typedef uint64_t travel_indice_pair_t;

/*Don't play with them ...the app is hardcoded to use only 2*/
static const travel_indice_t k_invalid_relation = (travel_indice_t)std::numeric_limits<travel_indice_t>::max();
static const travel_indice_t k_mate = (travel_indice_t)1;
static const travel_indice_t k_node_zero = 0;
static const travel_indice_t k_node_one = 1;
static const travel_indice_t k_node_range = 2;
static const travel_indice_t k_relation_shift = (sizeof(travel_indice_t) << 3) >> 1;

enum flight_class_t {
    flight_class_a = 0,
    flight_class_b = 1,
    flight_class_c = 2,
    flight_class_d = 3,
    flight_class_invalid = 4,
};

/**
 * \struct Parameters
 * \brief Store the program's parameters.
 * This structure don't need to be modified but feel free to change it if you want.
 */
struct Parameters {
    indexed_string_t from;                  /*!< The city where the travel begins */
    indexed_string_t to;                    /*!< The city where the conference takes place */
    uint64_t dep_time_min;                  /*!< The minimum departure time for the conference (epoch). 
                                            No flight towards the conference's city must be scheduled before this time. */
    uint64_t dep_time_max;                  /*!< The maximum departure time for the conference (epoch). No flight towards the conference's                                                  city must be scheduled after this time.  */
    uint64_t ar_time_min;                   /*!< The minimum arrival time after the conference (epoch). No flight from the conference's                                                 city must be scheduled before this time.  */
    uint64_t ar_time_max;                   /*!< The maximum arrival time after the conference (epoch). No flight from the conference's                                                 city must be scheduled after this time.  */
    uint64_t max_layover_time;              /*!< You don't want to wait more than this amount of time at the airport between 2 flights (in                                                  seconds) */
    uint64_t vacation_time_min;         /*!< Your minimum vacation time (in seconds). You can't be in a plane during this time. */
    uint64_t vacation_time_max;         /*!< Your maximum vacation time (in seconds). You can't be in a plane during this time. */
    std::list<indexed_string_t> airports_of_interest;/*!< The list of cities you are interested in. */
    std::string flights_file;               /*!< The name of the file containing the flights. */
    std::string alliances_file;             /*!< The name of the file containing the company alliances. */
    std::string work_hard_file;             /*!< The file used to output the work hard result. */
    std::string play_hard_file;             /*!< The file used to output the play hard result. */
    int32_t nb_threads;                     /*!< The maximum number of worker threads */    
    int32_t b_silent;                       /*!< Dump stuff in console..?*/
    int32_t perm_size;                      /*Size of permutation ring buffer*/
    uint32_t merge_buffer_thresold;         /*Thresold per part*/
};

extern "C" {
    union flight_ref_t {
        struct {
            indexed_string_t from_hash;     /*!< City where you take off. */
            indexed_string_t to_hash;       /*!< City where you land. */
            indexed_string_t company_hash;  /*!< The company's name. */
            indexed_string_t id_hash;       /*!< Unique id of the flight. */
            uint64_t take_off_time;         /*!< Take off time (epoch). */
            uint64_t land_time;             /*!< Land time (epoch). */
            uint32_t index;                 /*(relative)Index in flight list*/
            f32 cost;                       /*!< The cost of the flight. */
            f32 discount;                   /*!< The discount applied to the cost. */
        };
        uint8_t _align[64];                 /*1 cache line*/
    };
}

/**
 * \struct travel_t
 * \brief Store a travel.
 * This structure don't need to be modified but feel free to change it if you want.
 */
struct travel_t {
    travel_t() : relation(k_invalid_relation) , node(k_node_zero) {}
 
    inline travel_t& operator= (const travel_t& other) {
        if (&other == this) {
            return *this;
        }
        this->flights = other.flights;
        this->relation = other.relation;
        this->node = other.node;
        return *this;
    }

    inline void mate(const travel_indice_t a,const travel_indice_t b,const travel_indice_t n) {
        this->node = n;
        this->relation = (a << k_relation_shift) | b;
    }

    travel_indice_t relation;
    uint8_t node;
    std::vector<override_stl_allocator(flight_indice_t)> flights;       /*!< A travel is just a list of indices to flights. */
};



#endif

