#include <test/greatest.h>
#include "./pmm.h"
#include "./malloc.h"

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
  uint32_t frames_total = pmm_get_free_frame_count();
  uint8_t* allocated = kmalloc(10);
  uint32_t size_after = pmm_get_free_frame_count();
  kfree(allocated);
  ASSERT_EQ(pmm_get_free_frame_count(), frames_total);
  
  PASS();
}

SUITE(SUITE_MEMORY) {
  RUN_TEST(TEST_PMM);
}

// requires virtual memory enabled
SUITE(SUITE_KMALLOC) {
  RUN_TEST(TEST_KMALLOC);
}
