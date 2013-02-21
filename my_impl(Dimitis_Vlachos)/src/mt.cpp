/*
    mt.cpp : multithreaded operations
    Author : Dimitris Vlachos ( dimitrisv22@gmail.com )
*/
#include "mt.hpp"
#include "io.hpp"
#include "permutations.hpp"

extern "C" {
    #include <pthread.h>
    #include <unistd.h>
}

struct alliance_t {                                                         /*A copy of the alliance list for each thread*/
    std::vector<std::vector<indexed_string_t>> alliances;
};
 
struct compute_path2_args_t {                                               
    std::vector<override_stl_allocator(travel_t)>* input;                    /*Input vector to be proccessed*/
    std::vector<override_stl_allocator(travel_t)>* output;                   /*Remainder to be summed up*/
    std::vector<override_stl_allocator(travel_t)>* final_travels;            /*Final travel list to be returned*/
    indexed_string_t to;                                                     /*Hash of destination*/
    uint32_t start,end;                                                     /*Start/end offsets in flight list*/
    uint32_t flight_count,thread_index;                                     /*Number of flights , thread index*/
    int64_t start2,end2;                                                    /*Start/end offsets in input list (bottom->top)*/
    uint64_t t_min,t_max,max_layover_time;                                  /*Time upper/lower bound*/         
};

struct fill_travel_args_t {                 
    uint32_t start;                                                         /*First element in flight list*/
    uint32_t end;                                                           /*Last element in flight list*/
    uint32_t thread_index;                                                  /*Thread index*/
    uint32_t flight_count;                                                  /*Number of flights*/
    uint64_t t_min,t_max;                                                   /*time Upper/Lower bound*/
    std::vector<override_stl_allocator(travel_t)>* results;                  /*Partial result*/
    indexed_string_t starting_point;                                         /*Source point hash*/
};

struct merge_path_args_t {              
    std::vector<override_stl_allocator(travel_t)>* travel1;                   /*Source travel 1*/
    std::vector<override_stl_allocator(travel_t)>* travel2;                   /*Source travel 2*/
    std::vector<override_stl_allocator(travel_indice_pair_t)>* results;     /*Partial results*/
    uint32_t start,end,start2,end2;                                          /*Start/End offsets in travel1/travel2 lists*/
    uint32_t thread_index;                                                   /*Thread index*/
    uint32_t flight_count;                                                   /*Number of flights*/
};
 
struct find_cheapest_args_t {
    int64_t start,end;                                                       /*Start,end offsets in travel list*/
    uint32_t best_ind,thread_index,flight_count;                             /*Best indice,thread index,number of flights*/
    f32 best_cost;                                                            /*Best cost for this thread*/
    std::vector<override_stl_allocator(travel_t)>* travels;                   /*Input travels*/
    travel_t* out_travel;
};

struct ss_match_args_t {                                                     /*for static_strings.cpp*/
    uint32_t start,end;                                                      /*Start/End offsets in children list*/
    uint32_t found,offset;                                                   /*Found match flag , offset of matched string*/
    std::vector<std::string>* children;                                       /*Input vector with all children*/
    std::string* look_for;                                                    /*String to look for*/
};

struct copy_travel_args_t {                                                   /*for mt_copy_travel_entry_point*/
    std::vector<override_stl_allocator(travel_t)>* src;                        /*Input*/
    std::vector<override_stl_allocator(travel_t)>* dst;                        /*Output*/
    uint32_t start,end;                                                       /*Length*/
    uint32_t base;                                                            /*Base in dst*/
};

struct merge_phase_node_t {                                                     /*Used to store merge states...*/
    std::vector<override_stl_allocator(travel_t)> a;
    std::vector<override_stl_allocator(travel_t)> b;
};

struct merge_phase_relation_t {
    std::vector<override_stl_allocator(merge_phase_node_t)> nodes;
};


struct extent_t {                                                              /*Used for thread work subdivision*/
    uint32_t s0,s1;                                                            /*start - end offsets : length = s1 - s0*/
    extent_t() : s0(0),s1(1) {}
    extent_t(const uint32_t in_s0,const uint32_t in_s1) : s0(in_s0) , s1(in_s1) {}
};

pthread_t* g_thread_context;                                                     /*Allocated threads*/
Parameters* g_parameters;                                                        /*A copy of Parameters*/
flight_ref_t* g_flights;                                                         /*A copy of all flights * number of threads*/
uint64_t** g_thread_context_res;                                                /*Thread results*/
uint32_t g_thread_contexts;                                                     /*Number of thread contexts*/
uint32_t g_flights_size;                                                        /*Number of flights*/
uint32_t g_mt_initialized = 0;                                                  /*Module initialization flag*/
    
