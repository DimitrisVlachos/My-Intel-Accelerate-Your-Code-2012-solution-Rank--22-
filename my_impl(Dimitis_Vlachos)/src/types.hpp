#ifndef _types_hpp_
#define _types_hpp_

#include <stdint.h>
#include <iostream>
#include <cstdlib>
#include <string>
#include <list>
#include <vector>
#include <fstream>
#include <time.h>
#include <cassert>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits>


typedef int32_t boolean_t;
typedef float f32;
typedef double f64;

#if 0
 
    #include <tbb/scalable_allocator.h>
 
    #define override_stl_allocator(_base_) _base_,tbb::scalable_allocator<_base_>
 
#else
#if 0
   // #include <ext/pool_allocator.h>
    #include <ext/mt_allocator.h>
    //#include <ext/bitmap_allocator.h>

    #define override_stl_allocator(_base_) _base_,__gnu_cxx::__mt_alloc<_base_>

#else
    #define override_stl_allocator(_base_) _base_
#endif
#endif

#if defined __INTEL_COMPILER
    #define likely(_expr_)  (_expr_)
    #define unlikely(_expr_)(_expr_)
#elif defined __GNUC__ 
    #define likely(_expr_)  ( __builtin_expect(!!(_expr_),1))
    #define unlikely(_expr_)( __builtin_expect(!!(_expr_),0))
#else
    #define likely(_expr_)  (_expr_)
    #define unlikely(_expr_)(_expr_)
#endif

#define _little_endian_cpu_
#define _linux_build_
#endif

