#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

/* 
 * Reads a memory access from the trace file and returns
 * 1) access type (instruction or data access
 * 2) memory address
 */
mem_access_t read_transaction(FILE* ptr_file) {
    char buf[1000];
    char* token;
    char* string = buf;
    mem_access_t access;

    if (fgets(buf, 1000, ptr_file) != NULL) {
        /* Get the access type */
        token = strsep(&string, " \n");
        if (strcmp(token, "I") == 0) {
            access.accesstype = instruction;
        } else if (strcmp(token, "D") == 0) {
            access.accesstype = data;
        } else {
            printf("Unkown access type\n");
            exit(0);
        }

        /* Get the access type */
        token = strsep(&string, " \n");
        access.address = (uint32_t)strtol(token, NULL, 16);

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

    /**
     * addr_size:   32 b
     * block_size:  64 B
     * block_bits:  6 b
     * num_blocks:  cache_size/blocks
     * index_bits:  log_2(num_blocks)
     */
    uint8_t num_blocks = cache_size/64; // Blocks are 64B
    uint8_t num_index_bits = 0;
    for (; num_blocks > 1; num_index_bits++) num_blocks >>= 1; // Calculates log2 of 2^integer
    uint8_t num_offset_bits = 6;
    uint8_t num_tag_bits = 32 - num_offset_bits - num_index_bits; // 32 b addr, 6 b offset (64 B blocks)

    uint32_t tag_mask = (0xffffffff<<num_tag_bits);
    uint32_t index_mask = (0xffffffff<<num_offset_bits) & ~tag_mask;
    uint32_t offset_mask = ~(tag_mask | index_mask);

    /* Open the file mem_trace.txt to read memory accesses */
    FILE* ptr_file;
    ptr_file = fopen("mem_trace1.txt", "r");
    if (!ptr_file) {
        printf("Unable to open the trace file\n");
        exit(1);
    }

    /* Loop until whole trace file has been read */
    mem_access_t access;
    while (1) {
        access = read_transaction(ptr_file);
        // If no transactions left, break out of loop
        if (access.address == 0) break;
        printf("%d %x\n", access.accesstype, access.address);
        /* Do a cache access */
        // ADD YOUR CODE HERE

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
