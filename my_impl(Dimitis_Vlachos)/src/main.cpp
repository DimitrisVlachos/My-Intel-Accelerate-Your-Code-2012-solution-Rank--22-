/*
    --file main.cpp
    --brief : This file contains source code that solves the Work Hard - Play Hard problem for the Acceler8 contest
    
    Original code : Intel Software
    Optimized multithreaded version : Dimitris Vlachos ( dimitrisv22@gmail.com )
*/
#include "base.hpp"
#include "mt.hpp"
#include "profiling.hpp"

using namespace std;

time_t convert_to_timestamp(int32_t day,int32_t month,int32_t year,int32_t hour,int32_t minute,int32_t seconde);

time_t convert_string_to_timestamp(const string& s);
void print_params(Parameters &parameters);
void print_flight(flight_indice_t ref_ind,ofstream& output);
void read_parameters(Parameters& parameters, int32_t argc, char **argv);
void split_string(vector<string>& result, string line, char separator);
void parse_flights(Parameters& params,vector<flight_ref_t>& flights_ref);
void parse_alliance(vector<string> &alliance, string line);
void parse_alliances(vector<vector<indexed_string_t> > &alliances, string filename);
bool company_are_in_a_common_alliance(const string& c1, const string& c2, vector<vector<indexed_string_t> >& alliances);
bool has_just_traveled_with_company(flight_ref_t& flight_before, flight_ref_t& current_flight);
bool has_just_traveled_with_alliance(flight_ref_t& flight_before, flight_ref_t& current_flight, vector<vector<indexed_string_t> >& alliances);
static void apply_discount(travel_t & travel, vector<vector<indexed_string_t> >&alliances);
static f32 compute_cost(travel_t & travel, vector<vector<indexed_string_t> >&alliances);
void print_alliances(vector<vector<indexed_string_t> > &alliances);
void print_flights(std::vector<override_stl_allocator(flight_indice_t)>& flights,ofstream& output);
void print_travel(travel_t& travel, vector<vector<indexed_string_t> >&alliances, ofstream& output);

travel_t work_hard(Parameters& parameters, vector<vector<indexed_string_t> >& alliances);
vector<override_stl_allocator(travel_t)> play_hard( Parameters& parameters, vector<vector<indexed_string_t> >& alliances);
void output_play_hard(Parameters& parameters, vector<vector<indexed_string_t> >& alliances);
void output_work_hard(Parameters& parameters, vector<vector<indexed_string_t> >& alliances);
time_t timegm(struct tm *tm);

void env_shutdown(); /*For optimized time_gm*/
void env_init(); /*For optimized time_gm*/

char* g_timezone = 0; /*For optimized time_gm*/

static travel_t find_cheapest(vector<override_stl_allocator(travel_t)>& travels, vector<vector<indexed_string_t> >&alliances){
    travel_t result;
    profiler_profile_me();
    mt_find_cheapest(result,travels,alliances);
    
    return result;//compiler's RVO opt pass does its job
}

static void compute_path(const indexed_string_t to,vector<override_stl_allocator(travel_t)>& travels, uint64_t t_min, uint64_t t_max, Parameters parameters) {
    profiler_profile_me();
    mt_compute_path(to,travels,t_min,t_max);
}

static void fill_travel(vector<override_stl_allocator(travel_t)>& travels,indexed_string_t starting_point, uint64_t t_min, uint64_t t_max) {
    profiler_profile_me();
    mt_fill_travel(travels,starting_point,t_min,t_max);
}

static void merge_path(vector<override_stl_allocator(travel_t)>& travel1, vector<override_stl_allocator(travel_t)>& travel2,const travel_indice_t relation = k_invalid_relation,const travel_indice_t node = k_node_zero) {
    profiler_profile_me();
    mt_merge_path(travel1,travel2,relation,node);
}

