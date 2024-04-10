#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "kernel/util/string/string.h"

#include "kernel/include/ctype.h"
#include "kernel/util/vsprintf.h"
#include "kernel/fs/vfs.h"
#include "kernel/util/debug.h"
#include "kernel/devices/kybrd.h"
#include "kernel/proc/task.h"
#include "kernel/fs/poll.h"
#include "kernel/system/sysapi.h"
#include "kernel/util/math.h"
#include "kernel/include/fcntl.h"
#include "kernel/cpu/hal.h"
#include "kernel/memory/kernel_info.h"
#include "kernel/devices/terminal.h"

static const size_t VGA_WIDTH = 80;
static const size_t VGA_HEIGHT = 25;
static uint16_t* const VGA_MEMORY = (uint16_t*)(0xB8000);
static const size_t VIDEO_MEMORY_SIZE = VGA_WIDTH * VGA_HEIGHT << 2;
static uint16_t BLANK;
static size_t video_memory_index;
static size_t terminal_row;
static size_t terminal_column;
static uint8_t terminal_color;
static uint16_t* terminal_buffer;
// static uint16_t video_memory[VIDEO_MEMORY_SIZE];

void terminal_set_color(enum vga_color color) {
  terminal_color = vga_entry_color(color, VGA_COLOR_BLACK);
}

void terminal_reset_color() {
  terminal_color = vga_entry_color(VGA_COLOR_GREEN, VGA_COLOR_BLACK);
}

void disable_cursor() {
  outportb(0x3D4, 0x0A);
  outportb(0x3D5, 0x20);
}

void enable_cursor(uint8_t cursor_start, uint8_t cursor_end) {
  outportb(0x3D4, 0x0A);
  outportb(0x3D5, (inportb(0x3D5) & 0xC0) | cursor_start);

  outportb(0x3D4, 0x0B);
  outportb(0x3D5, (inportb(0x3D5) & 0xE0) | cursor_end);
}

