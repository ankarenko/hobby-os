#include "kernel/include/list.h"
#include "kernel/util/string/string.h"

#include "kernel/fs/vfs.h"

// TODO: optimization: use red-black trees
struct list_head all_dentries;

int vfs_cache(struct vfs_dentry* dentry) {
  list_add_tail(&dentry->d_cache_sibling, &all_dentries);
}

int vfs_cache_remove(struct vfs_dentry* dentry) {
  list_del(&dentry->d_cache_sibling);  
}

struct vfs_dentry* vfs_cache_get(struct vfs_dentry *parent, char *name) {
  struct vfs_dentry* iter;
  list_for_each_entry(iter, &all_dentries, d_cache_sibling) {
    if (iter->d_parent == parent && strcmp(name, iter->d_name)) {
      return iter;
    }
  }
  return NULL;
}

void vfs_cache_init() {
  INIT_LIST_HEAD(&all_dentries);
}