std::vector<alliance_t> g_alliances;                                             /*A copy of the alliance list*/
std::vector<override_stl_allocator(merge_phase_relation_t)>* g_merge_phase_relations;   /*All relations in this merge phase*/
path_permutations_c* g_global_permutations;                                      /*Global permutations*/

/*Thread entry point functions fw-decl*/
static void* mt_fill_travel_entry_point(void* in_args);                        /*MT version of fill_travels*/
static void* mt_merge_path_entry_point(void* in_args);                         /*MT version of merge_path*/
static void* mt_find_cheapest_entry_point(void* in_args);                      /*MT version of find_cheapest*/
static void* mt_compute_path2_entry_point(void* in_args);                     /*MT version of compute_path */
static void* mt_copy_travel_entry_point(void* in_args);                        
 


static void join_nodes(travel_t& out,const travel_t& in,const uint32_t thread_index);

/*Wait for all threads to finish their task*/
static void mt_wait_threads(const uint32_t active_threads) {
    for (uint32_t i = 0;i < active_threads;++i) {
        if (0 != pthread_join(g_thread_context[i],(void**)&g_thread_context_res[i])) {
            printf("pthread_join failed!\n");
            assert(0);
        }
    }
}

/*Calculate the tile size of each worker thread*/
static void calculate_extent(std::vector<extent_t>& result,const uint32_t slice_len,
                                const uint32_t thread_count) {
    const uint32_t part_per_slice = (slice_len >= thread_count) ? slice_len / thread_count : 1;

    result.clear();
    result.reserve(thread_count);

    for (uint32_t i = 0,start = 0;i < thread_count;++i) {
        uint32_t end = (start + part_per_slice);
        end = (end > slice_len) ? slice_len : end;
 
        if ((i + 1) == thread_count) {
            end = slice_len;
        }
        
        result.push_back(extent_t(start,end));

        //Last tile
        if (end >= slice_len) {
            break;
        }

        start = end;
    }

    //Apply borrowing?
}

/*
    Links two nodes and creates a copy of them for each thread...
*/
void mt_merge_phase_link_node(const std::vector<override_stl_allocator(travel_t)>& a,
                                   const std::vector<override_stl_allocator(travel_t)>& b,
                                   const travel_indice_t node) {
    for (uint32_t i = 0;i < g_thread_contexts;++i) {
        g_merge_phase_relations->at(i).nodes[node].a = a;
        g_merge_phase_relations->at(i).nodes[node].b = b;
    }
}

/*
    Initializes merge phase relation list
*/
void mt_init_merge_phase_relations() {
    g_merge_phase_relations = new std::vector<override_stl_allocator(merge_phase_relation_t)>();
    assert(g_merge_phase_relations != 0);

    for (uint32_t i = 0;i < g_thread_contexts;++i) { //We need one for each thread!
        g_merge_phase_relations->push_back(merge_phase_relation_t());   

        for (uint32_t j = 0;j < k_node_range;++j) {    //0...upper bound
            g_merge_phase_relations->back().nodes.push_back(merge_phase_node_t());
        }
    }
}

void mt_shutdown_merge_phase_relations() {
    delete g_merge_phase_relations;
    g_merge_phase_relations = 0;
}

/*Returns actual flights ptr/size used by this MT session*/
void mt_get_flights(flight_ref_t*& ptr,uint32_t& size) {
    ptr = g_flights;
    size = g_flights_size;
}

/*The MT version of merge_path*/
void mt_merge_path(std::vector<override_stl_allocator(travel_t)>& travel1,std::vector<override_stl_allocator(travel_t)>& travel2,const travel_indice_t relation,const travel_indice_t node) {

    if (travel2.empty()) {
        travel1.clear();
        return;
    } else if (travel1.empty()) {
        travel2.clear();
        return;
    }

    const uint64_t thresold = g_parameters[0].merge_buffer_thresold;
    std::vector<override_stl_allocator(travel_t)> result;
    uint32_t head = 0,tail = (uint32_t)travel1.size();

    while (head < tail) {
        uint32_t len = thresold;
        if ((head + len) > tail) {
            len = tail - head;
        }

        mt_merge_path_impl(travel1,travel2,result,head,head + len,relation,node);
        head += len;
    }

    travel1 = result;
}

