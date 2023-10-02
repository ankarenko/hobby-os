#ifndef INCLUDE_LIST_H
#define INCLUDE_LIST_H

#include <stdbool.h>

#define LIST_POISON1 0
#define LIST_POISON2 0

/*
  good explanantion how this linked list works
  https://medium.com/@414apache/kernel-data-structures-linkedlist-b13e4f8de4bf
*/

struct list_head {
	struct list_head *next, *prev;
};

#define LIST_HEAD_INIT(name) { &(name), &(name) } 
#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)

static inline void INIT_LIST_HEAD(struct list_head *list) { 
  list->next = list; 
  list->prev = list; 
} 

#define LIST_HEAD(name) \ 
  struct list_head name = LIST_HEAD_INIT(name) 

/* 
  * Insert a new entry between two known consecutive entries. 
  * 
  * This is only for internal list manipulation where we know 
  * the prev/next entries already! 
*/ 
 
static inline void __list_add(
  struct list_head *item, 
  struct list_head *prev, 
  struct list_head *next
) { 
  next->prev = item; 
  item->next = next; 
  item->prev = prev; 
  prev->next = item; 
} 

/** 
  * list_add - add a new entry 
  * @new: new entry to be added 
  * @head: list head to add it after 
  * 
  * Insert a new entry after the specified head. 
  * This is good for implementing stacks. 
*/ 
static inline void list_add(struct list_head *item, struct list_head *head) { 
  __list_add(item, head, head->next); 
}

/**
 * list_add_tail - add a new entry
 * @new: new entry to be added
 * @head: list head to add it before
 *
 * Insert a new entry before the specified head.
 * This is useful for implementing queues.
 */
static inline void list_add_tail(struct list_head *item, struct list_head *head) {
	__list_add(item, head->prev, head);
}

/*
 * Delete a list entry by making the prev/next entries
 * point to each other.
 *
 * This is only for internal list manipulation where we know
 * the prev/next entries already!
 */
static inline void __list_del(struct list_head *prev, struct list_head *next) {
	next->prev = prev;
	prev->next = next;
}

static inline bool __list_del_entry_valid(struct list_head *entry) {
	return entry->next != LIST_POISON1 && entry->prev != LIST_POISON2;
}

/**
 * list_empty - tests whether a list is empty
 * @head: the list to test.
 */
static inline int list_empty(const struct list_head *head) {
	return head->next == head;
}

/**
 * list_del - deletes entry from list.
 * @entry: the element to delete from the list.
 * Note: list_empty() on entry does not return true after this, the entry is
 * in an undefined state.
 */
static inline void __list_del_entry(struct list_head *entry)
{
	if (!__list_del_entry_valid(entry))
		return;

	__list_del(entry->prev, entry->next);
}

static inline void list_del(struct list_head *entry) {
	__list_del_entry(entry);
	entry->next = LIST_POISON1;
	entry->prev = LIST_POISON2;
}

/**
  * list_for_each        -       iterate over a list 
  * @pos:        the &struct list_head to use as a loop counter. 
  * @head:       the head for your list. 
*/ 
#define list_for_each(pos, head) \ 
  for (pos = (head)->next; pos != (head); pos = pos->next)

/**
 * list_for_each_prev	-	iterate over a list backwards
 * @pos:	the &struct list_head to use as a loop cursor.
 * @head:	the head for your list.
 */
#define list_for_each_prev(pos, head) \
	for (pos = (head)->prev; pos != (head); pos = pos->prev)

/** 
  * list_entry - get the struct for this entry 
  * @ptr:        the &struct list_head pointer. 
  * @type:       the type of the struct this is embedded in. 
  * @member:     the name of the list_struct within the struct. 
*/ 
#define list_entry(ptr, type, member) \ 
  container_of(ptr, type, member)

/** 
  * container_of - cast a member of a structure out to the containing structure 
  * @ptr:        the pointer to the member. 
  * @type:       the type of the container struct this is embedded in. 
  * @member:     the name of the member within the struct. 
  * 
*/ 
#define container_of(ptr, type, member) ({                \ 
  const typeof( ((type *)0)->member ) *__mptr = (ptr);    \ 
  (type *)((char *)__mptr - offsetof(type,member) );}) 


/** 
  * list_for_each_entry  -       iterate over list of given type 
  * @pos:        the type * to use as a loop counter. 
  * @head:       the head for your list. 
  * @member:     the name of the list_struct within the struct. 
*/ 
#define list_for_each_entry(pos, head, member)                    \ 
  for (pos = list_entry((head)->next, typeof(*pos), member);      \ 
    &pos->member != (head);                                       \ 
    pos = list_entry(pos->member.next, typeof(*pos), member)) 

/**
 * list_for_each_entry_reverse - iterate backwards over list of given type.
 * @pos:	the type * to use as a loop cursor.
 * @head:	the head for your list.
 * @member:	the name of the list_head within the struct.
 */
#define list_for_each_entry_reverse(pos, head, member)      \
	for (pos = list_last_entry(head, typeof(*pos), member); \
		 &pos->member != (head);                            \
		 pos = list_prev_entry(pos, member))

static inline int list_count(const struct list_head *head) {
	int count = 0;
	struct list_head *iter;
	list_for_each(iter, head)
		count++;
	return count;
}

#endif