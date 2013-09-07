My-Intel-Accelerate-Your-Code-2012-solution-Rank--22-
=====================================================


============Team info============
Author : Dimitris Vlachos ( dimitrisv22@gmail.com )
Age : 24
Location : Athens,Greece
Team members : 1 (one)



=====Implementation  details=====
Before i begin writing the in-depth explanation of my solution , i would like to mention that my MT implementation is heavily based 
on the original algorithm that was provided....or in other words : i've optimized the original logic and splitted all tasks to multiple worker threads.A much more efficient approach that could reduce the polynomial complexity of the algorithm would be to implement a graph/path-finding algorithm.
    

===========================================================================================

[Source file description]
main.cpp            : Application entry point & Top layer of all computations
mt.cpp              : Multithreading operations
permutations.cpp    : Path permutations matcher
util.cpp            : Utility code
static_strings.cpp  : Static string tree for string index codes
profiling.cpp       : A basic scoped profiler
io.c/hpp            : I/O operations

===========================================================================================


[Data Structures]
One of the first things that i noticed while studying the original code was its "problematic" data structures : 
While they where fine for prototyping , they would cause serious slowdowns(stalls) during an MT computation session since the allocator would have to expand ,copy blocks and call c/dtors all over the place.

Particularly , this is the original "Flight" structure for reference :
struct Flight{ //1constructor / 1destructor call / 1object copy
    string id;  //1constructor / 1destructor call / 1object copy
    string from; //1constructor / 1destructor call / 1object copy
    string to; //1constructor / 1destructor call / 1object copy
    unsigned long take_off_time; 
    unsigned long land_time; 
    string company; //1constructor / 1destructor call / 1object copy
    float cost; 
    float discout; 
};

As you may have noticed already that object during a single session will do 4 contructor calls , 4 destructor calls , 4 copies + allocations which is guaranteed to mess our timings hence it would be a top priority to replace it with something "else"....And that "else" , could may as well be this:

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

So what is "this" ? And what do we get ?

