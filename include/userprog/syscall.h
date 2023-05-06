#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

void syscall_init(void);
/* project2 */
void check_address(const void *addr);
struct lock filesys_lock;
/* project2 */

#endif /* userprog/syscall.h */