travel_t work_hard(Parameters& parameters, vector<vector<indexed_string_t> >& alliances) {
    vector<override_stl_allocator(travel_t)> travels;

    //First, we need to create as much travels as it as the number of flights that take off from the
    //first city
    fill_travel(travels,parameters.from, parameters.dep_time_min, parameters.dep_time_max);
    compute_path(parameters.to, travels, parameters.dep_time_min, parameters.dep_time_max, parameters);
    vector<override_stl_allocator(travel_t)> travels_back;

    //Then we need to travel back
    fill_travel(travels_back,parameters.to, parameters.ar_time_min, parameters.ar_time_max);
    compute_path(parameters.from, travels_back, parameters.ar_time_min, parameters.ar_time_max, parameters);
    merge_path(travels, travels_back);

    return find_cheapest(travels, alliances);
}
 
vector<override_stl_allocator(travel_t)> play_hard(Parameters& parameters, vector<vector<indexed_string_t> >& alliances) {
    vector<override_stl_allocator(travel_t)> results;
    list<indexed_string_t>::iterator it = parameters.airports_of_interest.begin();

    for (; it != parameters.airports_of_interest.end(); it++) {
    mt_init_merge_phase_relations();
        

        const indexed_string_t current_airport_of_interest = *it;
        vector<override_stl_allocator(travel_t)>* all_travels;
        /*
         * The first part compute a travel from home -> vacation -> conference -> home
         */
        vector<override_stl_allocator(travel_t)>* home_to_vacation = new vector<override_stl_allocator(travel_t)>();
        vector<override_stl_allocator(travel_t)>* vacation_to_conference = new vector<override_stl_allocator(travel_t)>();
        vector<override_stl_allocator(travel_t)>* conference_to_home = new vector<override_stl_allocator(travel_t)>();

        //compute the paths from home to vacation
        fill_travel(*home_to_vacation, parameters.from, parameters.dep_time_min-parameters.vacation_time_max, parameters.dep_time_min-parameters.vacation_time_min);
        compute_path(current_airport_of_interest,*home_to_vacation, parameters.dep_time_min-parameters.vacation_time_max, parameters.dep_time_min-parameters.vacation_time_min, parameters);
        //compute the paths from vacation to conference
        fill_travel(*vacation_to_conference,current_airport_of_interest, parameters.dep_time_min, parameters.dep_time_max);
        compute_path(parameters.to,*vacation_to_conference, parameters.dep_time_min, parameters.dep_time_max, parameters);
        //compute the paths from conference to home
        fill_travel(*conference_to_home, parameters.to, parameters.ar_time_min, parameters.ar_time_max);
        compute_path( parameters.from,*conference_to_home, parameters.ar_time_min, parameters.ar_time_max, parameters);
     
        merge_path(*home_to_vacation,*vacation_to_conference);
        delete vacation_to_conference;  
 
 
        mt_merge_phase_link_node(*home_to_vacation,*conference_to_home,k_node_zero);

        merge_path(*home_to_vacation,*conference_to_home,k_mate,k_node_zero);
        delete  conference_to_home; 

        all_travels = home_to_vacation;
 
        /*
         * The second part compute a travel from home -> conference -> vacation -> home
         */
        vector<override_stl_allocator(travel_t)>* home_to_conference = new vector<override_stl_allocator(travel_t)>();
        vector<override_stl_allocator(travel_t)>* conference_to_vacation = new vector<override_stl_allocator(travel_t)>();
        vector<override_stl_allocator(travel_t)>* vacation_to_home = new vector<override_stl_allocator(travel_t)>();

        //compute the paths from home to conference
        fill_travel(*home_to_conference, parameters.from, parameters.dep_time_min, parameters.dep_time_max);
        compute_path(parameters.to,*home_to_conference, parameters.dep_time_min, parameters.dep_time_max, parameters);
        //compute the paths from conference to vacation
        fill_travel(*conference_to_vacation,parameters.to, parameters.ar_time_min, parameters.ar_time_max);
        compute_path( current_airport_of_interest,*conference_to_vacation, parameters.ar_time_min, parameters.ar_time_max, parameters);
        //compute paths from vacation to home
        fill_travel(*vacation_to_home,current_airport_of_interest, parameters.ar_time_max+parameters.vacation_time_min, parameters.ar_time_max+parameters.vacation_time_max);
        compute_path( parameters.from,*vacation_to_home, parameters.ar_time_max+parameters.vacation_time_min, parameters.ar_time_max+parameters.vacation_time_max, parameters);


        merge_path(*home_to_conference,*conference_to_vacation);
        delete conference_to_vacation; 

        mt_merge_phase_link_node(*home_to_conference,*vacation_to_home,k_node_one);

        merge_path(*home_to_conference,*vacation_to_home,k_mate,k_node_one);
        delete vacation_to_home;

        const uint32_t hsize = home_to_conference->size();
        for (register uint32_t i = 0, j = hsize;i < j;++i) {
            all_travels->push_back(home_to_conference->at(i));
        }

        delete home_to_conference;

        results.push_back(find_cheapest(*all_travels,alliances));
        delete all_travels;

        mt_shutdown_merge_phase_relations();


    }
 
    return results;
}