void mt_merge_path_impl(std::vector<override_stl_allocator(travel_t)>& travel1,
                                std::vector<override_stl_allocator(travel_t)>& travel2,
                                std::vector<override_stl_allocator(travel_t)>& results,
                                const uint32_t travel1_start,
                                const uint32_t travel1_end,
                                const travel_indice_t relation,
                                const travel_indice_t node) {
    const uint32_t thread_count = g_thread_contexts;
    std::vector<extent_t> extent;
    uint32_t e;
    merge_path_args_t* my_arg;
  
    //Calculate tile size per worker thread
    calculate_extent(extent,travel1_end - travel1_start,thread_count); //travel_t1 / N
    e = extent.size();

    //Initialize contexts
    my_arg = new merge_path_args_t[thread_count];
    assert(my_arg != 0);
 
 
    boolean_t skip_one_copy = true; //Don't copy travel2 for one of the instances

    for (uint32_t i = 0;i < e;++i) {
        my_arg[i].results =  new std::vector<override_stl_allocator(travel_indice_pair_t)>();
        assert(my_arg[i].results != 0);
        my_arg[i].travel1 = &travel1;
        my_arg[i].travel2 = (skip_one_copy) ? &travel2  : new std::vector<override_stl_allocator(travel_t)>(travel2);
        my_arg[i].start =  extent[i].s0;
        my_arg[i].end =  extent[i].s1;
       // printf("%u %u / l %u --> %lu\n",my_arg[i].start,my_arg[i].end,travel1_end - travel1_start,travel1.size());
        my_arg[i].thread_index = i;
        my_arg[i].flight_count = g_flights_size;
        my_arg[i].start2 = 0;
        my_arg[i].end2 = travel2.size();
        skip_one_copy = false;
    }
 
    for (uint32_t i = 0;i < e;++i) { 
        pthread_create(&g_thread_context[i],NULL,mt_merge_path_entry_point,(void*)&my_arg[i]);
    }

    //Wait for results
    mt_wait_threads(e);

    for (uint32_t q = 0; q < e;++q) {
        if (&travel2 != my_arg[q].travel2) {
            delete my_arg[q].travel2;
        }

        const uint32_t output_len = my_arg[q].results->size();
        for (uint32_t j = 0;j < output_len;++j) {
            const travel_indice_pair_t& pair = my_arg[q].results->at(j);
            const travel_indice_t a0 = (travel_indice_t)(pair >> 32);
            const travel_indice_t a1 = (travel_indice_t)(pair & 0xffffffff);

            results.push_back(travel_t()); 
            travel_t& new_travel = results.back();
            if (relation == k_mate) {
                new_travel.mate(a0,a1,node);
            } else {
        
                const travel_t& t1 = travel1[a0];
                const travel_t& t2 = travel2[a1];

                const uint32_t tsize = t1.flights.size();
                const uint32_t hsize = t2.flights.size();
        
                new_travel.flights.reserve(tsize + hsize);
                new_travel.relation = k_invalid_relation;

                for (uint32_t i = 0;i < tsize;++i) {    
                    new_travel.flights.push_back(t1.flights[i]);
                }

                for (uint32_t i = 0;i < hsize;++i) {    
                    new_travel.flights.push_back(t2.flights[i]);
                }
            }
        }

        delete my_arg[q].results;
    }

    delete []my_arg;
}



/*The MT version of fill_travel*/
void mt_fill_travel(std::vector<override_stl_allocator(travel_t)>& travels,const indexed_string_t starting_point, uint64_t t_min, uint64_t t_max) {

    const uint32_t thread_count = g_thread_contexts;
    std::vector<extent_t> extent;
    uint32_t exp,e;
    fill_travel_args_t* my_arg;
    const indexed_string_t starting_point_hash = starting_point;

    //Calculate tile size per worker thread
    calculate_extent(extent,g_flights_size,thread_count);
    e = extent.size();

    //Initialize contexts
    my_arg = new fill_travel_args_t[thread_count];
    assert(my_arg != 0);

    //Split work in threads
    for (uint32_t i = 0;i < e;++i) {
        my_arg[i].t_min = t_min;
        my_arg[i].t_max = t_max;
        my_arg[i].starting_point = starting_point_hash;
        my_arg[i].thread_index = i;
        my_arg[i].flight_count = g_flights_size;
        my_arg[i].results = new std::vector<override_stl_allocator(travel_t)>();
        my_arg[i].start = extent[i].s0;
        my_arg[i].end = extent[i].s1;
        pthread_create(&g_thread_context[i],NULL,mt_fill_travel_entry_point,(void*)&my_arg[i]);
    }

    //Wait for results
    mt_wait_threads(e);

    //Sum up wanted size and allocate it
    exp = 0;
    for (uint32_t i = 0; i < e;++i) {
        exp += my_arg[i].results->size();
    }

    travels.clear();
    travels.reserve(exp);

    //Append results + cleanup
    exp = 0;
    for (uint32_t i = 0; i < e;++i) {
        const uint32_t output_len = my_arg[i].results->size();
        for (uint32_t j = 0;j < output_len;++j) {
            travels.push_back(my_arg[i].results->at(j));
        }
        delete my_arg[i].results;
    }
 
    delete []my_arg;
}

