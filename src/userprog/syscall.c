#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/init.h"
#include "threads/vaddr.h"
#include "devices/shutdown.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "userprog/process.h"
#include <list.h>
#include <stdlib.h>
#include "devices/input.h"
#include "threads/malloc.h"
#include "userprog/pagedir.h"
#include <string.h>
// defining the first file descriptor value (0,1 are used by STDIN AND STDOUT)
static int unique_fd = 2;


// defining syscall functions here so can be accessed below
void halt(void);
void exit (int);
bool create (const char*, unsigned);
pid_t exec(const char*);
bool remove(const char*);
int open(const char*);
int filesize(int);
int read(int, void*, unsigned);
int write(int, const void*, unsigned);
void seek(int, unsigned);
unsigned tell(int fd);
void close(int fd);

static void syscall_handler (struct intr_frame *);
static struct lock file_lock; // creating lock to prevents multiple threads accessing file functions 
static struct list file_list; //creating list storing all our file_desc open files


//check whether pointer given is in user memory
static
bool good_pointer(const void* interupt_arg){
  void*  pointer = (void*) (*((int*) interupt_arg));
  if (!is_user_vaddr(pointer)){
	return false;
  }
  else {
	return true;
  }	
}


void
syscall_init (void) 
{
  lock_init(&file_lock); //initiliasing the lock 
  list_init(&file_list); //initialising file list
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}


 
static void 
syscall_handler (struct intr_frame *f UNUSED)
{
	//general validation for the interupt stack and exiting if unsuccessful
	if(!good_pointer(f->esp+1) || !good_pointer(f->esp+2) ||  !good_pointer(f->esp+3)){
	  exit(-1);
	} 

  //switch statements works using enums, converts stack pointer to 
  //int and increments pointer to grab arguments given
  //any values returned are passed into the eax register
  switch(*(int*)f->esp){
	case SYS_HALT:{ 
	 halt();
	 break;
	}
	case SYS_EXIT:{
	 int status  = *((int*)f->esp + 1);
	 exit(status);
         break;
	}
	case SYS_EXEC:{
	 const char *cmd_line = (char*)(*((int*) f->esp +1));
         pid_t program_id = exec(cmd_line);
	 f -> eax = program_id;
	 break;
	}
	case SYS_CREATE:{
	 const char *file = (char*) (*((int*) f->esp +1));
	 unsigned initial_size = *((unsigned*)f->esp+2);
	 bool success_code = create(file, initial_size);
         f -> eax = success_code;
	 break;
	}
	case SYS_OPEN:{
	 const char *file = (char*) (*((int*) f->esp+1));
	 int file_desc = open(file);
	 f -> eax = file_desc;
	 break;
	}
	case SYS_REMOVE:{
	 const char *file = (char*) (*((int*) f->esp+1));
	 bool success_code = remove(file);
	 f -> eax = success_code;
	 break;
	}
	case SYS_FILESIZE:{
	 int fd = *((int*) f->esp +1);
	 int size_of_file =filesize(fd);
	 f -> eax = size_of_file;
	 break;
	}
	case SYS_READ:{
	 int fd = *((int*) f->esp +1);
	 void* buffer = (void*) (*((int*) f->esp + 2));
	 unsigned size = *((unsigned*)f->esp +3);
	 int bytes_read = read(fd, buffer, size);
	 f -> eax = bytes_read;
	 break;
	}
	case SYS_WRITE:{
	 int bytes_wrote;
	 int fd = *((int*) f->esp +1);
	 void* buffer= (void*) (*((int*) f->esp +2));
	 unsigned size = *((unsigned*) f->esp +3);
	 bytes_wrote = write(fd, buffer, size);
	 f->eax = bytes_wrote; 
	 break;
	}
	case SYS_SEEK:{
	int fd = *((int*) f->esp +1);
	unsigned position = *((unsigned*)f->esp +2);
	seek(fd, position);
	break;
	}
	case SYS_TELL:{
	 int fd = *((int*) f->esp +1);
	 f->eax = tell(fd);
	 break;
	}
	case SYS_CLOSE:{
	 int fd = *((int*) f->esp +1);
	 close(fd);
	 break;
	}


		
  }
 // printf ("system call!\n");
  //thread_exit ();
}
//shutdowns pintOS (use sparingly)
void halt(void){
   shutdown_power_off();
}

//exits with a given status, if any files are open they are close 
//before exiting
void exit(int status){
  struct list_elem *start;
  struct file_desc *to_remove;
  thread_current() -> exit_code = status;
  while (!list_empty (&file_list)){
        start  = list_begin (&file_list);
	to_remove = list_entry(start, struct file_desc, elem);
        close(to_remove->fd);
  }
  thread_exit();
}

//responsible for creating a file with a given name and size
//as with many syscalls uses locks to prevent threads from accessing shared
//memory at the same time
bool create(const char* file, unsigned initial_size){
  bool success_code;
  //exeception handling
  if(file == NULL || file[0] == '\0'|| strlen(file) > 40 || pagedir_get_page(thread_current()->pagedir, file) == NULL || !is_user_vaddr(file)){
	exit(-1);
  }
  lock_acquire(&file_lock);
  success_code = filesys_create(file, initial_size);
  lock_release(&file_lock); 
  return success_code;
}

//exec executes a user given executable returning the process_id
//synchronisation takes place in process.c using the order semaphore
pid_t exec(const char* cmd_line){
  pid_t process_id = -1;
  lock_acquire(&file_lock); 
  if(cmd_line != NULL || is_user_vaddr(cmd_line) || pagedir_get_page(thread_current()->pagedir, cmd_line) != NULL){ 
  //null file exception handling
	process_id = process_execute(cmd_line);
  }
  lock_release(&file_lock);
  return process_id;
}