void split_string(vector<string>& result,string line, char separator){
    while(line.find(separator) != string::npos){
        size_t pos = line.find(separator);
        result.push_back(line.substr(0, pos));
        line = line.substr(pos+1);
    }
    result.push_back(line);
}

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
        return flight_class_d; //Add flight to the global list
    }

    return flight_class_invalid; //Skip flight
}


void parse_flights(Parameters& params,vector<flight_ref_t>& flights_ref) {
//  profiler_profile_me();
    ifstream file;
    file.open(params.flights_file.c_str());
    if(!file.is_open()){
        cerr<<"Problem while opening the file "<<params.flights_file<<endl;
        exit(0);
    }
 
    //Patch ;,\r\n with '\0'
    register char* code;
    register uint32_t len,head,start,q_len,index,actual_index,lines;
 
    flight_ref_t ref;
    file.seekg(0,ios::end);
    len = file.tellg();
    file.seekg(0,ios::beg);

    code = new char[len];
    assert(code != 0);
    file.read(code,len);
 

    //Count lines
    head = 0;
    lines = 0;
    while (head < len) {
        const char c = code[head++];
        if ((c == '\r') || (c == '\n') || (head == len)) {
            ++lines;
        }
    }

    //alloc+parse
    flights_ref.reserve(lines+2);
    q_len = 0;
    index = actual_index = 0;
    head = 0;
    start = 0;

    while (head < len) {
        const char c = code[head++];
        if (c == ';') {
            code[head - 1] = '\0'; // PATCH
            ++q_len;
            continue;
        } else if ((c == '\r') || (c == '\n') || (head == len)) {
            code[head - 1] = '\0'; // PATCH

            if (++q_len == 7) {         
                const char* p = (const char*)(code + start);
                const char* id_p,*from_p,*to_p,*company_p;
                id_p = p; p += strlen(p) + 1;
                from_p = p; p += strlen(p) + 1;
                ref.take_off_time = convert_string_to_timestamp(p);   p += strlen(p) + 1;
                to_p = p; p += strlen(p) + 1;
                ref.land_time = convert_string_to_timestamp(p);   p += strlen(p) + 1;

                const flight_class_t fclass = classify_flight(params,ref.take_off_time,ref.land_time);
                if (fclass != flight_class_invalid) {
                    ref.cost = atof(p); p += strlen(p) + 1;
                    company_p = p;

                    ref.id_hash = ss_register(std::string(id_p));
                    ref.to_hash = ss_register(std::string(to_p));
                    ref.from_hash = ss_register(std::string(from_p));
                    ref.company_hash = ss_register(std::string(company_p));
                    ref.index = index++;
                    flights_ref.push_back(ref);
                }
                ++actual_index;
            }

            while ((head < len) && isspace(code[head])) {
                ++head;
            }
            start = head;
            q_len = 0;
            continue;
        }
    }

    printf("Parse Flights : Reduced initial input from %u to %u nodes\n",actual_index,index);
    delete[] code;
}

void output_play_hard(Parameters& parameters, vector<vector<indexed_string_t> >& alliances){
    ofstream output;
    output.open(parameters.play_hard_file.c_str());
    vector<override_stl_allocator(travel_t)> travels = play_hard(parameters, alliances);
    list<indexed_string_t> cities = parameters.airports_of_interest;
    for(uint32_t i=0; i<travels.size(); i++){
        output<<"“Play Hard” Proposition "<<(i+1)<<" : "<<ss_resolve(cities.front())<<endl;
        print_travel(travels[i], alliances, output);
        cities.pop_front();
        output<<endl;
    }
    output.close();
}