/*The MT version of find_cheapest*/
void mt_find_cheapest(travel_t& result,std::vector<override_stl_allocator(travel_t)>& travels,std::vector<std::vector<indexed_string_t> >&alliances) {

    if (travels.empty()) { //Nothing to do
        return;
    }
    
    const uint32_t thread_count = g_thread_contexts;
    uint32_t e;
    find_cheapest_args_t* my_arg; 
    std::vector<extent_t> extent;

    //Calculate tile size per worker thread
    calculate_extent(extent,travels.size(),thread_count);
    e = extent.size();

    //Initialize contexts
    my_arg = new find_cheapest_args_t[thread_count];
    assert(my_arg != 0);

    //Split work in threads
    for (uint32_t i = 0;i < e;++i) {
        my_arg[i].flight_count = g_flights_size;
        my_arg[i].thread_index = i;
        my_arg[i].start = (int64_t)extent[i].s0;
        my_arg[i].end = (int64_t)extent[i].s1;
        my_arg[i].travels = &travels;
        my_arg[i].out_travel = new travel_t;
        pthread_create(&g_thread_context[i],NULL,mt_find_cheapest_entry_point,(void*)&my_arg[i]);
    }

    //Wait for results
    mt_wait_threads(e);

    //Figure out which indice contains the best cost
    const uint32_t bottom = (uint32_t)e-1;
    uint32_t best_ind = bottom;
    f32 best_cost = my_arg[bottom].best_cost;

    for (int64_t i = (int64_t)bottom;i >= 0 ;--i) { 
        if (my_arg[i].best_cost < best_cost) {        
            best_cost = my_arg[i].best_cost;
            best_ind = i;
        }
    }

    //And return it as result

    result = *my_arg[best_ind].out_travel;

    for (uint32_t i = 0;i < e;++i) {
        delete my_arg[i].out_travel;
    }
    travels.clear();
    delete []my_arg;
}

/*Initialize ALL global contexts of mt module*/ 
boolean_t mt_init(const Parameters& params) {
    const uint32_t thread_count = (uint32_t)params.nb_threads;

    g_global_permutations = new path_permutations_c("GLOBAL",(uint32_t)params.perm_size);
    //g_local_permutations = new path_permutations_c("LOCAL",(uint32_t)params.perm_size);

    g_thread_contexts = thread_count;
    g_thread_context = new pthread_t[thread_count];
    assert(g_thread_context != 0);
    g_thread_context_res = new uint64_t*[thread_count];
    assert(g_thread_context_res != 0);
    g_parameters = new Parameters[thread_count];
    assert(g_parameters != 0);

    for (uint32_t i = 0;i < thread_count;++i) {
        g_thread_context[i] = 0;
        g_thread_context_res[i] = NULL;
        g_parameters[i] = params;
    }

 
    //Set initialization flag and return
    g_mt_initialized = 1;
    return true;
}
 
/*Receives all input data for the current session*/
boolean_t mt_set_work_data(std::vector<flight_ref_t>& flights_ref,
                 const std::vector<std::vector<indexed_string_t>>& alliances) {

    const uint32_t thread_count = g_thread_contexts;
 
    //Allocate space for flights list (one for each thread)
    g_flights_size = flights_ref.size();
    g_flights = new flight_ref_t[g_flights_size * thread_count];
    assert(g_flights != 0);
 
    //Create a copy of allianes (one for each thread)
    g_alliances.reserve(thread_count);
 
    //Copy lists
    for (uint32_t i = 0;i < thread_count;++i) {
        g_alliances.push_back(alliance_t());
        g_alliances[i].alliances = alliances;
        flight_ref_t* base = &g_flights[g_flights_size * i];

        for (uint32_t j = 0,k = flights_ref.size();j < k;++j) {
            base[j].cost = flights_ref[j].cost;
            base[j].take_off_time = flights_ref[j].take_off_time;
            base[j].land_time = flights_ref[j].land_time;
            base[j].discount = flights_ref[j].discount;
            base[j].index = flights_ref[j].index;
            base[j].from_hash = flights_ref[j].from_hash;
            base[j].to_hash = flights_ref[j].to_hash;
            base[j].company_hash = flights_ref[j].company_hash;
            base[j].id_hash = flights_ref[j].id_hash;
            base[j].index = j;
        }
    }
 
    return true;
}

/*Cleanup session global contexts*/
void mt_shutdown() {

    delete g_global_permutations;
    delete[] g_thread_context;
    delete[] g_thread_context_res;
    delete[] g_flights;
    delete[]  g_parameters;
 
    g_global_permutations = 0;
    g_flights = 0;
    g_parameters = 0;
    g_thread_context = 0;
    g_thread_context_res = 0;
    g_thread_contexts = 0;

    g_mt_initialized = 0;

    g_alliances.clear();
}
 
