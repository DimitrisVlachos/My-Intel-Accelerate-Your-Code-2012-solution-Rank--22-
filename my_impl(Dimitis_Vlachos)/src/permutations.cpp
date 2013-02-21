/*
    Author : Dimitris Vlachos ( dimitrisv22@gmail.com )
*/

#include "permutations.hpp"
 
//Full cycle

 

void path_permutations_c::cycle(const indexed_string_t loc,const std::vector<override_stl_allocator(travel_t)>& travels,
                                            const std::vector<override_stl_allocator(travel_t)>& path,
                                            std::vector<override_stl_allocator(composite_sequence_t)>& cseq) {

    m_permutations_ring_buffer[m_permutations_ring_buffer_head].loc = loc;
    m_permutations_ring_buffer[m_permutations_ring_buffer_head].travels = travels;
    m_permutations_ring_buffer[m_permutations_ring_buffer_head].path = path;
    m_permutations_ring_buffer[m_permutations_ring_buffer_head].combosite.clear();
    m_permutations_ring_buffer[m_permutations_ring_buffer_head].combosite = cseq;

    m_permutations_ring_buffer_head = (m_permutations_ring_buffer_head + 1) % m_permutations_ring_buffer_tail;
}

void path_permutations_c::cycle(const indexed_string_t loc,const std::vector<override_stl_allocator(travel_t)>& travels,
                                            std::vector<override_stl_allocator(composite_sequence_t)>& cseq,
                                            const uint32_t travels_start,const uint32_t travels_end) {
    m_permutations_ring_buffer[m_permutations_ring_buffer_head].loc = loc;

    m_permutations_ring_buffer[m_permutations_ring_buffer_head].travels.clear();
    m_permutations_ring_buffer[m_permutations_ring_buffer_head].travels.resize(travels_end - travels_start);

    std::copy(travels.begin() + travels_start,travels.begin() + travels_end,
    m_permutations_ring_buffer[m_permutations_ring_buffer_head].travels.begin());


    m_permutations_ring_buffer[m_permutations_ring_buffer_head].combosite.clear();
    m_permutations_ring_buffer[m_permutations_ring_buffer_head].combosite = cseq;

    m_permutations_ring_buffer_head = (m_permutations_ring_buffer_head + 1) % m_permutations_ring_buffer_tail;
}

void path_permutations_c::cycle(const indexed_string_t loc,const std::vector<override_stl_allocator(travel_t)>& travels,
                                const std::vector<override_stl_allocator(travel_t)>& path) {
    m_permutations_ring_buffer[m_permutations_ring_buffer_head].loc = loc;
    m_permutations_ring_buffer[m_permutations_ring_buffer_head].travels = travels;
    m_permutations_ring_buffer[m_permutations_ring_buffer_head].path = path;
    m_permutations_ring_buffer_head = (m_permutations_ring_buffer_head + 1) % m_permutations_ring_buffer_tail;
}

void path_permutations_c::cycle(const indexed_string_t loc,const std::vector<override_stl_allocator(travel_t)>& travels,
                                const std::vector<override_stl_allocator(travel_t)>& path,
                                const uint32_t travels_start,const uint32_t travels_end,
                                const uint32_t path_start,const uint32_t path_end) {
    m_permutations_ring_buffer[m_permutations_ring_buffer_head].loc = loc;

    m_permutations_ring_buffer[m_permutations_ring_buffer_head].travels.clear();
    m_permutations_ring_buffer[m_permutations_ring_buffer_head].travels.resize(travels_end - travels_start);

    std::copy(travels.begin() + travels_start,travels.begin() + travels_end,
    m_permutations_ring_buffer[m_permutations_ring_buffer_head].travels.begin());

    m_permutations_ring_buffer[m_permutations_ring_buffer_head].path.clear();
    m_permutations_ring_buffer[m_permutations_ring_buffer_head].path.resize(path_end - path_start);

    std::copy(path.begin() + travels_start,path.begin() + path_end,
    m_permutations_ring_buffer[m_permutations_ring_buffer_head].path.begin());

    m_permutations_ring_buffer_head = (m_permutations_ring_buffer_head + 1) % m_permutations_ring_buffer_tail;
}

void path_permutations_c::cycle(const indexed_string_t loc,const std::vector<override_stl_allocator(travel_t)>& travels,
                                const std::vector<override_stl_allocator(travel_t)>& path,
                                const uint32_t travels_start,const uint32_t travels_end) {
    m_permutations_ring_buffer[m_permutations_ring_buffer_head].loc = loc;

    m_permutations_ring_buffer[m_permutations_ring_buffer_head].travels.clear();
    m_permutations_ring_buffer[m_permutations_ring_buffer_head].travels.resize(travels_end - travels_start);

    std::copy(travels.begin() + travels_start,travels.begin() + travels_end,
    m_permutations_ring_buffer[m_permutations_ring_buffer_head].travels.begin());

    m_permutations_ring_buffer[m_permutations_ring_buffer_head].path = path;

    m_permutations_ring_buffer_head = (m_permutations_ring_buffer_head + 1) % m_permutations_ring_buffer_tail;
}


