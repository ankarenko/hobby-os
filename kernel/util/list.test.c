#include <stdint.h>
#include <test/greatest.h>

#include "kernel/include/list.h"

// make sure data was not corrupted
#define TEST_MAGIC 14235343

struct just_item {
	int data;
  int magic;
  struct list_head list; /* kernel's list structure */	
};

TEST TEST_LIST(void) {
  int items_size = 10;
  struct just_item items[items_size];
  LIST_HEAD(items_head);

  for (int i = 0; i < items_size; ++i) {
    items[i].data = i;
    items[i].magic = TEST_MAGIC;
    INIT_LIST_HEAD(&items[i].list);
    list_add_tail(&items[i].list, &items_head);
  }

  ASSERT_EQ(list_count(&items_head), items_size);

  struct just_item* item = NULL ; 
  for (int i = 0; i < items_size; ++i) {
    if (items[i].data % 2 == 0) {
      list_del(&items[i].list);
    }
  }

  ASSERT_EQ(list_count(&items_head), items_size / 2);

  list_for_each_entry(item, &items_head, list) { 
    ASSERT_EQ(item->data % 2, 1); 
  }

  list_add_tail(&items[2].list, &items_head);
  uint32_t count = list_count(&items_head);
  ASSERT_EQ(count, items_size / 2 + 1);
  list_del(&items[2].list);

  list_for_each_entry(item, &items_head, list) { 
    ASSERT_EQ(item->data % 2, 1); 
    ASSERT_EQ(items->magic, TEST_MAGIC);
  }

  PASS();
  
  /*  
  struct list_head* pos; 
  list_for_each(pos, &items_head) { 
    printf("surfing the linked list next = %x and prev = %x\n" , 
      pos->next, 
      pos->prev 
    );
  }

  struct just_item* ret = list_entry(items_head.prev, typeof(*ret), list);
  printf("erased: %d\n", ret->data);

  struct just_item* item = NULL ; 
  list_for_each_entry(item, &items_head, list) { 
    printf("data  =  %d\n" , item->data); 
  }
  */
}

SUITE(SUITE_LIST) {
  RUN_TEST(TEST_LIST);
}