void mt_copy_travel(std::vector<override_stl_allocator(travel_t)>* dst,std::vector<override_stl_allocator(travel_t)>* src,
    const uint32_t dst_base,const uint32_t len) {
    const uint32_t thread_count = g_thread_contexts;
    copy_travel_args_t* my_arg;
    std::vector<extent_t> extent;
    uint32_t e;

    if (0 == len) { 
        return;
    }

    calculate_extent(extent,len,thread_count);
    e = extent.size();

    my_arg = new copy_travel_args_t[thread_count];
    assert(my_arg != 0);

    for (uint32_t i = 0;i < e;++i) {
        my_arg[i].src = src;
        my_arg[i].dst = dst;
        my_arg[i].base = dst_base;
        my_arg[i].start = extent[i].s0;
        my_arg[i].end = extent[i].s1;

        if (pthread_create(&g_thread_context[i],NULL,mt_copy_travel_entry_point,(void*)&my_arg[i]) != 0) {
            printf("pthread_create failed!\n"); 
            assert(0);
        }
    }

    mt_wait_threads(e);

    delete[] my_arg;
}
 
void mt_compute_path(const indexed_string_t to,std::vector<override_stl_allocator(travel_t)>& travels,uint64_t t_min,uint64_t t_max) {
    if (travels.empty()) {  //Nothing to do
        return;
    }

    compute_path2_args_t* my_arg;
    const uint32_t thread_count = g_thread_contexts;
    std::vector<extent_t> extent;
    uint32_t e;

    {
        //If we visited again this sequence return its previous result..
        permutation_sequence_t* pseq = g_global_permutations->match(to,travels);
        if (pseq != 0) {
            //printf("Match SEQ %lu %lu %lu\n",travels.size(),pseq->travels.size(),pseq->path.size());
            travels = pseq->path;
            return;
        }
    }

    //Calculate tile size for flights.size()
    calculate_extent(extent,g_flights_size,thread_count);
    e = extent.size();

    //Initialize contexts up to thread_count since sub-tile-size might differ
    my_arg = new compute_path2_args_t[thread_count];
    assert(my_arg != 0);

    for (uint32_t i = 0;i < e;++i) {
        my_arg[i].output = new std::vector<override_stl_allocator(travel_t)>();
        assert(my_arg[i].output != 0);
        my_arg[i].input = new std::vector<override_stl_allocator(travel_t)>();
        assert(my_arg[i].input != 0);
        my_arg[i].final_travels = new std::vector<override_stl_allocator(travel_t)>();
        assert(my_arg[i].final_travels != 0);
        my_arg[i].t_min = t_min;
        my_arg[i].t_max = t_max;
        my_arg[i].max_layover_time = g_parameters[0].max_layover_time;
        my_arg[i].start = extent[i].s0;
        my_arg[i].end = extent[i].s1;
        my_arg[i].to = to;
        my_arg[i].thread_index = i;
        my_arg[i].flight_count = g_flights_size;    
 
    }

    //Partial perm update : Target / Input
    g_global_permutations->cycle(to,travels);

    //Repeat until travel list has no more elements
    
    //uint32_t optimized_paths = 0;
    uint32_t exp = 0;
    while (!travels.empty()) { 
        const uint32_t len = travels.size();

        for (uint32_t j = 0;j < e ;++j) {
            //Start/end offsets in flight list
            my_arg[j].start = extent[j].s0;
            my_arg[j].end = extent[j].s1;
                    
            //Start/end offsets in travels list
            my_arg[j].start2 = 0;
            my_arg[j].end2 = len;
            *my_arg[j].input = travels;

            // printf("%u %u\n",my_arg[j].start2,my_arg[j].end2);
            if (pthread_create(&g_thread_context[j],NULL,mt_compute_path2_entry_point,(void*)&my_arg[j]) != 0) {
                printf("pthread_create failed!\n"); 
                assert(0);
            }
        }

        //Wait for threads to finish their task 
        mt_wait_threads(e);


        //Sum up wanted size and allocate it
        exp = 0;
        for (uint32_t i = 0; i < e;++i) {
            exp += my_arg[i].output->size();
        }

        travels.clear();
        travels.reserve(exp);

        //Append results  
        exp = 0;
        for (uint32_t i = 0; i < e;++i) {
            const uint32_t output_len = my_arg[i].output->size();
            for (uint32_t j = 0;j < output_len;++j) {
                travels.push_back(my_arg[i].output->at(j));
            }
            my_arg[i].output->clear();
        }
    }

    //Cleanup
    for (uint32_t i = 0;i < e;++i) {
        delete my_arg[i].output;
        delete my_arg[i].input;
 
    }
 
    //Sum up wanted size and allocate it
    exp = 0;
    for (uint32_t i = 0; i < e;++i) {
        exp += my_arg[i].final_travels->size();
    }

    travels.clear();
    travels.reserve(exp);

    //Append results  
    exp = 0;
    for (uint32_t i = 0; i < e;++i) {
        const uint32_t output_len = my_arg[i].final_travels->size();
        for (uint32_t j = 0;j < output_len;++j) {
            travels.push_back(my_arg[i].final_travels->at(j));
        }

        delete my_arg[i].final_travels; 
    }

    //if (0 != optimized_paths) {
        //printf("Optimization pass was successful! Total optimized paths %u \n",optimized_paths);
   // }

    //Partial perm update : Output
    g_global_permutations->cycle(travels);

    //Partial perm update : Index
    g_global_permutations->cycle();

    delete[] my_arg;
}
 
 
/*Thread entry point functions implementation*/
/*
    The only difference from the original version is that string comparisons have been replaced by indexes to string list
*/
static inline bool never_traveled_to(flight_ref_t* p_flights,const travel_t& travel,const uint32_t range,const indexed_string_t city) {
    register const std::vector<override_stl_allocator(flight_indice_t)>& flights = travel.flights ;
 
    for(register uint32_t i = 0,j = range; i < j;++i) {
        if ((p_flights[flights[i]].from_hash == city) || (p_flights[flights[i]].to_hash == city)) {
            return false;
        }
    }

    return true;
}

