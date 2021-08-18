//! @param numa.c
//! @brief File containing NUMA nodes manager interface implementation

#include <lib/log.h>
#include <lib/panic.h>
#include <mem/bootstrap.h>
#include <mem/rc.h>
#include <mem/rc_static_pool.h>
#include <sys/acpi/numa.h>
#include <sys/numa/numa.h>
#include <thread/locking/spinlock.h>

MODULE("sys/numa")
TARGET(numa_available, numa_init, {acpi_numa_available, mem_bootstrap_alloc_available})

//! @brief Number of nodes detected on the system
numa_id_t numa_nodes_count;

//! @brief NUMA nodes array
struct numa_node *numa_nodes;

//! @brief Size of NUMA nodes array
numa_id_t numa_nodes_size;

//! @brief Initialize neighbours array by iterating node lists
//! @param id ID of the node for which neighbour lists should be initialized
static void numa_init_neighbour_list(numa_id_t id) {
	numa_id_t *neighbours_array = mem_bootstrap_alloc(numa_nodes_count * sizeof(numa_id_t));
	size_t currrent_index = 0;
	for (size_t i = 0; i < numa_nodes_size; ++i) {
		if (numa_nodes[i].initialized) {
			neighbours_array[currrent_index++] = i;
		}
	}
	for (size_t i = 0; i < numa_nodes_count; ++i) {
		numa_id_t minimum_id = i;
		numa_distance_t minimum_distance = acpi_numa_get_distance(id, neighbours_array[i]);
		for (size_t j = i + 1; j < numa_nodes_count; ++j) {
			numa_distance_t new_distance = acpi_numa_get_distance(id, neighbours_array[j]);
			if (new_distance < minimum_distance) {
				minimum_distance = new_distance;
				minimum_id = j;
			}
		}
		numa_id_t tmp = neighbours_array[i];
		neighbours_array[i] = neighbours_array[minimum_id];
		neighbours_array[minimum_id] = tmp;
	}
	struct numa_node *self = numa_nodes + id;
	self->neighbours = neighbours_array;
}

//! @brief Dump NUMA nodes
static void numa_dump_nodes(void) {
	for (numa_id_t i = 0; i < numa_nodes_size; ++i) {
		if (!numa_nodes[i].initialized) {
			continue;
		}
		log_printf("Node %%\033[36m%u\033[0m: { neighbours: { ", i);
		for (size_t j = 0; j < numa_nodes_count; ++j) {
			log_printf("%%\033[32m%u\033[0m, ", numa_nodes[i].neighbours[j]);
		}
		log_printf("} }\n");
	}
}

//! @brief Estimate upper bound on NUMA nodes
static numa_id_t numa_nodes_upper_bound(void) {
	struct acpi_numa_proximities_iter iter = ACPI_NUMA_PROXIMITIES_ITER_INIT;
	numa_id_t buf;
	numa_id_t max = 0;
	while (acpi_numa_enumerate_at_boot(&iter, &buf)) {
		if (buf > max) {
			max = buf;
		}
	}
	return max + 1;
}

//! @brief Initialize NUMA subsystem
static void numa_init(void) {
	// Create array for all NUMA nodes
	numa_nodes_size = numa_nodes_upper_bound();
	numa_nodes = mem_bootstrap_alloc(sizeof(struct numa_node) * numa_nodes_size);
	for (numa_id_t i = 0; i < numa_nodes_size; ++i) {
		numa_nodes[i].initialized = false;
	}
	struct acpi_numa_proximities_iter iter = ACPI_NUMA_PROXIMITIES_ITER_INIT;
	numa_id_t buf;
	numa_nodes_count = 0;
	// Initialize nodes
	while (acpi_numa_enumerate_at_boot(&iter, &buf)) {
		if (!numa_nodes[buf].initialized) {
			numa_nodes[buf].initialized = true;
			numa_nodes[buf].slab_data = MEM_HEAP_SLAB_DATA_INIT;
			numa_nodes[buf].lock = THREAD_SPINLOCK_INIT;
			numa_nodes[buf].ranges = NULL;
			numa_nodes_count++;
		}
	}
	// Initialize neighbour lists
	for (numa_id_t i = 0; i < numa_nodes_count; ++i) {
		if (numa_nodes[i].initialized) {
			numa_init_neighbour_list(i);
		}
	}
	numa_dump_nodes();
}
