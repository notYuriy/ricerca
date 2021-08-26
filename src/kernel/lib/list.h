//! @file list.h
//! @brief File containing intrusive list functions

#pragma once

#include <lib/containerof.h>
#include <lib/panic.h>
#include <stddef.h>

//! @brief List node
struct list_node {
	//! @brief Next node
	struct list_node *next;
	//! @brief Previous node
	struct list_node *prev;
};

//! @brief List (list_append_head -> head <-----> tail <- list_append_tail)
struct list {
	//! @brief Head node
	struct list_node *head;
	//! @brief Tail node
	struct list_node *tail;
};

//! @brief List static initializer
#define LIST_INIT                                                                                  \
	(struct list) {                                                                                \
		.head = NULL, .tail = NULL                                                                 \
	}

//! @brief Append node before head
//! @param list Pointer to the list
//! @param node Node to append
static inline void list_append_head(struct list *list, struct list_node *node) {
	node->prev = NULL;
	if (list->head == NULL) {
		node->next = NULL;
		list->head = list->tail = node;
	} else {
		list->head->prev = node;
		node->next = list->head;
		list->head = node;
	}
}

//! @brief Append node after tail
//! @param list Pointer to the list
//! @param node Node to append
static inline void list_append_tail(struct list *list, struct list_node *node) {
	node->next = NULL;
	if (list->tail == NULL) {
		node->prev = NULL;
		list->head = list->tail = node;
	} else {
		list->tail->next = node;
		node->prev = list->tail;
		list->tail = node;
	}
}

//! @brief Remove head
//! @param list Pointer to the list
//! @return head or NULL if list is empty
static inline struct list_node *list_remove_head(struct list *list) {
	struct list_node *result = list->head;
	if (result == NULL) {
		return NULL;
	} else if (result->next == NULL) {
		list->head = list->tail = NULL;
	} else {
		list->head->next->prev = NULL;
		list->head = list->head->next;
	}
	return result;
}

//! @brief Remove tail
//! @param list Pointer to the list
//! @return tail or NULL if list is empty
static inline struct list_node *list_remove_tail(struct list *list) {
	struct list_node *result = list->tail;
	if (result == NULL) {
		return NULL;
	} else if (result->prev == NULL) {
		list->head = list->tail = NULL;
	} else {
		list->tail->prev->next = NULL;
		list->tail = list->tail->prev;
	}
	return result;
}

//! @brief Remove node from the list
//! @param list Pointer to the list
//! @param node Node to remove
static inline void list_remove(struct list *list, struct list_node *node) {
	if (list->head == node) {
		list_remove_head(list);
	} else if (list->tail == node) {
		list_remove_tail(list);
	} else {
		node->prev->next = node->next;
		node->next->prev = node->prev;
	}
}

//! @brief Generic LIST_APPEND_HEAD macro
//! @param l Pointer to the list
//! @param node Pointer to the node
//! @param hook Name of the member inside node struct that points to list_node
#define LIST_APPEND_HEAD(l, node, hook) list_append_head(l, &((node)->hook))

//! @brief Generic LIST_APPEND_TAIL macro
//! @param l Pointer to the list
//! @param node Pointer to the node
//! @param hook Name of the member inside node struct that points to list_node
#define LIST_APPEND_TAIL(l, node, hook) list_append_tail(l, &((node)->hook))

//! @brief Generic LIST_REMOVE_HEAD macro
//! @param l Pointer to the list
//! @param T Node type
//! @param hook Name of the member inside node struct that points to list_node
#define LIST_REMOVE_HEAD(l, T, hook) CONTAINER_OF_NULLABLE(list_remove_head(l), T, hook)

//! @brief Generic LIST_REMOVE_TAIL macro
//! @param l Pointer to the list
//! @param T Node type
//! @param hook Name of the member inside node struct that points to list_node
#define LIST_REMOVE_TAIL(l, T, hook) CONTAINER_OF_NULLABLE(list_remove_tail(l), T, hook)

//! @brief Generic LIST_REMOVE macro
//! @param l Pointer to the list
//! @param node Pointer to the node
//! @param hook Name of the member inside node struct that points to list_node
#define LIST_REMOVE(l, node, hook) list_remove(l, &((node)->hook))

//! @brief List iterator
//! @param l Pointer to the list
//! @param T Node type
//! @param hook Name of the member inside node struct that points to list_node
//! @param name Name of the variable that will be used to store pointer to the node
#define LIST_FOREACH(l, T, hook, name)                                                             \
	for (T *name = CONTAINER_OF_NULLABLE(l->first, T, hook); name != NULL;                         \
	     name = CONTAINER_OF_NULLABLE(name->hook.next, T, hook))