/*  
    The MT version of compute_path.
*/
static void* mt_compute_path2_entry_point(void* in_args) {
    compute_path2_args_t* args = (compute_path2_args_t*)in_args;

    const uint64_t t_min = args->t_min;
    const uint64_t t_max = args->t_max;
    const uint64_t max_layover_time = args->max_layover_time;
    const indexed_string_t to = args->to;
    std::vector<override_stl_allocator(travel_t)>* final_travels = args->final_travels;
    std::vector<override_stl_allocator(travel_t)>* input = args->input;
    std::vector<override_stl_allocator(travel_t)>* output = args->output;

    register flight_ref_t* flights = &g_flights[args->flight_count * args->thread_index];
        
 

    for (register int64_t k = (int64_t)args->end2-1,m = args->start2;k >= m;--k) { 
        register travel_t& travel = input->at(k);
        register const flight_ref_t& current_city = flights[travel.flights.back()];

        if (current_city.to_hash == to) {  
            final_travels->push_back(travel);   
            continue;
        }

        //Save prev len
        const uint32_t travel_size = travel.flights.size();

        //Expand list a bit to write to that element in subloop...
        travel.flights.push_back(0); 

        register flight_indice_t& last_ind = travel.flights[travel_size];

        for (register uint32_t i = args->start,j = args->end;i < j;++i) { // 1 fraction of the flight list
            register const flight_ref_t& flight = flights[i];
 
            if ((flight.from_hash == current_city.to_hash) &&  (flight.take_off_time >= t_min) && 
                (flight.land_time <= t_max) && 
                (flight.take_off_time > current_city.land_time) && 
                ((flight.take_off_time - current_city.land_time) <= max_layover_time) &&  
                never_traveled_to(flights,travel,travel_size,flight.to_hash)   ) {
 
                //Set last element here to flight index
                last_ind = flight.index;

                if (flight.to_hash == to) {
                    final_travels->push_back(travel); 
                } else { 
                    output->push_back(travel); //Push to bucket and handle it in another pass
                }
            } 
        }

        //Restore contents...
        travel.flights.pop_back();
    }
     
    //final_travels->shrink_to_fit();

    pthread_exit(NULL);
    return NULL;
}
 
