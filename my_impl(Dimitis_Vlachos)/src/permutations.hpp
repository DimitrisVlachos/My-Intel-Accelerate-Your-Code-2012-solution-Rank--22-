#ifndef __permutattions_hpp__
#define __permutattions_hpp__
/*
    Author : Dimitris Vlachos ( dimitrisv22@gmail.com )
*/

#include "base.hpp"

struct composite_sequence_t {
    std::vector<override_stl_allocator(travel_t)> fields;
 
};

struct permutation_sequence_t {
    std::vector<override_stl_allocator(travel_t)> travels;
    std::vector<override_stl_allocator(travel_t)> path;
    std::vector<override_stl_allocator(composite_sequence_t)> combosite;

    indexed_string_t loc;
};
 
class path_permutations_c {
    private:

    //Description
    std::string m_description;

    //Ring buffer which keeps a record of previous states
    std::vector<override_stl_allocator(permutation_sequence_t)> m_permutations_ring_buffer;

    //Current offset in ring buffer
    uint32_t m_permutations_ring_buffer_head;

    //Variable ring buffer size...
    uint32_t m_permutations_ring_buffer_tail;

    public:

    path_permutations_c(const std::string& desc,const uint32_t ring_buffer_size) : m_description(desc) { init(ring_buffer_size); }
    path_permutations_c() : m_description("")  { init(64); }
    ~path_permutations_c() {}

    //Full cycle
  

    void cycle(const indexed_string_t loc,const std::vector<override_stl_allocator(travel_t)>& travels,
                                            const std::vector<override_stl_allocator(travel_t)>& path,
                                            std::vector<override_stl_allocator(composite_sequence_t)>& cseq);

    void cycle(const indexed_string_t loc,const std::vector<override_stl_allocator(travel_t)>& travels,
                                            std::vector<override_stl_allocator(composite_sequence_t)>& cseq,
                                            const uint32_t travels_start,const uint32_t travels_end);

    void cycle(const indexed_string_t loc,const std::vector<override_stl_allocator(travel_t)>& travels,
                                            const std::vector<override_stl_allocator(travel_t)>& path);
 

    void cycle(const indexed_string_t loc,const std::vector<override_stl_allocator(travel_t)>& travels,
                                    const std::vector<override_stl_allocator(travel_t)>& path,
                                    const uint32_t travels_start,const uint32_t travels_end);

    void cycle(const indexed_string_t loc,const std::vector<override_stl_allocator(travel_t)>& travels,
                                    const std::vector<override_stl_allocator(travel_t)>& path,
                                    const uint32_t travels_start,const uint32_t travels_end,
                                    const uint32_t path_start,const uint32_t path_end);

    //Partial cycling to reduce copies
    void cycle(const indexed_string_t loc,const std::vector<override_stl_allocator(travel_t)>& travels);

    void cycle(const std::vector<override_stl_allocator(travel_t)>& path);
    void cycle() ;
    void invalidate() ;

    permutation_sequence_t* match(const indexed_string_t loc,
                                  const std::vector<override_stl_allocator(travel_t)>& travels);
    permutation_sequence_t* match(const indexed_string_t loc,
                                  const std::vector<override_stl_allocator(travel_t)>& travels,
                                  const uint32_t travels_start,const uint32_t travels_end);
    void init(uint32_t ring_buffer_size);
    void shutdown();
};

#endif

