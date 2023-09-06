#include <test/greatest.h>
#include "./pmm.h"
#include "./malloc.h"
#include "./vmm.h"

TEST TEST_PMM(void) {
  uint32_t frames_total = pmm_get_free_frame_count();

  uint8_t* frames = pmm_alloc_frame();
  ASSERT_EQ(frames_total - 1, pmm_get_free_frame_count());
  pmm_free_frame(frames);
  ASSERT_EQ(frames_total, pmm_get_free_frame_count());

  frames = pmm_alloc_frames(100);
  ASSERT_EQ(frames_total - 100, pmm_get_free_frame_count());
  pmm_free_frames(frames, 100);
  ASSERT_EQ(frames_total, pmm_get_free_frame_count());

  // is not possible to allocate more than exists
  frames = pmm_alloc_frames(frames_total + 1);
  ASSERT_EQ(frames, NULL);
  ASSERT_EQ(pmm_get_free_frame_count(), frames_total);

  frames = pmm_alloc_frames(frames_total);
  ASSERT_EQ(pmm_get_free_frame_count(), 0);
  pmm_free_frames(frames, frames_total);
  ASSERT_EQ(pmm_get_free_frame_count(), frames_total);

  frames = pmm_alloc_frames(frames_total - 1);
  ASSERT_EQ(pmm_get_free_frame_count(), 1);
  uint8_t* last_frame = pmm_alloc_frame();
  ASSERT_EQ((uint32_t)last_frame % PMM_FRAME_SIZE, 0);  // PMM_FRAME_SIZE ALIGNED
  uint32_t as = pmm_get_free_frame_count();
  ASSERT_EQ(pmm_get_free_frame_count(), 0);
  ASSERT_EQ(pmm_alloc_frame(), NULL); // all memory is used
  ASSERT_EQ(pmm_alloc_frames(2), NULL);

  pmm_free_frame(last_frame);
  ASSERT_EQ(pmm_get_free_frame_count(), 1);
  pmm_free_frames(frames, frames_total - 1);
  ASSERT_EQ(pmm_get_free_frame_count(), frames_total);
  
  PASS();
}

TEST TEST_KMALLOC() {
  struct pdirectory* pd = vmm_get_directory();  
  uint32_t frames_total = pmm_get_free_frame_count();
  uint8_t* block1 = kmalloc(PMM_FRAME_SIZE - sizeof(struct block_meta));


  //ASSERT_EQ(pd_entry_is_present(pd->m_entries[PAGE_DIRECTORY_INDEX((uint32_t)block1)]), true);
  
  uint32_t size_after = pmm_get_free_frame_count();
  ASSERT_EQ(pmm_get_free_frame_count(), frames_total - 1);
  
  // allocating the same block
  uint8_t* block2 = kmalloc(1);
  ASSERT_EQ(pmm_get_free_frame_count(), frames_total - 2);

  // bitmap does not get freed after kfree, it just sets 
  // flag as "free" so it can be used next time
  kfree(block2);
  ASSERT_EQ(pmm_get_free_frame_count(), frames_total - 2);
  // allocating another block
  //allocated = kmalloc(PMM_FRAME_SIZE - * sizeof(struct block_meta));
  //ASSERT_EQ(pmm_get_free_frame_count(), frames_total - 1);
  PASS();
}

SUITE(SUITE_PMM) {
  RUN_TEST(TEST_PMM);
}

// requires virtual memory enabled
SUITE(SUITE_MALLOC) {
  RUN_TEST(TEST_KMALLOC);
}