/*The MT version of fill travel*/
static void* mt_fill_travel_entry_point(void* in_args) {
    fill_travel_args_t* args = (fill_travel_args_t*)in_args;
    uint32_t fcount = args->flight_count;
    register flight_ref_t* flights = &g_flights[fcount * args->thread_index];
    register uint32_t start = args->start;
    register uint32_t end = args->end;
    register const uint64_t t_min = args->t_min;
    register const uint64_t t_max = args->t_max;
    std::vector<override_stl_allocator(travel_t)>* results = args->results;
    const indexed_string_t starting_point = args->starting_point;
    travel_t* t = new travel_t;

    #define fetch(f) {\
        if( (f.from_hash == starting_point) && (f.take_off_time >= t_min) && (f.land_time <= t_max) ){\
            t->flights[0] = f.index;\
            results->push_back(*t);\
        }\
    }

    t->flights.push_back(0);
    results->reserve((fcount >> 8) + 2);

    //Handle blocks of 16 first and give hints to the compiler and cpu's hw prefetcher...
    while ((start + 16) <= end) {
        const flight_ref_t& f0 = flights[start + 0];
        const flight_ref_t& f1 = flights[start + 1];
        const flight_ref_t& f2 = flights[start + 2];
        const flight_ref_t& f3 = flights[start + 3];
        const flight_ref_t& f4 = flights[start + 4];
        const flight_ref_t& f5 = flights[start + 5];
        const flight_ref_t& f6 = flights[start + 6];
        const flight_ref_t& f7 = flights[start + 7];
        const flight_ref_t& f8 = flights[start + 8];
        const flight_ref_t& f9 = flights[start + 9];
        const flight_ref_t& f10 = flights[start + 10];
        const flight_ref_t& f11 = flights[start + 11];
        const flight_ref_t& f12 = flights[start + 12];
        const flight_ref_t& f13 = flights[start + 13];
        const flight_ref_t& f14 = flights[start + 14];
        const flight_ref_t& f15 = flights[start + 15];

        fetch(f0);
        fetch(f1);
        fetch(f2);
        fetch(f3);
        fetch(f4);
        fetch(f5);
        fetch(f6);
        fetch(f7);
        fetch(f8);
        fetch(f9);
        fetch(f10);
        fetch(f11);
        fetch(f12);
        fetch(f13);
        fetch(f14);
        fetch(f15);

        start += 16;
    }

    //Handle remainder
    while (start < end) {
        const flight_ref_t& f = flights[start++];
        fetch(f);
    }
    #undef fetch

    delete t;

    pthread_exit(NULL);
    return NULL;
}

/*The MT version of merge_path*/

#if 0
    #include "asm.h"
#endif

static void* mt_merge_path_entry_point(void* in_args) {
    merge_path_args_t* args = (merge_path_args_t*)in_args;
    register std::vector<override_stl_allocator(travel_t)>* travel1 = args->travel1;
    register std::vector<override_stl_allocator(travel_t)>* travel2 = args->travel2;
    register std::vector<override_stl_allocator(travel_indice_pair_t)>* results = args->results;
    register uint32_t start = args->start;
    register const uint32_t end = args->end;
    register flight_ref_t* flights = &g_flights[args->flight_count * args->thread_index];

    for (;start < end;++start) {
        const travel_t& t1 = travel1->at(start);
        if (t1.flights.empty()) { 
            continue;
        }

        const flight_ref_t& last_flight_t1 = flights[t1.flights.back()];

        //Encode indices to T1/T2 lists
        travel_indice_pair_t pair = (travel_indice_pair_t)start << 32;

        for (register uint32_t j = args->start2,m = args->end2;j < m;++j) {
            const travel_t& t2 = travel2->at(j);
            if (t2.flights.empty()) {
                continue;
            }
            const flight_ref_t& first_flight_t2 = flights[t2.flights[0]];
    
            if (last_flight_t1.land_time < first_flight_t2.take_off_time) {
                results->push_back(pair | (travel_indice_pair_t)j);
            }
        }
    
        //printf("%u %u\n",added,args->end2);
    }
 
     //printf("%lu %lu\n",((results->size()+1)*sizeof(uint64_t))/(1024*1024),((results->capacity()+1)*sizeof(uint64_t))/(1024*1024));
    //results->shrink_to_fit();
 
    pthread_exit(NULL);
    return NULL;
}
 
static bool company_are_in_a_common_alliance(const indexed_string_t in_c1, 
const indexed_string_t in_c2, std::vector<std::vector<indexed_string_t> >& alliances){
    register const indexed_string_t c1 = in_c1,c2 = in_c2;
    for(uint32_t i=0,k = alliances.size(); i<k;++i) {
        uint8_t a = 0,b = 0;
        register const std::vector<indexed_string_t>& v = alliances[i];
        for(register uint32_t j=0,l = v.size(); j<l;++j) {
            const indexed_string_t tmp = v[j];
            a = (tmp == c1) ? 1 : a;
            b = (tmp == c2) ? 1 : b;
        }
        if ((a + b) == 2) {
            return true;
        }
    }
    return false;
}

static inline bool has_just_traveled_with_company(flight_ref_t& flight_before, flight_ref_t& current_flight) {
    return flight_before.company_hash == current_flight.company_hash;
}

static inline bool has_just_traveled_with_alliance(flight_ref_t& flight_before, flight_ref_t& current_flight, 
  std::vector<std::vector<indexed_string_t> >& alliances) {
    return company_are_in_a_common_alliance(current_flight.company_hash,flight_before.company_hash, alliances);
}

