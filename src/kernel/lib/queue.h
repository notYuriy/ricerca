//! @file queue.h
//! @brief Implementation of intrusive queue data structure

#pragma once

#include <lib/containerof.h>
#include <misc/types.h>

//! @brief Queue node
struct queue_node {
	//! @brief Next queue node
	struct queue_node *next;
};

//! @brief Queue
struct queue {
	//! @brief Queue head
	struct queue_node *head;
	//! @brief Queue tail
	struct queue_node *tail;
};

//! @brief Queue initializer
#define QUEUE_INIT                                                                                 \
	(struct queue) {                                                                               \
		0, 0                                                                                       \
	}

//! @brief Enqueue
//! @param queue Pointer to the queue
//! @param node Node to insert
static inline void queue_enqueue(struct queue *queue, struct queue_node *node) {
	node->next = NULL;
	if (queue->head == NULL) {
		queue->head = queue->tail = node;
	} else {
		queue->tail->next = node;
		queue->tail = node;
	}
}

//! @brief Dequeue
//! @param Pointer to the queue
//! @return Dequeued node or NULL otherwise
static inline struct queue_node *queue_dequeue(struct queue *queue) {
	if (queue->head == NULL) {
		return NULL;
	}
	struct queue_node *result = queue->head;
	if (queue->head->next == NULL) {
		queue->head = queue->tail = NULL;
	} else {
		queue->head = queue->head->next;
	}
	return result;
}

//! @brief Generic enqueue macro
//! @param queue Pointer to the queue
//! @param node Pointer to the node
//! @param hook Name of the member inside node struct that points to queue_node
#define QUEUE_ENQUEUE(queue, node, hook) queue_enqueue(queue, &((node)->hook))

//! @brief Generic dequeue macro
//! @param queue Pointer to the queue
//! @param T Node struct type
//! @param hook Name of the member inside node struct that points to queue_node
#define QUEUE_DEQUEUE(queue, T, hook) CONTAINER_OF_NULLABLE(queue_dequeue(queue), T, hook)
