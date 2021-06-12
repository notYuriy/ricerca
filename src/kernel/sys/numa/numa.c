//! @param numa.c
//! @brief File containing NUMA nodes manager interface implementation

#include <lib/log.h>
#include <lib/panic.h>
#include <lib/static_handle_table.h>
#include <mem/rc.h>
#include <mem/rc_static_pool.h>
#include <sys/acpi/numa.h>
#include <sys/numa/numa.h>
#include <thread/spinlock.h>

MODULE("sys/numa")
TARGET(numa_target, numa_init, {acpi_numa_target})

//! @brief Backing memory for NUMA nodes static memory pool
static struct numa_node numa_node_pool_backer[NUMA_MAX_NODES];

//! @brief NUMA nodes static RC pool
struct mem_rc_static_pool numa_node_pool =
    STATIC_POOL_INIT(struct numa_node, numa_node_pool_backer);

//! @brief Backing memory for NUMA nodes handle tables
static struct mem_rc *numa_table_backer[NUMA_MAX_NODES] = {NULL};

//! @brief Handle table for indexing NUMA nodes
static struct static_handle_table numa_table = STATIC_HANDLE_TABLE_INIT(numa_table_backer);

//! @brief NUMA subsystem lock
static struct thread_spinlock numa_lock = THREAD_SPINLOCK_INIT;

//! @brief Head of permanent NUMA nodes list
struct numa_node *numa_permanent_nodes;

//! @brief Head of all NUMA nodes list
struct numa_node *numa_nodes = NULL;

//! @brief Initialize neighbours array by iterating node lists
//! @param node Node in which neighbour lists should be initialized
static void numa_fill_neighbours_lists(struct numa_node *node) {
	struct numa_node *current = numa_permanent_nodes;
	while (current != NULL) {
		node->permanent_neighbours[node->permanent_used_entries++] = current->node_id;
		current = current->next_permanent;
	}
	current = numa_nodes;
	while (current != NULL) {
		node->neighbours[node->used_entries++] = current->node_id;
		current = current->next;
	}
}

//! @brief Sort neighbours array by distance
//! @param self NUMA node id of the node
//! @param neighbours Array with neighbours
//! @param neighbours_count Number of neighbours
static void numa_sort_neighbours(numa_id_t self, numa_id_t *neighbours, size_t neighbours_count) {
	// I just wrote selection sort cause I don't think its worth to do quick sort for 255 elems
	for (size_t i = 0; i < neighbours_count; ++i) {
		numa_id_t minimum_id = i;
		numa_distance_t minimum_distance = acpi_numa_get_distance(self, neighbours[i]);
		for (size_t j = i + 1; j < neighbours_count; ++j) {
			numa_distance_t new_distance = acpi_numa_get_distance(self, neighbours[j]);
			if (new_distance < minimum_distance) {
				minimum_distance = new_distance;
				minimum_id = j;
			}
		}
		numa_id_t tmp = neighbours[i];
		neighbours[i] = neighbours[minimum_id];
		neighbours[minimum_id] = tmp;
	}
}

//! @brief Dump NUMA nodes list
//! @param head Head of node's list
static void numa_dump_list(struct numa_node *head) {
	for (struct numa_node *current = head; current != NULL; current = current->next) {
		log_printf("Node %%\033[36m%u\033[0m: { neighbours: { ", (uint32_t)current->node_id);
		for (size_t i = 0; i < current->used_entries; ++i) {
			log_printf("%%\033[32m%u\033[0m, ", (uint32_t)current->neighbours[i]);
		}
		log_printf("} }\n");
	}
}

//! @brief Dump NUMA nodes
void numa_dump_nodes(void) {
	LOG_INFO("Dumping NUMA nodes.");
	numa_dump_list(numa_nodes);
}

//! @brief Take NUMA subsystem lock
//! @return Interrupt state
bool numa_acquire(void) {
	return thread_spinlock_lock(&numa_lock);
}

//! @brief Drop NUMA subsystem lock
//! @param state Interrupt state returned from numa_acquire
void numa_release(bool state) {
	thread_spinlock_unlock(&numa_lock, state);
}

//! @brief Initialize NUMA subsystem
static void numa_init(void) {
	// Enumerate all domains and create nodes for them
	numa_id_t buf;
	struct numa_node *head = NULL;
	struct numa_node *tail = NULL;
	while (acpi_numa_enumerate_at_boot(&buf)) {
		if (numa_table.handles[buf] == NULL) {
			LOG_INFO("Allocating NUMA node %u", (uint32_t)buf);
			// Allocate node from the static pool
			struct numa_node *node = STATIC_POOL_ALLOC(&numa_node_pool, struct numa_node);
			// Add node to the list of active nodes
			node->next = NULL;
			node->next_permanent = NULL;
			if (head == NULL) {
				head = node;
				tail = node;
			} else {
				tail->next = node;
				tail->next_permanent = node;
				tail = node;
			}
			// Set node ID field
			node->node_id = buf;
			node->permanent_used_entries = 0;
			node->used_entries = 0;
			node->permanent = true;
			node->hotpluggable_ranges = NULL;
			node->permanent_ranges = NULL;
			node->slub_data = MEM_HEAP_SLUB_DATA_INIT;
			if (node == NULL) {
				PANIC("Bootstrap NUMA node allocation failed");
			}
			// Reserve static handle table cell for this NUMA node
			static_handle_table_reserve(&numa_table, buf, (struct mem_rc *)node);
		}
	}
	numa_permanent_nodes = numa_nodes = head;
	// Iterate all nodes and build proximity graph
	struct numa_node *iter = numa_nodes;
	while (iter != NULL) {
		numa_fill_neighbours_lists(iter);
		numa_sort_neighbours(iter->node_id, iter->permanent_neighbours,
		                     iter->permanent_used_entries);
		iter = iter->next;
	}
	numa_dump_nodes();
	LOG_SUCCESS("Initialization finished!");
}

//! @brief Query NUMA node data by ID without borrowing reference
//! @return Borrowed RC reference to the numa data
//! @note Use only at boot
struct numa_node *numa_query_data_no_borrow(numa_id_t data) {
	if (numa_table.handles[data] != NULL) {
		return (struct numa_node *)numa_table.handles[data];
	}
	return NULL;
}
