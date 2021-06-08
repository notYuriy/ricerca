//! @param numa.c
//! @brief File containing NUMA nodes manager interface implementation

#include <lib/log.h>                 // For logging functions
#include <lib/panic.h>               // For PANIC and ASSERT
#include <lib/static_handle_table.h> // For handle table
#include <mem/rc.h>                  // For reference-counted objects
#include <mem/rc_static_pool.h>      // For static RC pool
#include <sys/acpi/numa.h>           // For NUMA related ACPI wrapper functions
#include <sys/numa/numa.h>           // For declarations of functions below

//! @brief Module name
#define MODULE "numa"

//! @brief Backing memory for NUMA nodes static memory pool
static struct numa_node numa_nodes[NUMA_MAX_NODES];

//! @brief NUMA nodes static RC pool
struct mem_rc_static_pool numa_node_pool = STATIC_POOL_INIT(struct numa_node, numa_nodes);

//! @brief Backing memory for NUMA nodes handle tables
static struct mem_rc *numa_table_backer[NUMA_MAX_NODES] = {NULL};

//! @brief Handle table for indexing NUMA nodes
static struct static_handle_table numa_table = STATIC_HANDLE_TABLE_INIT(numa_table_backer);

//! @brief Initialize neighbours array by enumerating the numa_table
//! @param node Node in which neighbour list should be initialized
//! @param list List of the entirety of NUMA
static void numa_get_neighbours_list(struct numa_node *node, struct numa_node *list) {
	struct numa_node *current = list;
	while (current != NULL) {
		node->neighbours[node->used_entries++] = current->node_id;
		current = current->next;
	}
}

//! @brief Sort neighbours array by distance
//! @param node Node neighbours of which are to be sorted
static void numa_sort_neighbours(struct numa_node *node) {
	// I just wrote selection sort cause I don't think its worth to do quick sort for 255 elems
	numa_id_t unreachable_index = node->used_entries;
	for (size_t i = 0; i < node->used_entries; ++i) {
		numa_id_t minimum_id = i;
		numa_distance_t minimum_distance =
		    acpi_numa_get_distance(node->node_id, node->neighbours[i]);
		for (size_t j = i + 1; j < node->used_entries; ++j) {
			numa_distance_t new_distance =
			    acpi_numa_get_distance(node->node_id, node->neighbours[j]);
			if (new_distance < minimum_distance) {
				minimum_distance = new_distance;
				minimum_id = j;
			}
		}
		if (minimum_distance == 0xff) {
			unreachable_index = i;
			break;
		}
		numa_id_t tmp = node->neighbours[i];
		node->neighbours[i] = node->neighbours[minimum_id];
		node->neighbours[minimum_id] = tmp;
	}
	node->used_entries = unreachable_index;
}

//! @brief Dump NUMA nodes
static void numa_dump_nodes() {
	LOG_INFO("Dumping NUMA nodes");
	for (size_t i = 0; i < NUMA_MAX_NODES; ++i) {
		if (numa_table.handles[i] == NULL) {
			continue;
		}
		struct numa_node *current = (struct numa_node *)(numa_table.handles[i]);
		log_printf("Node %%\033[36m%u\033[0m: { neighbours: { ", (uint32_t)current->node_id);
		for (size_t i = 0; i < current->used_entries; ++i) {
			log_printf("%%\033[32m%u\033[0m, ", (uint32_t)current->neighbours[i]);
		}
		log_write("} }, \n", 6);
	}
}

//! @brief Initialize NUMA subsystem
void numa_init(void) {
	// Enumerate all domains and create nodes for them
	numa_id_t buf;
	struct numa_node *current = NULL;
	while (acpi_numa_enumerate_at_boot(&buf)) {
		if (numa_table.handles[buf] == NULL) {
			LOG_INFO("Allocating NUMA node %u", (uint32_t)buf);
			// Allocate node from the static pool
			struct numa_node *node = STATIC_POOL_ALLOC(&numa_node_pool, struct numa_node);
			// Add node to the list of active nodes
			node->next = current;
			current = node;
			// Set node ID field
			node->node_id = buf;
			node->used_entries = 0;
			if (node == NULL) {
				PANIC("Bootstrap NUMA node allocation failed");
			}
			// Reserve static handle table cell for this NUMA node
			static_handle_table_reserve(&numa_table, buf, (struct mem_rc *)node);
		}
	}
	// Iterate all nodes and build proximity graph
	struct numa_node *iter = current;
	while (iter != NULL) {
		numa_get_neighbours_list(iter, current);
		numa_sort_neighbours(iter);
		iter = iter->next;
	}
	numa_dump_nodes();
	LOG_SUCCESS("Initialization finished!");
}
