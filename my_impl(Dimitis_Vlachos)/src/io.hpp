#ifndef __io_hpp__
#define __io_hpp__
/*
    io module : Handles all IO operations
    Author : Dimitris Vlachos ( dimitrisv22@gmail.com )
*/
#include "base.hpp"
#include <fcntl.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>

//TODO MMAPPED stream reader ?

class streamed_travel_list_c {
    private:

    public:

    streamed_travel_list_c() {}
    ~streamed_travel_list_c() {  }

    virtual boolean_t init(const std::vector<override_stl_allocator(travel_t)>& travels,const std::string& fpath,
                        const uint32_t bulk_size) { return false; }
    virtual boolean_t next_bulk(std::vector<override_stl_allocator(travel_t)>& res) { return false; }
    virtual void shutdown() { }
};

class streamed_travel_list_writer_c: public streamed_travel_list_c {
    private:
    inline uint64_t flush_stream(FILE* strm,const flight_indice_t* data,uint32_t& ptr);
    inline void write(FILE* strm,flight_indice_t* data,uint32_t& ptr,const uint32_t len,const flight_indice_t arg);

    public:
    streamed_travel_list_writer_c() {}
    streamed_travel_list_writer_c(const std::vector<override_stl_allocator(travel_t)>& travels,
                                  const std::string& fpath,const uint32_t bulk_size) { this->init(travels,fpath,bulk_size); }
    ~streamed_travel_list_writer_c() { this->shutdown(); }
    boolean_t init(const std::vector<override_stl_allocator(travel_t)>& travels,const std::string& fpath,const uint32_t bulk_size);

};

class streamed_travel_list_reader_c: public streamed_travel_list_c {
    private:
    flight_indice_t* m_hdr_buffer;
    FILE* m_hdr;
    FILE* m_data;
    uint64_t m_hdr_len,m_hdr_head;
    uint64_t m_bulk_size;
    std::string m_hdr_filename,m_data_filename; //To clean them up

    public:
    streamed_travel_list_reader_c() : m_hdr_buffer(0), m_hdr(0),m_data(0){}
    streamed_travel_list_reader_c(const std::vector<override_stl_allocator(travel_t)>& travels,const std::string& fpath,
                        const uint32_t bulk_size) { this->init(travels,fpath,bulk_size); }
    ~streamed_travel_list_reader_c() {this->shutdown();}

    void shutdown();
    boolean_t next_bulk(std::vector<override_stl_allocator(travel_t)>& res);
    boolean_t init(const std::vector<override_stl_allocator(travel_t)>& travels,const std::string& fpath,const uint32_t bulk_size);
};

 

#endif


