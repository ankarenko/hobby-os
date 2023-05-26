#include <string.h>
#include "fsys.h"

#define DEVICE_MAX 26

//! File system list
PFILESYSTEM _file_systems[DEVICE_MAX];

/**
 *	Opens a file
 */
FILE vol_open_file(const char* fname) {
  if (fname) {
    //! default to device 'a'
    unsigned char device = 'a';

    //! filename
    char* filename = (char*)fname;

    //! in all cases, if fname[1]==':' then the first character must be device letter
    //! FIXME: Using fname[2] do to BUG 2. Please see main.cpp for info
    if (fname[2] == ':') {
      device = fname[0];
      filename += 3;  // strip it from pathname
    }

    //! call filesystem
    if (_file_systems[device - 'a']) {
      //! set volume specific information and return file
      FILE file = _file_systems[device - 'a']->open(filename);
      file.device_id = device;
      return file;
    }
  }

  //! return invalid file
  FILE file;
  file.flags = FS_INVALID;
  return file;
}

/**
 *	Reads file
 */
void vol_read_file(PFILE file, uint8_t* buffer, uint32_t length) {
  if (file)
    if (_file_systems[file->device_id - 'a'])
      _file_systems[file->device_id - 'a']->read(file, buffer, length);
}

/**
 *	Close file
 */
void vol_close_file(PFILE file) {
  if (file)
    if (_file_systems[file->device_id - 'a'])
      _file_systems[file->device_id - 'a']->close(file);
}

void vol_ls(PFILE file) {
  if (_file_systems[file->device_id - 'a'])
    _file_systems[file->device_id - 'a']->ls(file);
}

/**
 *	Registers a filesystem
 */
void vol_register_file_system(PFILESYSTEM fsys, uint32_t device_id) {
  static int32_t i = 0;

  if (i < DEVICE_MAX)
    if (fsys) {
      _file_systems[device_id] = fsys;
      i++;
    }
}

/**
 *	Unregister file system
 */
void vol_unregister_file_system(PFILESYSTEM fsys) {
  for (int i = 0; i < DEVICE_MAX; i++)
    if (_file_systems[i] == fsys)
      _file_systems[i] = 0;
}

/**
 *	Unregister file system
 */
void vol_unregister_file_system_by_id(uint32_t device_id) {
  if (device_id < DEVICE_MAX)
    _file_systems[device_id] = 0;
}
