/*
  TODO: very bad written code, needs to be rewritten
*/
#include <math.h>
#include <list.h>

#include "kernel/proc/task.h"
#include "kernel/memory/vmm.h"
#include "kernel/memory/pmm.h"
#include "kernel/util/debug.h"

// addr supposed to be outside of any vm_area
unit_static vm_area_struct* get_unmapped_area(
  mm_struct_mos *mm, uint32_t addr, uint32_t len
) {
  vm_area_struct *vma = kcalloc(1, sizeof(vm_area_struct));
  vma->vm_mm = mm;

  if (!addr || addr < mm->end_brk) {
    addr = max(mm->free_area_cache, mm->end_brk);
  }

  // assert(addr == PAGE_ALIGN(addr));
  len = PAGE_ALIGN(len);

  uint32_t found_addr = addr;
  if (list_empty(&mm->mmap)) {
    list_add(&vma->vm_sibling, &mm->mmap); // just add it if list empty
  } else {
    vm_area_struct *iter = NULL;
    list_for_each_entry(iter, &mm->mmap, vm_sibling) {
      vm_area_struct *next = list_next_entry(iter, vm_sibling);
      bool is_last_entry = list_is_last(&iter->vm_sibling, &mm->mmap);

      if (addr + len <= iter->vm_start) {
        list_add(&vma->vm_sibling, &mm->mmap);  // can fit before the beggining
        break;
      } else if (  // can fit between two areas
          addr >= iter->vm_end &&
          (is_last_entry ||
           (!is_last_entry && addr + len <= next->vm_start))) {
        list_add(&vma->vm_sibling, &iter->vm_sibling);  // add right after "iter"
        break;
      } else if ( // can fit if adjust the address accordingly
          !is_last_entry &&
          (iter->vm_end <= addr && addr < next->vm_start) &&
          next->vm_start - iter->vm_end >= len) {
        list_add(&vma->vm_sibling, &iter->vm_sibling);
        found_addr = next->vm_start - len;
        break;
      }
    }
  }

  if (found_addr) {
    vma->vm_start = found_addr;
    vma->vm_end = found_addr + len;
    mm->free_area_cache = vma->vm_end;
  }

  return vma;
}

unit_static int expand_area(
  mm_struct_mos* mm, 
  vm_area_struct *vma, 
  uint32_t address, 
  bool fixed
) {
  address = PAGE_ALIGN(address);
  if (address <= vma->vm_end)
    return 0;

  if (list_is_last(&vma->vm_sibling, &mm->mmap))
    vma->vm_end = address;
  else {
    vm_area_struct *next = list_next_entry(vma, vm_sibling);
    if (address <= next->vm_start)
      vma->vm_end = address;
    else { // if we overlay a different region 
      LOG("Region overlay", NULL);
      KASSERT(false);

      KASSERT(!fixed);
      list_del(&vma->vm_sibling);
      // BAD CODE: first he adds element to the list and then deletes it 
      vm_area_struct *vma_expand = get_unmapped_area(
        mm, 0, address - vma->vm_start
      );
      memcpy(vma, vma_expand, sizeof(vm_area_struct));
      kfree(vma_expand);
    }
  }
  return 0;
}

static struct vm_area_struct *find_vma(mm_struct_mos *mm, uint32_t addr) {
  vm_area_struct *iter = NULL;
  list_for_each_entry(iter, &mm->mmap, vm_sibling) {
    if (iter->vm_start <= addr && addr < iter->vm_end) {
      return iter;
    }
  }

  return NULL;
}

virtual_addr do_mmap(
  uint32_t addr,
  size_t len, 
  uint32_t prot,
  uint32_t flag, 
  int32_t fd
) {
  
  /*
    for now it only supports ZERRO addresses, which means
    kernel decides on his own where allocate address.
  */
  //KASSERT(addr == 0 && flag != MMAP_FIXED);
  
  // struct vfs_file *file = fd >= 0 ? current_process->files->fd[fd] : NULL;
  mm_struct_mos* mm = get_current_process()->mm_mos;
  uint32_t aligned_addr = ALIGN_DOWN(addr, PMM_FRAME_SIZE);
  vm_area_struct *vma = find_vma(mm, aligned_addr);

  if (!vma)
    vma = get_unmapped_area(mm, aligned_addr, len);
  else if (vma->vm_end < addr + len)
    expand_area(mm, vma, addr + len, true);

  /*
    if (file)
    {
            file->f_op->mmap(file, vma);
            vma->vm_file = file;
    }
    else
  */

  /*
  uint32_t frames = div_ceil(vma->vm_end - vma->vm_start, PMM_FRAME_SIZE); 
  uint32_t paddr = (uint32_t)pmm_alloc_frames(frames);
  */

  if (flag != MMAP_DONT_ALLOCATE) {
    for (
      virtual_addr vaddr = vma->vm_start; 
      vaddr < vma->vm_end; 
      vaddr += PMM_FRAME_SIZE
    ) {
      physical_addr paddr = (physical_addr)pmm_alloc_frame();
      vmm_map_address(vaddr, paddr, I86_PTE_PRESENT | I86_PTE_WRITABLE | I86_PTE_USER);
    }
  }

  return addr ? addr : vma->vm_start;
}

// FIXME: MQ 2019-01-16 Currently, we assume that start_brk is not changed
virtual_addr do_brk(virtual_addr addr, size_t len) {
  mm_struct_mos *mm = get_current_process()->mm_mos;
  vm_area_struct *vma = find_vma(mm, addr);
  uint32_t new_brk = PAGE_ALIGN(addr + len);
  mm->brk = new_brk;

  if (!vma || new_brk <= vma->vm_end)
    return 0;

  vm_area_struct *new_vma = kcalloc(1, sizeof(vm_area_struct));
  memcpy(new_vma, vma, sizeof(vm_area_struct));

  if (new_brk > mm->brk)
    expand_area(mm, new_vma, new_brk, true);
  else
    new_vma->vm_end = new_brk;

  /*
    if (vma->vm_file)
            vma->vm_file->f_op->mmap(vma->vm_file, new_vma);
    else
    {
  */
  if (vma->vm_end < new_vma->vm_end) {
    KASSERT((new_vma->vm_end - vma->vm_end) % PMM_FRAME_SIZE == 0);

    uint32_t nframes = (new_vma->vm_end - vma->vm_end) / PMM_FRAME_SIZE;
    physical_addr paddr = (physical_addr)pmm_alloc_frames(nframes);
    for (uint32_t vaddr = vma->vm_end; vaddr < new_vma->vm_end; vaddr += PMM_FRAME_SIZE, paddr += PMM_FRAME_SIZE)
      vmm_map_address(vaddr, paddr, I86_PTE_PRESENT | I86_PTE_WRITABLE | I86_PTE_USER);
  } else if (new_vma->vm_end < vma->vm_end)
    for (uint32_t addr = new_vma->vm_end; addr < vma->vm_end; addr += PMM_FRAME_SIZE)
      vmm_unmap_address(addr);

  memcpy(vma, new_vma, sizeof(vm_area_struct));

  return 0;
}
