//! @param numa.h
//! @brief File containing NUMA nodes manager interface definition

#pragma once

#include <lib/target.h>
#include <mem/heap/slab.h>
#include <mem/rc.h>
#include <thread/locking/spinlock.h>

//! @brief Type of NUMA node ID
typedef uint32_t numa_id_t;

//! @brief Type of NUMA distance between two nodes
typedef uint32_t numa_distance_t;

//! @brief Structure representing one NUMA node
struct numa_node {
	//! @brief Neighbours sorted by distance (from the smallest to the largest)
	//! @note Includes self for convinience
	numa_id_t *neighbours;
	//! @brief Memory ranges
	struct mem_range *ranges;
	//! @brief Heap slabs on this node
	struct mem_heap_slab_data slab_data;
	//! @brief Node's lock
	struct thread_spinlock lock;
	//! @brief True if node's data was initialized
	bool initialized;
};

//! @brief Number of nodes detected on the system
extern numa_id_t numa_nodes_count;

//! @brief Size of NUMA nodes array
extern numa_id_t numa_nodes_size;

//! @brief NUMA nodes array
extern struct numa_node *numa_nodes;

//! @brief Export ACPI init target
EXPORT_TARGET(numa_available)