void update_cursor(int x, int y) {
  uint16_t pos = y * VGA_WIDTH + x;

  outportb(0x3D4, 0x0F);
  outportb(0x3D5, (uint8_t)(pos & 0xFF));
  outportb(0x3D4, 0x0E);
  outportb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

uint16_t get_cursor_position(void) {
  uint16_t pos = 0;
  outportb(0x3D4, 0x0F);
  pos |= inportb(0x3D5);
  outportb(0x3D4, 0x0E);
  pos |= ((uint16_t)inportb(0x3D5)) << 8;
  return pos;
}

void clrscr() {
  terminal_row = 0;
  terminal_column = 0;
  for (size_t y = 0; y < VGA_HEIGHT; y++) {
    for (size_t x = 0; x < VGA_WIDTH; x++) {
      const size_t index = y * VGA_WIDTH + x;
      terminal_buffer[index] = BLANK;
    }
  }
  update_cursor(terminal_column, terminal_row);
}

void terminal_clrline() {
  for (size_t x = 0; x < VGA_WIDTH; x++) {
    const size_t index = terminal_row * VGA_WIDTH + x;
    terminal_buffer[index] = BLANK;
  }
  terminal_column = 0;
  update_cursor(terminal_column, terminal_row);
}

void terminal_initialize(void) {
  enable_cursor(13, 13);
  
  video_memory_index = 0;
  terminal_color = vga_entry_color(VGA_COLOR_GREEN, VGA_COLOR_BLACK);
  BLANK = vga_entry(' ', terminal_color);

  terminal_buffer = VGA_MEMORY;

  clrscr();

  /*
  for (size_t i = 0; i < VIDEO_MEMORY_SIZE; ++i) {
    video_memory[i] = 0;
  }
  */
}

void terminal_setcolor(uint8_t color) {
  terminal_color = color;
}

void terminal_putentryat(unsigned char c, uint8_t color, size_t x, size_t y) {
  const size_t index = y * VGA_WIDTH + x;
  terminal_buffer[index] = vga_entry(c, color);
  // video_memory[video_memory_index] = terminal_buffer[index];
}

void scroll() {
  memmove(terminal_buffer, terminal_buffer + VGA_WIDTH, sizeof(uint16_t) * (VGA_WIDTH * (VGA_HEIGHT - 1)));

  for (int i = 0; i < VGA_WIDTH; ++i) {
    terminal_buffer[(VGA_HEIGHT - 1) * VGA_WIDTH + i] = BLANK;
  }

  terminal_row = VGA_HEIGHT - 1;
  terminal_column = 0;
}

void terminal_newline() {
  terminal_column = 0;
  terminal_row++;
  if (terminal_row == VGA_HEIGHT) {
    scroll();
  }
  update_cursor(terminal_column, terminal_row);
}

void terminal_popchar() {
  if (terminal_column == 0 && terminal_row == 0) {
    return;
  }

  do {
    if (terminal_column == 0 && terminal_row != 0) {
      terminal_row--;
      terminal_column = VGA_WIDTH;
    } else {
      terminal_column--;
    }
  } while (terminal_buffer[terminal_row * VGA_WIDTH + terminal_column] == BLANK);

  terminal_buffer[terminal_row * VGA_WIDTH + terminal_column] = BLANK;
  update_cursor(terminal_column, terminal_row);
}

void terminal_clrscr() {
  clrscr();
}

void terminal_putchar(char c) {
  const bool is_special = c == '\n';

  switch (c) {
    case '\n': {
      terminal_newline();
      break;
    }

    default: {

      if (terminal_column == VGA_WIDTH) {
        terminal_column = 0;
        terminal_row++;
        if (terminal_row == VGA_HEIGHT) {
          scroll();
        }
      }

      unsigned char uc = c;
      terminal_putentryat(uc, terminal_color, terminal_column, terminal_row);
      terminal_column++;
    }
  }
}

void terminal_write(const char* data, size_t size) {
  for (size_t i = 0; i < size; i++)
    terminal_putchar(data[i]);

  // It would be faster to update cursor after printing an entire string.
  update_cursor(terminal_column, terminal_row);
}

void terminal_writestring(const char* data) {
  terminal_write(data, strlen(data));
}

extern void shell_start();

void terminal_run() {
  log("terminal: initializing");
  struct process *parent = get_current_process();
  
  terminal_initialize();
  int master_fd = 0;
  // returns fd of a master terminal
  if ((master_fd = vfs_open("/dev/ptmx", O_RDONLY)) < 0) {
    err("Unable to open ptmx");
  }
  
  dup2(master_fd, stdin);
  dup2(master_fd, stdout);
  
  int err_fd;
  if ((err_fd = vfs_open("/dev/serial0", O_WRONLY)) < 0) {
    err("Cannot open serial");
  }

  if (err_fd != stderr) {
    dup2(err_fd, stderr);
    vfs_close(err_fd);
  }

  struct key_event ev;

  int32_t id = 0;
  
  if ((id = process_spawn(parent)) == 0) {
    setpgid(0, 0);
    struct process *proc_child = get_current_process();
    
    proc_child->sid = proc_child->pid;
    proc_child->tty = NULL;
    assert(proc_child->sid == proc_child->gid && proc_child->gid == proc_child->pid);

    int slave_fd;
    if ((slave_fd = vfs_open("/dev/pts/0", O_RDONLY)) < 0) {
      err("Cannot open slave tty");
    }

    dup2(slave_fd, stdin);
    dup2(slave_fd, stdout);
    dup2(slave_fd, stderr);
    
    
    //vfs_close(stdin);
    //vfs_close(stdout);
    //vfs_close(stderr);
    
    if (slave_fd != stdin || slave_fd != stdout) {
      vfs_close(slave_fd);
    }
    /*
    int error_fd;
    vfs_close(stderr);
    if ((error_fd = vfs_open("/dev/serial0", O_RDONLY)) < 0) {
      err("Cannot open serial0");
    }
    assert(error_fd == stderr);
    */

    proc_child->name = strdup("shell");

    // dirty way to load a program in a userspace
    proc_child->va_dir = vmm_create_address_space();
    proc_child->pa_dir = vmm_get_physical_address(proc_child->va_dir, false); 
    pmm_load_PDBR(proc_child->pa_dir);
    char* argv[] = {};
    char* envp[] = { "ENV1=/etc/profile", "HOME=/" };
    process_execve("/bin/dash", &argv, &envp);
    
    //shell_start();
    assert_not_reached();
  }
  setpgid(id, id);
  
  int size = 20;
  char buf[size + 1];

  int kybrd_fd;
  if ((kybrd_fd = vfs_open("/dev/input/kybrd", O_RDONLY)) < 0) {
    err("Cannot open keyboard descriptor");
  }
  
  // stdin = ptmx
  // stdout = ptmx

  while (true) {
  newline:
    char key;
    int nfds = 2;
    struct pollfd poll_fd[] = {
      {
        .fd = kybrd_fd,
        .events = POLLIN,
        .revents = 0
      },
      {
        .fd = stdin,
        .events = POLLIN,
        .revents = 0
      }
    };
    
    do_poll(poll_fd, nfds, NULL);

    for (int i = 0; i < nfds; ++i) {
      if (poll_fd[i].events & poll_fd[i].revents) {
        if (poll_fd[i].fd == kybrd_fd) {
          struct key_event ev;
          vfs_fread(poll_fd[i].fd, &ev, sizeof(struct key_event));

          if (ev.type == KEY_RELEASE) {
            buf[0] = '\0';
            continue;
          }
          
          char c = kkybrd_key_to_ascii(ev.key);
          buf[0] = c;
          buf[1] = '\0';

          if (c == 13) {
            int a = 123;
          }

          vfs_fwrite(stdout, buf, 1);
        } else {
          int read = vfs_fread(poll_fd[i].fd, buf, size);
          if (read > 0) {
            buf[read] = '\0';
          } else {
            buf[0] = '\0';
            continue;
          }
        }
      }
    }


    int i = 0;
    while (i < size && buf[i] != '\0') {
      key = buf[i];
      
      switch (key) {
        case '\e':
          char command[10];
          // TODO: very dirty implementation 
          int command_index = 0;
          while (buf[i] != 'm' ) {
            command[command_index] = buf[i];
            ++i;
            ++command_index;
            if (i >= size || buf[i] == '\0') { // read more
              kreadline(&buf, size);
              i = 0;
            }
          }
          
          command[command_index] = 'm';
          command[command_index + 1] = '\0';

          int com_size = strlen(command);

          if (memcmp(&command, COLOR_RESET, com_size) == 0) {
            terminal_reset_color();
            break;
          }
          
          enum vga_color color = VGA_COLOR_GREEN;

          if (memcmp(&command, BLK, com_size) == 0) {
            color = VGA_COLOR_BLACK;
          } else if (memcmp(&command, RED, com_size) == 0) {
            color = VGA_COLOR_LIGHT_RED;
          } else if (memcmp(&command, GRN, com_size) == 0) {
            color = VGA_COLOR_GREEN;
          } else if (memcmp(&command, YEL, com_size) == 0) {
            //color = VGA_COLOR_
          } else if (memcmp(&command, BLU, com_size) == 0) {
            color = VGA_COLOR_LIGHT_BLUE;
          } else if (memcmp(&command, MAG, com_size) == 0) {
            color = VGA_COLOR_MAGENTA;
          } else if (memcmp(&command, CYN, com_size) == 0) {
            color = VGA_COLOR_CYAN;
          } else if (memcmp(&command, WHT, com_size) == 0) {
            color = VGA_COLOR_WHITE;
          } else {
            err("terminal: nvalid terminal code, ignoring");
          }

          terminal_set_color(color);
            
          break;

        case '\n':
          terminal_newline();
          break;
        case '\0':
          goto newline;
          break;
        case KEY_BACKSPACE:
        case '\177':
          terminal_popchar();
          break;

        case 12:
          terminal_clrscr();
          break;
        
        case KEY_RETURN:
        case KEY_UNKNOWN:
          break;
        default:
          char c = kkybrd_key_to_ascii(key);
          //log("print %c", c);
          if (isascii(c)) {
            terminal_write(&c, 1);
          }
          break;
      }
      ++i;
    }
  }
}