void output_work_hard(Parameters& parameters, vector<vector<indexed_string_t> >& alliances){
    ofstream output;
    output.open(parameters.work_hard_file.c_str());
    travel_t travel = work_hard(parameters, alliances);
    output<<"“Work Hard” Proposition :"<<endl;
    print_travel(travel, alliances, output);
    output.close();
}

void env_init() {
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
    g_timezone = 0;
}

time_t timegm(struct tm *tm) {
#if 0
        //original code
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
#else
    return mktime(tm); //my simplified version
#endif
}

int main(int argc, char **argv) {
  
    Parameters parameters;
    vector<vector<indexed_string_t>> alliances;


    printf("Intializing contexts...\n");
    profiler_init(parameters.b_silent);
    env_init();
    {
        vector<flight_ref_t> flights_ref; 

        profiler_profile_me_ex("init_contexts");
        if (!ss_init()) {
            printf("ss_init failed\n");
            assert(0);
        }

        //Read params
        read_parameters(parameters, argc, argv);    

        mt_init(parameters);

        //Parse flights
        parse_flights(parameters,flights_ref);

        //Parse alliances
        parse_alliances(alliances, parameters.alliances_file);

        //Initialize multi-thread ops
        mt_set_work_data(flights_ref,alliances);
    }

    printf("Intializing contexts...OK\n");

    {
        printf("Solving...\n");
        printf("Solving...[PLAY HARD]\n");
        output_play_hard(parameters,alliances);
        printf("Solving...[WORK HARD]\n");
        output_work_hard(parameters,alliances);
        printf("Solving...OK\n");
    }

    profiler_shutdown();
    mt_shutdown();
    ss_shutdown();
    env_shutdown();
    return 0;
}

/*All the work is done in mt.cpp but these are kept just to output the final results...*/
bool company_are_in_a_common_alliance(const indexed_string_t c1, const indexed_string_t c2, vector<vector<indexed_string_t> >& alliances){
    bool result = false;
    for(unsigned int i=0; i<alliances.size(); i++){
        bool c1_found = false, c2_found = false;
        for(unsigned int j=0; j<alliances[i].size(); j++){
            if(alliances[i][j] == c1) c1_found = true;
            if(alliances[i][j] == c2) c2_found = true;
        }
        result |= (c1_found && c2_found);
    }
    return result;
}

bool has_just_traveled_with_company(flight_ref_t& flight_before, flight_ref_t& current_flight){
    return flight_before.company_hash == current_flight.company_hash;
}

bool has_just_traveled_with_alliance(flight_ref_t& flight_before, flight_ref_t& current_flight, vector<vector<indexed_string_t> >& alliances){
    return company_are_in_a_common_alliance(current_flight.company_hash,flight_before.company_hash, alliances);
}


void parse_alliance(vector<indexed_string_t> &alliance, string line){
    vector<string> splittedLine;
    split_string(splittedLine, line, ';');
    for(uint32_t i=0; i<splittedLine.size(); i++){
        alliance.push_back(ss_register(splittedLine[i]));
    }
}

void parse_alliances(vector<vector<indexed_string_t> > &alliances, string filename){
    string line = "";
    ifstream file;

    file.open(filename.c_str());
    if(!file.is_open()){
        cerr<<"Problem while opening the file "<<filename<<endl;
        exit(0);
    }
    alliances.reserve(256);
    while (!file.eof())
    {
        vector<indexed_string_t> alliance;
        alliance.reserve(16);
        getline(file, line);
        parse_alliance(alliance, line);
        alliances.push_back(alliance);
    }
}

void print_alliances(vector<vector<indexed_string_t> > &alliances){
    for(uint32_t i=0; i<alliances.size(); i++){
        cout<<"Alliance "<<i<<" : ";
        for(uint32_t j=0; j<alliances[i].size(); j++){
            cout<<"**"<<ss_resolve(alliances[i][j])<<"**; ";
        }
        cout<<endl;
    }
}

