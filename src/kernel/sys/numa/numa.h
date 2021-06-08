//! @param numa.h
//! @brief File containing NUMA nodes manager interface definition

#pragma once

#include <mem/rc.h> // For reference-counted objects

//! @brief Type of NUMA node ID
typedef uint8_t numa_id_t;

//! @brief Type of NUMA distance between two nodes
typedef uint8_t numa_distance_t;

enum {
	//! @brief Max NUMA nodes count.
	NUMA_MAX_NODES = 256
};

//! @brief Structure representing one NUMA node
struct numa_node {
	//! @brief Reference counting base
	struct mem_rc rc_base;
	//! @brief Next active NUMA node
	struct numa_node *next;
	//! @brief NUMA ID of the node itself
	numa_id_t node_id;
	//! @brief Accessible neighbours sorted by distance (from the smallest to the largest)
	//! @note Includes self for convinience
	numa_id_t neighbours[NUMA_MAX_NODES];
	//! @brief Number of used entries in neighbours array
	numa_id_t used_entries;
};

//! @brief Initialize NUMA subsystem
void numa_init(void);

//! @brief Take NUMA subsystem lock
void numa_acquire(void);

//! @brief Drop NUMA subsystem lock
void numa_release(void);

//! @brief Query NUMA node data by ID
//! @return Borrowed RC reference to the numa data
struct numa_node *numa_query_data(numa_id_t data);