//Partial cycling to reduce copies
void path_permutations_c::cycle(const indexed_string_t loc,const std::vector<override_stl_allocator(travel_t)>& travels) {
    m_permutations_ring_buffer[m_permutations_ring_buffer_head].loc = loc;
    m_permutations_ring_buffer[m_permutations_ring_buffer_head].travels = travels;
}

void path_permutations_c::cycle(const std::vector<override_stl_allocator(travel_t)>& path) {
    m_permutations_ring_buffer[m_permutations_ring_buffer_head].path = path;
}

void path_permutations_c::cycle() {
    m_permutations_ring_buffer_head = (m_permutations_ring_buffer_head + 1) % m_permutations_ring_buffer_tail;
}

void path_permutations_c::invalidate() {
    for (uint32_t i = 0;i < m_permutations_ring_buffer_tail;++i) {
        m_permutations_ring_buffer[i].loc = (indexed_string_t)std::numeric_limits<indexed_string_t>::max(); 
        m_permutations_ring_buffer[i].combosite.clear(); 
        m_permutations_ring_buffer[i].path.clear(); 
    }
}

permutation_sequence_t* path_permutations_c::match(const indexed_string_t loc,
                                                    const std::vector<override_stl_allocator(travel_t)>& travels) {
    if (!travels.empty()) {
        const uint32_t m1 = travels.size();
        for (uint32_t i = 0;i < m_permutations_ring_buffer_tail;++i) {
            if (m_permutations_ring_buffer[i].loc != loc) {
                continue;
            }

            const uint32_t m2 = m_permutations_ring_buffer[i].travels.size();
            if (m2 != m1) {
                continue;
            }
     
            for (uint32_t j = 0;j < m2;++j) {
                if (m_permutations_ring_buffer[i].travels[j].flights.size() != travels[j].flights.size()) {    
                    continue;
                }  

                const std::vector<override_stl_allocator(flight_indice_t)>& va = m_permutations_ring_buffer[i].travels[j].flights;
                const std::vector<override_stl_allocator(flight_indice_t)>& vb = travels[j].flights;
                boolean_t match = true;

                for (uint32_t k = 0,l = m_permutations_ring_buffer[i].travels[j].flights.size();k < l;++k) {
                    if (va[k] != vb[k]) {
                        match = false;
                        break;
                    }
                }

                if (match != false) {
                    return &m_permutations_ring_buffer[i];
                }
            }
        }
    }
    return 0;
}

permutation_sequence_t* path_permutations_c::match(const indexed_string_t loc,
                                                    const std::vector<override_stl_allocator(travel_t)>& travels,
                                                    const uint32_t travels_start,const uint32_t travels_end) {
    if (!travels.empty()) {
        const uint32_t m1 = travels_end - travels_start;
        for (uint32_t i = 0;i < m_permutations_ring_buffer_tail;++i) {
            if (m_permutations_ring_buffer[i].loc != loc) {
                continue;
            }

            const uint32_t m2 = m_permutations_ring_buffer[i].travels.size();
            if (m2 != m1) {
                continue;
            }
     
            for (uint32_t j = 0;j < m2;++j) {
                if (m_permutations_ring_buffer[i].travels[j].flights.size() != travels[travels_start + j].flights.size()) {    
                    continue;
                }  

                const std::vector<override_stl_allocator(flight_indice_t)>& va = m_permutations_ring_buffer[i].travels[j].flights;
                const std::vector<override_stl_allocator(flight_indice_t)>& vb = travels[travels_start + j].flights;
                boolean_t match = true;

                for (uint32_t k = 0,l = m_permutations_ring_buffer[i].travels[j].flights.size();k < l;++k) {
                    if (va[k] != vb[k]) {
                        match = false;
                        break;
                    }
                }

                if (match != false) {
                    return &m_permutations_ring_buffer[i];
                }
            }
        }
    }
    return 0;
}

void path_permutations_c::init(uint32_t ring_buffer_size) {
    if (!ring_buffer_size) {
            printf("permutations::init (%s):: WARNING ring buffer was set to 0!Will force it to 1! Please check your -perm_size argument!\n",
            m_description.c_str());
            ring_buffer_size = 1;
    }
    printf("permutations::init (%s) | Ring buffer size : %u (set -perm_size N(=constant integer) to extend its size)\n",
            m_description.c_str(),ring_buffer_size);
    m_permutations_ring_buffer_tail = ring_buffer_size;
    m_permutations_ring_buffer_head = 0;
    m_permutations_ring_buffer.clear();
    m_permutations_ring_buffer.reserve(m_permutations_ring_buffer_tail);

    for (uint32_t i = 0;i < m_permutations_ring_buffer_tail;++i) {
        m_permutations_ring_buffer.push_back(permutation_sequence_t());
    }

    invalidate();
}

void path_permutations_c::shutdown() {
    m_permutations_ring_buffer.clear();
}


