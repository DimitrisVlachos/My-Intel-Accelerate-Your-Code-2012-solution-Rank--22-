/*
    io module : Handles all IO operations
    Author : Dimitris Vlachos ( dimitrisv22@gmail.com )
*/

#include "io.hpp"
#include <cerrno>

inline uint64_t streamed_travel_list_writer_c::flush_stream(FILE* strm,const flight_indice_t* data,uint32_t& ptr) {
    if (0 != ptr) {
        uint64_t dummy = fwrite(data,1,ptr * sizeof(flight_indice_t),strm);
        ptr = 0;
        return dummy;
    }
    return 0;
}

inline void streamed_travel_list_writer_c::write(FILE* strm,flight_indice_t* data,uint32_t& ptr,const uint32_t len,const flight_indice_t arg) {
    if ((ptr + 1) >= len) {
        flush_stream(strm,data,ptr);
    }
    data[ptr++] = arg;
}

boolean_t streamed_travel_list_writer_c::init(const std::vector<override_stl_allocator(travel_t)>& travels,
                                        const std::string& fpath,const uint32_t bulk_size) {
    if (0 == bulk_size) {
        printf("bulk size => 0\n");
        assert(0);
    }

    std::string tmp_path = fpath;
    std::string hdr_fn,data_fn;
    FILE* hdr;
    FILE* data;
    flight_indice_t* hdr_buffer;
    flight_indice_t* data_buffer;
    const uint32_t hdr_buffer_len = 4096;
    const uint32_t data_buffer_len = 4096;
    uint32_t hdr_buffer_head = 0,data_buffer_head = 0;
    uint64_t data_stream_offs = 0;

    hdr_buffer = new flight_indice_t[hdr_buffer_len];
    assert(hdr_buffer != 0);
    data_buffer = new flight_indice_t[data_buffer_len];
    assert(data_buffer != 0);

    if (!tmp_path.empty()) {
        if (tmp_path[tmp_path.length()-1] != '/') {
        tmp_path += '/';
        }
    }

    hdr_fn = tmp_path + "hdr.bin";
    data_fn = tmp_path + "data.bin";

    hdr = fopen64(hdr_fn.c_str(),"wb");
    if (!hdr) {
        printf("Unable to open %s\n",hdr_fn.c_str());   
        assert(0);
    }

    data = fopen64(data_fn.c_str(),"wb");
    if (!data) {
        printf("Unable to open %s\n",data_fn.c_str());   
        assert(0);
    }
    
    //First 8 hdr bytes : flight count  + 4bytes for padding
    write(hdr,hdr_buffer,hdr_buffer_head,hdr_buffer_len,(flight_indice_t)(travels.size() >> 32)); 
    write(hdr,hdr_buffer,hdr_buffer_head,hdr_buffer_len,(flight_indice_t)(travels.size()  )); 
    write(hdr,hdr_buffer,hdr_buffer_head,hdr_buffer_len,(flight_indice_t)0); 

    for (uint32_t i = 0,j = travels.size();i < j;++i) {
        const std::vector<override_stl_allocator(flight_indice_t)>& flights = travels[i].flights;
        const uint32_t flights_size = (uint32_t)flights.size();

        //Header section
        write(hdr,hdr_buffer,hdr_buffer_head,hdr_buffer_len,(flight_indice_t)flights_size); 
        write(hdr,hdr_buffer,hdr_buffer_head,hdr_buffer_len,(flight_indice_t)(data_stream_offs >> 32));
        write(hdr,hdr_buffer,hdr_buffer_head,hdr_buffer_len,(flight_indice_t)(data_stream_offs));

        //Data section
        for (uint32_t j = 0;j < flights_size;++j) {
            write(data,data_buffer,data_buffer_head,data_buffer_len,flights[j]); 
        }

        //Inc offs
        data_stream_offs += (uint64_t)flights_size * sizeof(flight_indice_t);
    }

    flush_stream(hdr,hdr_buffer,hdr_buffer_head);
    flush_stream(data,data_buffer,data_buffer_head);

    fclose(hdr);
    fclose(data);

    delete[] hdr_buffer;
    delete[] data_buffer;
    return true;
}
 