0.A C POD-Like defined chunk of memory aligned to some power of 2(2^6 -> which "happens" to equal the size of cpu's cache line) , and without any traffic from c/dtors taking place

1.The strings have been replaced with that magic type called "indexed_string_t" which is just a typedef'd uint32_t that represents the index(or hash) of the string in a static string tree(more on that later).

2.Eliminated all string comparisons with just a long comparison

3.Improved cpu's HW prefetching

Now,all these sound great so far , but they would still cause -some- stalls when running low on memory or when adding massively data to contexts across all threads at the same time , so there's only one thing left to do : Replace all object copies with just indices to a "static"(allocated once) array of flight_ref_t's to get the best possible bandwidth!

===========================================================================================



[Static string tree (@static_strings.cpp)]


Since i had to eliminate string comparisons , and i also couldn't trust a hashing algorithm because soon or later
it would introduce collisions , i came up with this simple concept : References to a string table.

You could think of it as a series of elements(or parent nodes) that contain a sub field(children) that consists a list of strings.
For example , if we assume that we have the following input :

ABCD,PAOSM,BVASR,BBBB,BB,APRQQQ,AFFEE,QQQ,QQQ,QQ,QQQQ,QQQQ,QQQQ,QQQQ,QQQQ,QQQQ,AAAA,AA,AA,AA,AA,AAAA

We get a tree with the following elements :

    {AAA
    {A
    {BCD        
A-> {FFEE
    {PRQQQ
    
    {VASR
B-> {BB
    {B

    {
P-> {AOSM
    {
    
    {QQ
Q-> {Q
    {QQQ

And now we to encode a reference we use the following formula (Assumming that we have 32bits available for encoding):


Let Parent be uint32_t 
Let Child be uint32_t

Parent = (input.length > 1) ? (input.symbol(0) << 16) | (input.symbol(input.length >> 1) << 8)  : (input.symbol(0) << 8)
Child = Parent.list.index_of(input)
Final code would be : (Parent & ((1 << 16)-1) | Child & ((1 << 16)-1))

And that was it..

Note : The static tree after it has been built , it will be only accessed at the end of the computation just to print the final results.All the comparisons are performed on the -actual- code indexes.

===========================================================================================

[Permutations ring buffer (permutations.cpp)]

The permutations ring buffer is simply keeping a record of N(=variable) travels and paths that have been visited in previous cycles
 which is then used by my_compute_path() to either expand its records , or if a match was found , just return its already computed path.

Currently the ring buffer reserves by default room for 32 entries , but feel free to expand its size by just passing the argument 
-perm_size N (where N is the wanted size...) via command line/script....


===========================================================================================
[Algorithms : compute_path()  (mt.cpp) ]
The method that is used by mt.cpp is the following :
    Approx Its/Worker: (Flight count / Thread Count) * Travel Count 
    
    0.Split flight count by N(=threads)
    1.Create N-1 copies of travel list (N*N Tiling doesn't make any difference)
    2.Run threads
    3.Get outputs from threads
    4.Combine all outputs
    5.If the list with the combined outputs contains elements jump to #1
    6.Merge final travels
    7.Return merged travels
 
===========================================================================================


[Algorithms : find_cheapest() (mt.cpp) ]
The method that is used by mt.cpp is the following :

Approx Its/Worker : Elements in Input Travel / Thread count

    0.Create an extent of travel list for each thread
    1.Each thread does only 1 call per block length to compute_cost and returns the best travel 
    2.When all threads have finished their task find the lowest cost
    3.Finally,only the main thread does the actual object copy of the element and returns it as a result


===========================================================================================

[Algorithms : merge_path() (mt.cpp) ]
The method that is used by mt.cpp is the following :

Approx Its/Worker : Input travel1 size / Thread count

    0.Create N-1 copies of travel2 list
    1.Create an extent of N threads based on travel1 size
    2.Start threads
    3.Each thread stores 64bit indices as partial results
    4.Main thread maps/merges results this way :
       4a.If we can combine 2 travel fields with just a reference to a previous path then store the 64bit pair and mark
        travel_t list as a node in link merge buffer
       4b.Otherwise just construct a complete unified array of travel1/travel2
    5.Return results


===========================================================================================


[Algorithms : fill_travel() (mt.cpp) ]
The method that is used by mt.cpp is the following :

Approx Its/Worker : Flight count / Thread count

    0.Create an extent of travel list for each thread
    1.Each thread filters only a fraction of the list
    2.Each thread stores partial results
    3.Main thread merges results and returns them


===========================================================================================


[Algorithms : Reducing the initial flights input list]

To reduce the initial input im simply checking if the delta time of the flight exceeds any of the conditions stated by play_hard and work_hard functions.

For reference here is the actual flight classification function that is used during flights.txt parsing :

static inline flight_class_t classify_flight(const Parameters& params,const uint64_t& take_off_time,const uint64_t& land_time) {

    if ((take_off_time >= params.dep_time_min) && (land_time <= params.dep_time_max)) {
        return flight_class_a; //Add flight to the global list
    } else if ((take_off_time >= params.ar_time_min) && (land_time <= params.ar_time_max)) {
        return flight_class_b; //Add flight to the global list
    } else if ((take_off_time >= (params.ar_time_max+params.vacation_time_min)) && 
            (land_time <= (params.ar_time_max+params.vacation_time_max))) {
        return flight_class_c; //Add flight to the global list
    } else if ((take_off_time >= (params.dep_time_min-params.vacation_time_max)) && 
            (land_time <= (params.dep_time_min-params.vacation_time_min))) {
        return flight_class_d;  //Add flight to the global list
    }

    return flight_class_invalid; //Skip flight
}



===========================================================================================


[Not so obvious optimizations]

Here i'll list a few not-so-obvious optimizations that i've gone through....:

#0 : 
    The flights.txt parser had to be re-written from scratch since it was really slow..

#1 : 
    timegm() was causing a huge bottloneck to initialization phase, so i replaced this :

time_t timegm(struct tm *tm){
       time_t ret;
       char *tz;

       tz = getenv("TZ");
       setenv("TZ", "", 1);
       tzset();
       ret = mktime(tm);
       if (tz)
           setenv("TZ", tz, 1);
       else
           unsetenv("TZ");
       tzset();
       return ret;
}

With the following :

void env_init() { //Init ONCE
    g_timezone = getenv("TZ");
    setenv("TZ", "", 1);
    tzset();
}

void env_shutdown() {
    if (g_timezone) {
        setenv("TZ",g_timezone, 1);
    } else {
        unsetenv("TZ");
    }
    tzset();
    g_timezone = 0;
}

time_t timegm(struct tm *tm) {
    return mktime(tm); //my simplified version
}

Which doesn't require enviroment variable requests for each call...


===========================================================================================


[Closing comments]
Well that was all!
I would just like to thank you for the challenge and for giving me the chance to participate and have some fun :) !
 