//removing a created file
bool remove(const char* file){
  bool success_code;
  //exception handling checking whether pointer belongs to page in virtual memory
  if(pagedir_get_page(thread_current()->pagedir, file) == NULL){
	exit(-1);
  }
 lock_acquire(&file_lock);
 success_code = filesys_remove(file);
 lock_release(&file_lock);
 return success_code;
}

//opens a created file
int open(const char* file){
   //exception handling to prevent empty/null/non existent 
   //files from opening
   if(file == NULL || pagedir_get_page(thread_current()->pagedir, file) == NULL || file[0] == '\0'){
	exit(-1);
   }
   int initial_fd = -1;
   lock_acquire(&file_lock);
   //opening given file and retriving its unix structure
   struct file *fp = filesys_open(file);
   lock_release(&file_lock);
   if(fp == NULL){
	exit(-1);
   }
   struct file_desc *file_open  = malloc(sizeof(struct file_desc));
   //allocating dynamic memory for instance of file_desc structure
   lock_acquire(&file_lock);
   file_open -> fd = unique_fd;  
   //making the file descriptor = the unique_fd (starts at 2)
   unique_fd++; //incrementing file descriptor value for next file
   file_open -> file = fp;
   //making the file in the structure equal to the file we have taken into the
   // function
   list_push_front(&file_list, &file_open->elem);
   //pushing the element onto the file list created above
   initial_fd = file_open ->fd;
   lock_release(&file_lock);
 return initial_fd;
}


//initiliasing the select file function responsible for getting a file_desc 
//linked to  a user space inputted file descriptor
struct file_desc * select_file (int syscall_fd);


//Returns the file size of a file using fd
int filesize(int syscall_fd){
	//getting the file desc linked to the inputted syscall_fd
	struct file_desc *selected_file_desc = select_file(syscall_fd);
	int size;
	lock_acquire(&file_lock);
	size = file_length(selected_file_desc -> file);
	lock_release(&file_lock);
	return size;
}


struct file_desc * select_file(int syscall_fd){ 
   struct list_elem *e;
   struct file_desc *retrieved_file_desc;
   //using given double linked list iterator to get files in file_list
   for(e = list_begin (&file_list); e!= list_end (&file_list); e= list_next(e)){
	struct file_desc *f = list_entry(e, struct file_desc, elem);
	if (f -> fd == syscall_fd){
		retrieved_file_desc =  f;
		return retrieved_file_desc;
	// returning the file if the given
	// fd in interrupt stack is within a file_desc struct in
	// the file list
	}
  }
	exit(-1);	
}


int read(int fd, void* buffer, unsigned size){
   int bytes_read;
   struct file_desc* selected_file_desc = select_file(fd);
   //exception handing for bad pointer buffer
   if(!is_user_vaddr(buffer) || size <= 0 || pagedir_get_page(thread_current()->pagedir, buffer) == NULL){ 
      exit(-1); 
   }
   
   //stdin from keyboard
   if(fd ==0){	
	lock_acquire(&file_lock);
	unsigned buffer_value = 0;		
	while(buffer_value < size){
		*(uint8_t*)(buffer + buffer_value) = input_getc();
		buffer_value++;
	}
	//loop through until iterator is a big as given size
	// convert void buffer to an uint8_t as input_getc() return a
	// byte value and then go through buffer incrementally appending
	// each byte value	
	bytes_read = size;
	lock_release(&file_lock);
	}
   else{ //if file is being read then  use file_read function
      lock_acquire(&file_lock);
      bytes_read = file_read(selected_file_desc -> file, buffer, size);
      lock_release(&file_lock);
   }
 return bytes_read;
}


int write(int fd, const void* buffer, unsigned size){
   int written_bytes;
   if(fd ==1){ //writing to console 
	lock_acquire(&file_lock);
	//using given putbuf function to write to console
	putbuf(buffer, size);
	lock_release(&file_lock);
	written_bytes = size;
   }
   //checking if given size to write is 0
   else if(size == 0){
	written_bytes = 0;
   }
   else{ //writes size bytes from buffer to the write_file
	struct file_desc * write_file_desc = select_file(fd);
	//exception handling for bad pointer buffer
	if(!is_user_vaddr(buffer) || pagedir_get_page(thread_current()->pagedir, buffer) == NULL) {
		exit(-1);
	}
		
        lock_acquire(&file_lock);
	written_bytes = file_write(write_file_desc -> file, buffer, size);
	lock_release(&file_lock);
   }
   return written_bytes;		
}

//Changes the next byte to be read or written in open file
void seek(int fd, unsigned position){
   struct file_desc * seek_file_desc = select_file(fd);
   lock_acquire(&file_lock);
   file_seek(seek_file_desc->file,position);
   lock_release(&file_lock);
}

//Returns the position of the next byte to be read or written in open file fd
unsigned tell(int fd){
   unsigned next_byte;
   struct file_desc * tell_file_desc = select_file(fd);
   lock_acquire(&file_lock);
   next_byte = file_tell(tell_file_desc -> file);
   lock_release(&file_lock);
   return next_byte; 
}

//Closes a file using fd
void close(int fd){
   struct file_desc * close_file_desc = select_file(fd);
   lock_acquire(&file_lock);
   file_close(close_file_desc ->file);
   //removes file_desc element from file_list
   list_remove(&close_file_desc->elem);
   lock_release(&file_lock);
   //frees memory allocated during open
   free(close_file_desc);
}