void streamed_travel_list_reader_c::shutdown() {
 
    delete[] m_hdr_buffer;

    if (m_hdr) {
        fclose(m_hdr);

        //Cleanup contexts
        m_hdr = fopen(m_hdr_filename.c_str(),"wb");
        if (m_hdr) {
            fclose(m_hdr);
        }
    }

    if (m_data) {
        fclose(m_data);

        //Cleanup contexts
        m_data = fopen(m_data_filename.c_str(),"wb");
        if (m_data) {
            fclose(m_data);
        }
    }

    m_hdr_buffer = 0;
    m_hdr = 0;
    m_data = 0;
}

boolean_t streamed_travel_list_reader_c::next_bulk(std::vector<override_stl_allocator(travel_t)>& res) {
    uint64_t rd_len;
    uint64_t rd_block_len;

    if ((!m_hdr) || (!m_data)) {
        printf("next_bulk : streams not open!\n");
        assert(0);
    }

    if (m_hdr_head >= m_hdr_len) {
        return false;
    }

    rd_len = m_bulk_size;
    
    if ((m_hdr_head + rd_len) >= m_hdr_len) {
        rd_len = m_hdr_len - m_hdr_head;
    }

    rd_block_len = (rd_len * sizeof(flight_indice_t));
    rd_block_len = (rd_block_len << 1) + rd_block_len;
    
    uint64_t dummy;

    dummy = fread(m_hdr_buffer,1,rd_block_len,m_hdr);
    dummy += dummy; //skip warnings for fread
    res.clear();
    res.reserve(rd_len);

    for (uint64_t i = 0;i < rd_len;++i) {
        const uint64_t indice = (i << 1) + i;
        const uint64_t offset = ((uint64_t)m_hdr_buffer[indice + 1] << 32) | (uint64_t)m_hdr_buffer[indice + 2];

        res.push_back(travel_t());
        res.back().flights.resize(m_hdr_buffer[indice]);
    
        fseeko64(m_data,offset,SEEK_SET);//SEEK_CURR);
        dummy += fread((void*)&res.back().flights.front(),sizeof(flight_indice_t),m_hdr_buffer[indice],m_data);
    }

    m_hdr_head += rd_len;
    return true;
}

boolean_t streamed_travel_list_reader_c::init(const std::vector<override_stl_allocator(travel_t)>& travels,
                                                const std::string& fpath,const uint32_t bulk_size) {
    if (0 == bulk_size) {
        printf("bulk size => 0\n");
        assert(0);
    }

    std::string tmp_path = fpath;
    std::string hdr_fn,data_fn;

    if (!tmp_path.empty()) {
        if (tmp_path[tmp_path.length()-1] != '/') {
        tmp_path += '/';
        }
    }

    m_bulk_size = (uint64_t)bulk_size;
    m_hdr_head = 0;
    hdr_fn = tmp_path + "hdr.bin";
    data_fn = tmp_path + "data.bin";
    m_hdr_filename = hdr_fn;
    m_data_filename = data_fn;

    m_hdr_buffer = new flight_indice_t[bulk_size * 3];

    m_hdr = fopen64(hdr_fn.c_str(),"rb");
    if (!m_hdr) {
        printf("Unable to open %s\n",hdr_fn.c_str());   
        assert(0);
    }
 
    uint32_t dummy;

    flight_indice_t tmp;
    dummy = fread((void*)&tmp,1,sizeof(flight_indice_t),m_hdr);

    m_hdr_len = (uint64_t)tmp << 32;
    dummy+=fread((void*)&tmp,1,sizeof(flight_indice_t),m_hdr);
    m_hdr_len |= (uint64_t)tmp;
    dummy+=fread((void*)&tmp,1,sizeof(flight_indice_t),m_hdr); //pad bytes

    m_data = fopen64(data_fn.c_str(),"rb");
    if (!m_data) {
        printf("Unable to open %s\n",data_fn.c_str());   
        assert(0);
    }
    return true;
}
 
 
 

