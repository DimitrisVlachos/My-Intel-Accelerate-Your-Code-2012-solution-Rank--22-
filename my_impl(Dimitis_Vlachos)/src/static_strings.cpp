/*
    This is a simple and effective "perfect string hashing" function replacement...
 
    Author : Dimitris Vlachos ( dimitrisv22@gmail.com )
*/

#include "static_strings.hpp"
#include "mt.hpp"

struct parent_t {
    std::vector<std::string> children;
};

/*Root of tree*/
std::vector<parent_t> ss_root; 

/*Zero length string result*/
static const std::string k_p_ss_identity = "";

/*Zero length string signature*/
static const indexed_string_t k_ss_identity = (indexed_string_t)std::numeric_limits<indexed_string_t>::max(); 

/*Encode a parent:child reference*/
static inline indexed_string_t encode(const indexed_string_t parent,const indexed_string_t child) {
    return (indexed_string_t)( (parent << 16) | child);
}

/*Decode a parent:child reference*/
static inline void decode(const indexed_string_t encoded,indexed_string_t& parent,indexed_string_t& child) {
    parent = (encoded >> 16);
    child = encoded & (indexed_string_t)0x0000ffff;
}

/*Find most optimal path for this entry..*/
static inline indexed_string_t calc_parent(const std::string& s) {
    const indexed_string_t parent = (indexed_string_t)s[0];
    const uint32_t len = s.length();

    if (len > 1) {
        const indexed_string_t half_extent = len >> 1;
        const indexed_string_t r = (parent << 8);
        indexed_string_t mid = (indexed_string_t)s[half_extent];
#if 0
        indexed_string_t tmp;

        if ((mid >> 4) == 0) {
            mid = (mid & 0xf) << 4;
            if (half_extent + 1 < len) {
                tmp = (indexed_string_t)s[half_extent + 1];
                if ( (tmp >> 4) == 0 ) {
                    mid |= (tmp & 0xf);
                } else {
                    mid |= (tmp >> 4);
                }
            }
        } else if ((mid & 0xf) == 0) {
            if (half_extent + 1 < len) {
                tmp = (indexed_string_t)s[half_extent + 1];
                if ( (tmp >> 4) == 0 ) {
                    mid |= (tmp & 0xf);
                } else {
                    mid |= (tmp >> 4);
                }
            }
        }
#endif
        if (ss_root[r | mid].children.size() >= (1 << 16)) {
            return r | ((s[1] > s[len - 1]) ? s[1] : s[len - 1] );
        }

        return r | mid;
    }

    return parent;
}

boolean_t ss_init() {
    /*Initialize range to 2^16*/
    const indexed_string_t range = 1 << 16;
    indexed_string_t test;

    ss_root.clear();
    ss_root.reserve(range);

    for (indexed_string_t i = 0;i < range;++i) {
        test = ss_root.size();
        ss_root.push_back(parent_t());
        if (ss_root.size() <= test) {
            return false;
        } 
    }

    return true;
}

void ss_shutdown() {
    ss_root.clear();
}

/*Convert index back to string*/
const std::string& ss_resolve_safe(const indexed_string_t ind) {
    indexed_string_t p,c;

    if (k_ss_identity == ind) {
        return k_p_ss_identity;
    }

    decode(ind,p,c);

    if ((p >= ss_root.size()) || (c >= ss_root[p].children.size())) {
        printf("ss_resolve_safe failed\n");
        assert(0);
    }

    return ss_root[p].children[c];
}

/*Convert index back to string*/
const std::string& ss_resolve(const indexed_string_t ind) {
    indexed_string_t p,c;

    if (k_ss_identity == ind) {
        return k_p_ss_identity;
    }

    decode(ind,p,c);

    return ss_root[p].children[c];
}

static const boolean_t ss_match(const std::string& a,const std::string& b) {
    const indexed_string_t l0 = a.length();
    const indexed_string_t l1 = b.length();
    register const uint8_t* pa,*ea;
    register const uint8_t* pb;

    //If the length is different branch out early
    if (l0 != l1) {
        return false;
    }

    pa = (const uint8_t*)a.c_str();
    pb = (const uint8_t*)b.c_str();
    ea = pa + l0;

    //Check last,first 2 and mid point
    if (pa[l0-1] != pb[l0-1]) {
        return false;
    } else  if (pa[l0 >> 1] != pb[l0 >> 1]) {
        return false;
    } else if (l0 > 2) {
        if (pa[l0-2] != pb[l0-2]) {
            return false;
        } else if (pa[2] != pb[2]) {
            return false;
        }
    }

    //Fallback to full scan
    while ((ea - pa) >= 8) { //Compare quads
        if ((*(uint64_t*)pa) != (*(uint64_t*)pb) ) {
            return false;
        }
        pa += 8;
        pb += 8;
    }

    while ((ea - pa) >= 4) { //Compare longs
        if ((*(indexed_string_t*)pa) != (*(indexed_string_t*)pb) ) {
            return false;
        }
        pa += 4;
        pb += 4;
    }

    while (pa < ea) { //Compare...bytes
        if ((*pa) != (*pb) ) {
            return false;
        }
        ++pa;
        ++pb;
    }

    return true;
}

/*Convert string to index*/
indexed_string_t ss_register(const std::string& in) {
    indexed_string_t res;
    indexed_string_t parent;
    
    if (in.empty()) {
        return k_ss_identity;
    }
     
    parent = calc_parent(in);
    register std::vector<std::string>& children = ss_root[parent].children;

    //mt_ss_match is slower than this 
    for (register indexed_string_t i = 0,j = children.size();i < j;++i) {
        if (ss_match(children[i],in)) {
            return encode(parent,i);
        }
    }

    res = encode(parent,children.size());
    children.push_back(in);

#if 0
    static uint32_t elems = 0;
    ++elems;
    printf("%u c %u\n",elems,children.size());
    if (children.size() >= (1 << 16)) {
        assert(0);
    }
#endif
    return res;
}

