#ifndef _static_strings_hpp_
#define _static_strings_hpp_
/*
    This is a simple and effective "perfect string hashing" function replacement...
 
    Author : Dimitris Vlachos ( dimitrisv22@gmail.com )
*/

#include "types.hpp"

typedef uint32_t indexed_string_t;

boolean_t ss_init();
void ss_shutdown();
const std::string& ss_resolve(const indexed_string_t ind);
indexed_string_t ss_register(const std::string& in);
#endif

