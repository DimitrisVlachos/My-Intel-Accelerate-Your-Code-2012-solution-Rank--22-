
#ifndef _mt_hpp_
#define _mt_hpp_

/*
    Author : Dimitris Vlachos ( dimitrisv22@gmail.com )
*/

#include "base.hpp"

boolean_t mt_init(const Parameters& params);
boolean_t mt_set_work_data(std::vector<flight_ref_t>& flights_ref,
                 const std::vector<std::vector<indexed_string_t>>& alliances);
//boolean_t mt_ss_match(uint32_t& result_offset,std::vector<std::string>& children,const std::string& look_for);
void mt_get_flights(flight_ref_t*& ptr,uint32_t& size);
void mt_compute_path(const indexed_string_t to,std::vector<override_stl_allocator(travel_t)>& travels,uint64_t t_min,uint64_t t_max);
void mt_fill_travel(std::vector<override_stl_allocator(travel_t)>& travels,const indexed_string_t starting_point, uint64_t t_min, uint64_t t_max);
void mt_merge_path(std::vector<override_stl_allocator(travel_t)>& travel1,std::vector<override_stl_allocator(travel_t)>& travel2,
    const travel_indice_t relation,const travel_indice_t node);
void mt_find_cheapest(travel_t& result,std::vector<override_stl_allocator(travel_t)>& travels,
                    std::vector<std::vector<indexed_string_t> >&alliances);
void mt_shutdown();
void mt_copy_travel(std::vector<override_stl_allocator(travel_t)>* dst,std::vector<override_stl_allocator(travel_t)>* src,const uint32_t dst_base,const uint32_t len);

void mt_merge_phase_link_node(const std::vector<override_stl_allocator(travel_t)>& a,
                                   const std::vector<override_stl_allocator(travel_t)>& b,
                                   const travel_indice_t node);
void mt_init_merge_phase_relations();
void mt_shutdown_merge_phase_relations();

void mt_merge_path_impl(std::vector<override_stl_allocator(travel_t)>& travel1,
                                std::vector<override_stl_allocator(travel_t)>& travel2,
                                std::vector<override_stl_allocator(travel_t)>& results,
                                const uint32_t travel1_start,
                                const uint32_t travel1_end,
                                const travel_indice_t relation,
                                const travel_indice_t node);
#endif

