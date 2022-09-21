#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//#define get_tag_mask(num_tag_bits)                  (0xffffffff << (32 - num_tag_bits))
//#define get_index_mask(num_offset_bits, tag_mask)   ((0xffffffff << num_offset_bits) & ~tag_mask)
//#define get_offset_mask(tag_mask, index_mask)       (~(tag_mask | index_mask))

typedef enum {dm, fa} cache_map_t;
typedef enum {uc, sc} cache_org_t;
typedef enum {instruction, data} access_t;

typedef struct {
    uint32_t address;
    access_t accesstype;
} mem_access_t;

typedef struct {
    uint64_t accesses;
    uint64_t hits;
    // You can declare additional statistics if
    // you like, however you are now allowed to
    // remove the accesses or hits
} cache_stat_t;

// DECLARE CACHES AND COUNTERS FOR THE STATS HERE
uint32_t cache_size;
uint32_t block_size = 64;
cache_map_t cache_mapping;
cache_org_t cache_org;

// USE THIS FOR YOUR CACHE STATISTICS
cache_stat_t cache_statistics;


/**
 * TODO: Will use strtok() instead of strsep() if possible.
 * @brief Slices a string. strsep() is not standard C. This is Chris Dodd's implementation. Source:
 *        https://stackoverflow.com/questions/58244300/getting-the-error-undefined-reference-to-strsep-with-clang-and-mingw
 * @param stringp address of string
 * @param delim separator character
 * @return address of first string
 */
char* strsep(char** stringp, const char* delim) {
    char* rv = *stringp;
    if (rv) {
        *stringp += strcspn(*stringp, delim);
        if (**stringp)
            *(*stringp)++ = '\0';
        else
            *stringp = 0; 
    }
    return rv;
}

/* 
 * Reads a memory access from the trace file and returns
 * 1) access type (instruction or data access
 * 2) memory address
 */
mem_access_t read_transaction(FILE* ptr_file) {
    char type;
    mem_access_t access;
    if (fscanf(ptr_file, "%c %x\n", &type, &access.address) == 2) {
        if (type != 'I' && type != 'D') {
            printf("Unkown access type\n");
            exit(0);
        }
        access.accesstype = (type == 'I') ? instruction : data;
        return access;
    }
    /* 
     * If there are no more entries in the file,
     * return an address 0 that will terminate the infinite loop in main
     */
    access.address = 0;
    return access;
}

void main(int argc, char** argv) {
    // Reset statistics:
    memset(&cache_statistics, 0, sizeof(cache_stat_t));

    /* 
     * Read command-line parameters and initialize:
     * cache_size, cache_mapping and cache_org variables
     */
    /* 
     * IMPORTANT: *IF* YOU ADD COMMAND LINE PARAMETERS (you really don't need to),
     * MAKE SURE TO ADD THEM IN THE END AND CHOOSE SENSIBLE DEFAULTS SUCH THAT WE
     * CAN RUN THE RESULTING BINARY WITHOUT HAVING TO SUPPLY MORE PARAMETERS THAN
     * SPECIFIED IN THE UNMODIFIED FILE (cache_size, cache_mapping and cache_org)
     */
    if (argc != 4) { /* argc should be 4 for correct execution */
        printf(
            "Usage: ./cache_sim [cache size: 128-4096] [cache mapping: dm|fa] "
            "[cache organization: uc|sc]\n");
        exit(0);
    } else {
        /* argv[0] is program name, parameters start with argv[1] */

        /* Set cache size */
        cache_size = atoi(argv[1]);

        /* Set Cache Mapping */
        if (strcmp(argv[2], "dm") == 0) {
            cache_mapping = dm;
        } else if (strcmp(argv[2], "fa") == 0) {
            cache_mapping = fa;
        } else {
            printf("Unknown cache mapping\n");
            exit(0);
        }

        /* Set Cache Organization */
        if (strcmp(argv[3], "uc") == 0) {
            cache_org = uc;
        } else if (strcmp(argv[3], "sc") == 0) {
            cache_org = sc;
        } else {
            printf("Unknown cache organization\n");
            exit(0);
        }
    }

    // ============================================================
    // ============================================================
    uint8_t num_blocks = cache_size/64; // Blocks are 64B
    uint8_t num_index_bits = 0;
    for (; num_blocks > 1; num_index_bits++) num_blocks >>= 1; // Calculates log2 of 2^integer
    uint8_t num_offset_bits = 6;
    uint8_t num_tag_bits = 32 - num_offset_bits - num_index_bits; // 32 b addr, 6 b offset (64 B blocks)

    uint32_t tag_mask = (0xffffffff << (32 - num_tag_bits));
    uint32_t index_mask = (0xffffffff << num_offset_bits) & ~tag_mask;
    uint32_t offset_mask = ~(tag_mask | index_mask);

    typedef struct {
        uint32_t tag;
        uint8_t valid;
        //uint8_t dirty;
        //uint8_t lru;
    } block_t;

    typedef struct {
        block_t blocks[num_blocks];
    } cache_t;

    cache_t cache;
    memset(&cache, 0, sizeof(cache_t));
    // ============================================================
    // ============================================================

    /* Open the file mem_trace.txt to read memory accesses */
    FILE* ptr_file;
    ptr_file = fopen("mem_trace1.txt", "r");
    if (!ptr_file) {
        printf("Unable to open the trace file\n");
        exit(1);
    }

    mem_access_t access;
    while (1) { // Loop until whole trace file has been read
        access = read_transaction(ptr_file);
        cache_statistics.accesses++;
        if (access.address == 0) break; // If no transactions left, break out of loop
        printf("%d %x\n", access.accesstype, access.address);
        /* Do a cache access */

        uint8_t index;
        uint8_t tag;
        switch (cache_mapping) {
        case dm:
            index = (access.address & index_mask) >> num_offset_bits;
            tag = (access.address & tag_mask) >> (num_offset_bits + num_index_bits);
            if (cache.blocks[index].valid == 0) {
                cache.blocks[index].tag = tag;
                cache.blocks[index].valid = 1;
            } else if (cache.blocks[index].tag == tag) {
                cache_statistics.hits++;
            } else {
                cache.blocks[index].tag = tag;
            }
            break;
        case fa:
            tag = (access.address & (tag_mask | index_mask)) >> num_offset_bits;
            for (uint8_t i = 0; i < num_blocks; i++) {
                if (cache.blocks[i].valid == 0) {
                    cache.blocks[i].tag = tag;
                    cache.blocks[i].valid = 1;
                    break;
                } else if (cache.blocks[i].tag == tag) {
                    cache_statistics.hits++;
                    break;
                } else {
                    cache.blocks[i].tag = tag;
                    break;
                }
            }
            break;
        }
    }

    /* Print the statistics */
    // DO NOT CHANGE THE FOLLOWING LINES!
    printf("\nCache Statistics\n");
    printf("-----------------\n\n");
    printf("Accesses: %ld\n", cache_statistics.accesses);
    printf("Hits:     %ld\n", cache_statistics.hits);
    printf("Hit Rate: %.4f\n",
         (double)cache_statistics.hits / cache_statistics.accesses);
    // DO NOT CHANGE UNTIL HERE
    // You can extend the memory statistic printing if you like!

    /* Close the trace file */
    fclose(ptr_file);
}