void print_flights(std::vector<override_stl_allocator(flight_indice_t)>& flights,ofstream& output) {    
 
    for(uint32_t i=0; i<flights.size(); i++) {
        print_flight(flights[i],output);
    }
}
 

void print_travel(travel_t& travel, vector<vector<indexed_string_t> >&alliances, ofstream& output){
    output<<"Price : "<<compute_cost(travel, alliances)<<endl;
    print_flights(travel.flights, output);
    output<<endl;
}

static void apply_discount(travel_t & travel, vector<vector<indexed_string_t> >&alliances){
    flight_ref_t* flights;
    uint32_t count;
    mt_get_flights(flights,count);
    if(travel.flights.size()>0)
        flights[travel.flights[0] ].discount = 1;
    if(travel.flights.size()>1) {
        for(uint32_t i=1; i<travel.flights.size(); i++){
            flight_ref_t& flight_before = flights[travel.flights[i-1]];
            flight_ref_t& current_flight = flights[travel.flights[i]];
            if(has_just_traveled_with_company(flight_before, current_flight)){
                flight_before.discount = 0.7;
                current_flight.discount = 0.7;
            }else if(has_just_traveled_with_alliance(flight_before, current_flight, alliances)){
                if(flight_before.discount >0.8)
                    flight_before.discount = 0.8;
                current_flight.discount = 0.8;
            }else{
                current_flight.discount = 1;
            }
        }
    }
}


static f32 compute_cost(travel_t & travel, vector<vector<indexed_string_t> >&alliances){
    f32 result = 0;
    flight_ref_t* flights;
    uint32_t count;
    if (travel.flights.empty()) {
        return (f32)std::numeric_limits<f32>::max() ; 
    }
    apply_discount(travel, alliances);

    mt_get_flights(flights,count);

    for(unsigned int i=0; i<travel.flights.size(); i++){
        result += (flights[travel.flights[i]].cost * flights[travel.flights[i]].discount);
    }
    return result;
}

time_t convert_to_timestamp(int32_t day,int32_t month,int32_t year,int32_t hour,int32_t minute,int32_t seconde){
    tm time;
    time.tm_year = year - 1900;
    time.tm_mon = month - 1;
    time.tm_mday = day;
    time.tm_hour = hour;
    time.tm_min = minute;
    time.tm_sec = seconde;
    return timegm(&time);
}

time_t convert_string_to_timestamp(const string& s) {
    if(s.size() != 14){
        cerr<<"The given string is not a valid timestamp"<<endl;
        exit(0);
    }else{
        int32_t day, month, year, hour, minute, seconde;
        day = atoi(s.substr(2,2).c_str());
        month = atoi(s.substr(0,2).c_str());
        year = atoi(s.substr(4,4).c_str());
        hour = atoi(s.substr(8,2).c_str());
        minute = atoi(s.substr(10,2).c_str());
        seconde = atoi(s.substr(12,2).c_str());
        return convert_to_timestamp(day, month, year, hour, minute, seconde);
    }
}

void print_params(Parameters &parameters){
    cout<<"From : "                 <<parameters.from                   <<endl;
    cout<<"To : "                   <<parameters.to                     <<endl;
    cout<<"dep_time_min : "         <<parameters.dep_time_min           <<endl;
    cout<<"dep_time_max : "         <<parameters.dep_time_max           <<endl;
    cout<<"ar_time_min : "          <<parameters.ar_time_min            <<endl;
    cout<<"ar_time_max : "          <<parameters.ar_time_max            <<endl;
    cout<<"max_layover_time : "     <<parameters.max_layover_time       <<endl;
    cout<<"vacation_time_min : "    <<parameters.vacation_time_min      <<endl;
    cout<<"vacation_time_max : "    <<parameters.vacation_time_max      <<endl;
    cout<<"flights_file : "         <<parameters.flights_file           <<endl;
    cout<<"alliances_file : "       <<parameters.alliances_file         <<endl;
    cout<<"work_hard_file : "       <<parameters.work_hard_file         <<endl;
    cout<<"play_hard_file : "       <<parameters.play_hard_file         <<endl;
    list<indexed_string_t>::iterator it = parameters.airports_of_interest.begin();
    for(; it != parameters.airports_of_interest.end(); it++)
        cout<<"airports_of_interest : " <<ss_resolve(*it)   <<endl;
    cout<<"flights : "              <<parameters.flights_file           <<endl;
    cout<<"alliances : "            <<parameters.alliances_file         <<endl;
    cout<<"nb_threads : "           <<parameters.nb_threads             <<endl;
}