static void apply_discount(flight_ref_t* flights,travel_t & travel, std::vector<std::vector<indexed_string_t> >&alliances){
    const uint32_t fsize = travel.flights.size();
    const std::vector<override_stl_allocator(flight_indice_t)>* travel_flights = &travel.flights;
    if(fsize > 0) {
        flights[travel.flights[0]].discount = 1;
    }
    if(fsize > 1) {
        for(register uint32_t i = 1,j = fsize;i < j;++i) {
            flight_ref_t& flight_before = flights[travel_flights->at(i-1)];
            flight_ref_t& current_flight = flights[travel_flights->at(i)];
            if(has_just_traveled_with_company(flight_before, current_flight)){
                flight_before.discount = 0.7;
                current_flight.discount = 0.7;
                continue;
            }else if(has_just_traveled_with_alliance(flight_before, current_flight, alliances)){
                if(flight_before.discount > 0.8) {
                    flight_before.discount = 0.8;
                }
                current_flight.discount = 0.8;
                continue;
            }
            current_flight.discount = 1;
        }
    }
}
 
static inline f32 compute_cost(flight_ref_t* flights,travel_t & travel,std::vector<std::vector<indexed_string_t> >&alliances) {
 
    apply_discount(flights,travel, alliances);
    register f32 result = 0;
    for(register uint32_t i = 0,j = travel.flights.size();i < j;++i){
        result += (flights[travel.flights[i]].cost * flights[travel.flights[i]].discount);
    }
    return result;
}

/*Joins two nodes that relate to each other...*/
static void join_nodes(travel_t& out,const travel_t& in,const uint32_t thread_index) {
    if (in.relation == k_invalid_relation) {
        out = in;
        return;
    } else {
        const uint32_t a0 = (uint32_t)(in.relation >> k_relation_shift);
        const uint32_t a1 = (uint32_t)(in.relation);
        const travel_t& t1 = g_merge_phase_relations->at(thread_index).nodes[in.node].a[a0];
        const travel_t& t2 = g_merge_phase_relations->at(thread_index).nodes[in.node].b[a1];

        const uint32_t tsize = t1.flights.size();
        const uint32_t hsize = t2.flights.size();
       // printf("(N%u) t %u(%lu) h %u(%lu)\n",in.node,tsize,g_merge_phase_relations->at(thread_index).nodes[in.node].a.size(),
       // hsize,g_merge_phase_relations->at(thread_index).nodes[in.node].b.size());
        out.flights.clear();
        out.flights.reserve(tsize + hsize);
        out.relation = in.relation;
        out.node = in.node;

        for (uint32_t i = 0;i < tsize;++i) {    
            out.flights.push_back(t1.flights[i]);
        }

        for (uint32_t i = 0;i < hsize;++i) {    
            out.flights.push_back(t2.flights[i]);
        }
    }
}

/*The MT version of find_cheapest*/
static void* mt_find_cheapest_entry_point(void* in_args) {
    find_cheapest_args_t* args = (find_cheapest_args_t*)in_args;
 
    register int64_t start = args->start;
    register int64_t end = args->end;
    register f32 best_cost,curr_cost;
    register uint32_t best_ind;
    register std::vector<override_stl_allocator(travel_t)>* travels = args->travels;
    std::vector<std::vector<indexed_string_t>>& alliances = g_alliances[args->thread_index].alliances;
    flight_ref_t* flights = &g_flights[args->flight_count * args->thread_index];

    travel_t* tmp = new travel_t;

    //Get initial cost
    end -= end > start; //Won't happen
    best_ind = (uint32_t)end--;

    join_nodes(*tmp,travels->at(best_ind),args->thread_index);
    best_cost = compute_cost(flights,*tmp,alliances);

    while (end >= start) {

        //Get current cost
        join_nodes(*tmp,travels->at(end),args->thread_index);
        curr_cost = compute_cost(flights,*tmp,alliances);

        //Keep track of new records
        if (curr_cost < best_cost) {
            best_ind = (uint32_t)end;
            best_cost = curr_cost;
        }

        --end;
    }

    //Return result
    args->best_cost = best_cost;
    args->best_ind = best_ind;

    join_nodes(*tmp,travels->at(best_ind),args->thread_index);
    *args->out_travel = *tmp;
    delete tmp;
 
    pthread_exit(NULL);
    return NULL;
}
 
/*Swaps contents in src with dst*/
static void* mt_copy_travel_entry_point(void* in_args) {
    copy_travel_args_t* args = (copy_travel_args_t*)in_args;
    register uint32_t start = args->start,end = args->end,base = args->base;
    register std::vector<override_stl_allocator(travel_t)>* src = args->src;
    register std::vector<override_stl_allocator(travel_t)>* dst = args->dst;

    for (;start < end;++start) {
        dst->at(base + start)  = src->at(start);
    }

    pthread_exit(NULL);
    return NULL;
}


