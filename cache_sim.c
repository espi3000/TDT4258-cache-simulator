#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
} cache_stat_t;

uint32_t cache_size;
uint32_t block_size = 64;
cache_map_t cache_mapping;
cache_org_t cache_org;

typedef struct {
    uint32_t tag;
    uint8_t valid;
} block_t;

typedef struct {
    block_t* blocks;
} cache_t;

/**
 * @brief Checks cache for a hit and updates cache
 * 
 * @param cache 
 * @param tag 
 * @param index 
 * @return 1 if hit, 0 if miss
 */
uint8_t check_block_dm(cache_t cache, uint32_t tag, uint32_t index) {
    if (cache.blocks[index].valid == 0) {
        cache.blocks[index].tag = tag;
        cache.blocks[index].valid = 1;
    } else if (cache.blocks[index].tag == tag) {
        return 1;
    } else {
        cache.blocks[index].tag = tag;  // This causes segmentation fault
    }
    return 0;
}

/**
 * @brief Checks cache for a hit and updates cache
 * 
 * @param cache 
 * @param tag 
 * @param num_blocks 
 * @return 1 if hit, 0 if miss
 */
uint8_t check_block_fa(cache_t cache, uint32_t tag, uint8_t num_blocks) {
    static uint8_t first_in_index = 0;
    for (uint8_t i = 0; i < num_blocks; i++) {
        if (cache.blocks[i].valid == 0) {
            cache.blocks[i].tag = tag;
            cache.blocks[i].valid = 1;
            break;
        } else if (cache.blocks[i].tag == tag) {
            return 1;
        } else if (i == num_blocks - 1) {
            cache.blocks[first_in_index].tag = tag;
            first_in_index++;
            if (first_in_index == num_blocks) first_in_index = 0;
        }
    }
    return 0;
}

/**
 * @brief Opens file in read mode and returns a pointer to the file
 * 
 * @param filename path of file with file extension
 * @return FILE* pointer to file
 */
FILE* open_file(const char* filename) {
    FILE* ptr_file = fopen(filename, "r");
    if (!ptr_file) {
        printf("Unable to open the trace file\n");
        exit(1);
    }
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

void main(int argc, char** argv) { // argv[0] is program name, parameters start with argv[1]
    uint8_t num_blocks;
    uint8_t num_offset_bits;
    uint8_t num_index_bits;
    uint8_t num_tag_bits;
    uint32_t tag_mask;
    uint32_t index_mask;
    //uint32_t offset_mask;

    cache_t cache;
    cache_t cache2;

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
        cache_size = atoi(argv[1]);         // Set cache size
        num_blocks = cache_size/block_size; // Calculate number of blocks

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
            cache.blocks = (block_t*)malloc(num_blocks*sizeof(block_t)); // Allocate memory for blocks
            for (uint8_t i = 0; i < num_blocks; i++) { // Initialize cache to 0
                cache.blocks[i].valid = 0;
                cache.blocks[i].tag = 0;
            }
        } else if (strcmp(argv[3], "sc") == 0) {
            cache_org = sc;
            num_blocks /= 2;
            cache.blocks = (block_t*)malloc(num_blocks*sizeof(block_t)); // Allocate memory for blocks
            cache2.blocks = (block_t*)malloc(num_blocks*sizeof(block_t)); // Allocate memory for blocks
            for (uint8_t i = 0; i < num_blocks; i++) { // Initialize cache to 0
                cache.blocks[i].valid = 0;
                cache.blocks[i].tag = 0;
                cache2.blocks[i].valid = 0;
                cache2.blocks[i].tag = 0;
            }
        } else {
            printf("Unknown cache organization\n");
            exit(0);
        }
    }

    /* Calculate number of bits for offset, index and tag */
    num_offset_bits = 6;        // 2^6 = 64 bytes per block
    num_index_bits = 0;         // Calculates log2 of 2^num_blocks to calculate index bits
    for (uint8_t temp = num_blocks; temp > 1; num_index_bits++) temp >>= 1;
    num_tag_bits = 32 - num_offset_bits - num_index_bits; // Always 32 bit address

    /* Calculate bitmasks*/
    tag_mask = (0xffffffff << (32 - num_tag_bits));
    index_mask = (0xffffffff << num_offset_bits) & ~tag_mask;
    //offset_mask = ~(tag_mask | index_mask);

    FILE* ptr_file = open_file("mem_trace2.txt"); // Read memory trace file
    mem_access_t access;
    cache_stat_t cache_statistics;
    memset(&cache_statistics, 0, sizeof(cache_stat_t)); // Reset statistics:
    while (1) { // Loop until whole trace file has been read
        access = read_transaction(ptr_file);
        if (access.address == 0) break; // If no transactions left, break out of loop
        cache_statistics.accesses++;
        //printf("%d %x\n", access.accesstype, access.address);
        uint32_t index;
        uint32_t tag;
        if (cache_mapping == dm) {
            index = (access.address & index_mask) >> num_offset_bits;
            tag = (access.address & tag_mask) >> (num_offset_bits + num_index_bits);
            if (cache_org == uc) {
                cache_statistics.hits += check_block_dm(cache, tag, index);
            } else {
                if (access.accesstype == data) {
                    cache_statistics.hits += check_block_dm(cache, tag, index);
                } else {
                    cache_statistics.hits += check_block_dm(cache2, tag, index);
                }
            }
        } else {
            tag = (access.address & (tag_mask | index_mask)) >> num_offset_bits;
            if (cache_org == uc) {
                cache_statistics.hits += check_block_fa(cache, tag, num_blocks);
            } else {
                if (access.accesstype == data) {
                    cache_statistics.hits += check_block_fa(cache, tag, num_blocks);
                } else {
                    cache_statistics.hits += check_block_fa(cache2, tag, num_blocks);
                } 
            }
        }
    }

    // DO NOT CHANGE THE FOLLOWING LINES!
    printf("\nCache Statistics\n");
    printf("-----------------\n\n");
    printf("Accesses: %ld\n", cache_statistics.accesses);
    printf("Hits:     %ld\n", cache_statistics.hits);
    printf("Hit Rate: %.4f\n",
         (double)cache_statistics.hits / cache_statistics.accesses);
    // DO NOT CHANGE UNTIL HERE

    fclose(ptr_file);       // Close the trace file
    free(cache.blocks);     // Free the allocated memory
    free(cache2.blocks);    // Free the allocated memory
}