void print_flight(flight_indice_t ref_ind,ofstream& output) {
    struct tm * take_off_t, *land_t;
    flight_ref_t* flights;
    uint32_t count;

    mt_get_flights(flights,count);
    const flight_ref_t& ref = flights[ref_ind];

    take_off_t = gmtime(((const time_t*)&(ref.take_off_time)));
    output<<ss_resolve(ref.company_hash)<<"-";
    output<<""<<ss_resolve(ref.id_hash)<<"-";
    output<<ss_resolve(ref.from_hash)<<" ("<<(take_off_t->tm_mon+1)<<"/"<<take_off_t->tm_mday<<" "<<take_off_t->tm_hour<<"h"<<take_off_t->tm_min<<"min"<<")"<<"/";
    land_t = gmtime(((const time_t*)&(ref.land_time)));
    output<<ss_resolve(ref.to_hash)<<" ("<<(land_t->tm_mon+1)<<"/"<<land_t->tm_mday<<" "<<land_t->tm_hour<<"h"<<land_t->tm_min<<"min"<<")-";
    output<<ref.cost<<"$"<<"-"<<ref.discount*100<<"%"<<endl;

}

void read_parameters(Parameters& parameters, int32_t argc, char **argv){
    parameters.b_silent = 0;
    parameters.perm_size = 32;
    parameters.merge_buffer_thresold = 128*1024; //Merge up to 128K travels/Pass

    //parameters.s_method = (int32_t)s_method_2;

    for(int32_t i=0; i<argc; i++){
        const string current_parameter = argv[i];
        if(current_parameter == "-from"){
            parameters.from = ss_register(argv[++i]);
        }else if(current_parameter == "-arrival_time_min"){
            parameters.ar_time_min = convert_string_to_timestamp(argv[++i]);
        }else if(current_parameter == "-arrival_time_max"){
            parameters.ar_time_max = convert_string_to_timestamp(argv[++i]);
        }else if(current_parameter == "-to"){
            parameters.to = ss_register(argv[++i]);
        }else if(current_parameter == "-departure_time_min"){
            parameters.dep_time_min = convert_string_to_timestamp(argv[++i]);
        }else if(current_parameter == "-departure_time_max"){
            parameters.dep_time_max = convert_string_to_timestamp(argv[++i]);
        }else if(current_parameter == "-max_layover"){
            parameters.max_layover_time = atol(argv[++i]);
        }else if(current_parameter == "-vacation_time_min"){
            parameters.vacation_time_min = atol(argv[++i]);
        }else if(current_parameter == "-vacation_time_max"){
            parameters.vacation_time_max = atol(argv[++i]);
        }else if(current_parameter == "-vacation_airports"){
            while(((i+1) < argc) && (argv[i+1][0] != '-')){
                parameters.airports_of_interest.push_back(ss_register(argv[++i]));
            }
        }else if(current_parameter == "-flights"){
            parameters.flights_file = argv[++i];
        }else if(current_parameter == "-alliances"){
            parameters.alliances_file = argv[++i];
        }else if(current_parameter == "-work_hard_file"){
            parameters.work_hard_file = argv[++i];
        }else if(current_parameter == "-play_hard_file"){
            parameters.play_hard_file = argv[++i];
        }else if(current_parameter == "-nb_threads"){
            parameters.nb_threads = atoi(argv[++i]);
        }else if(current_parameter == "-b_silent"){
            parameters.b_silent = 1;
        }else if(current_parameter == "-perm_size"){
            parameters.perm_size = (int32_t)atol(argv[++i]);
        }else if(current_parameter == "-merge_buf_thresold"){
            parameters.merge_buffer_thresold = (int32_t)atol(argv[++i]);
            printf("Setting merge buffer thresold to %u\n",parameters.merge_buffer_thresold);
        }
    }
 
}

