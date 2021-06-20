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

enum
{
	//! @brief Max NUMA nodes count.
	NUMA_MAX_NODES = 256
};

//! @brief Structure representing one NUMA node
struct numa_node {
	//! @brief Reference counting base
	struct mem_rc rc_base;
	//! @brief Next NUMA node
	struct numa_node *next;
	//! @brief Next permanent NUMA node
	struct numa_node *next_permanent;
	//! @brief NUMA ID of the node itself
	numa_id_t node_id;
	//! @brief Permanent accessible neighbours sorted by distance (from the smallest to the largest)
	//! @note Includes self for convinience if self is permanent
	numa_id_t permanent_neighbours[NUMA_MAX_NODES];
	//! @brief Number of used entries in permanent_neighbours array
	numa_id_t permanent_used_entries;
	//! @brief Hotpluggable accessible neighbours sorted by distance (from the smallest to the
	//! largest)
	//! @note Includes self for convinience
	numa_id_t neighbours[NUMA_MAX_NODES];
	//! @brief Number of used entries in hotpluggable_neighbours array
	numa_id_t used_entries;
	//! @brief Permanent memory ranges
	struct mem_range *permanent_ranges;
	//! @brief Hotpluggable memory ranges
	struct mem_range *hotpluggable_ranges;
	//! @brief Heap slabs on this node
	struct mem_heap_slab_data slab_data;
	//! @brief True if node is permanent
	bool permanent;
	//! @brief Node's lock
	struct thread_spinlock lock;
};

//! @brief Head of NUMA nodes list
extern struct numa_node *numa_nodes;

//! @brief Head of permanent NUMA nodes list
extern struct numa_node *numa_permanent_nodes;

//! @brief Take NUMA subsystem lock
//! @return Interrupt state
bool numa_acquire(void);

//! @brief Drop NUMA subsystem lock
//! @param state Interrupt state returned from numa_acquire
void numa_release(bool state);

//! @brief Query NUMA node data by ID without borrowing reference
//! @return Weak reference to the data
struct numa_node *numa_query_data_no_borrow(numa_id_t data);

//! @brief Export ACPI init target
EXPORT_TARGET(numa_available)
