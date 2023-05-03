#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"

void syscall_entry (void);
void syscall_handler (struct intr_frame *);
void check_address(const void *addr);
void get_argument(void *rsp, int **arg, int count);

/* System call.
 *
 * Previously system call services was handled by the interrupt handler
 * (e.g. int 0x80 in linux). However, in x86-64, the manufacturer supplies
 * efficient path for requesting the system call, the `syscall` instruction.
 *
 * The syscall instruction works by reading the values from the the Model
 * Specific Register (MSR). For the details, see the manual. */

#define MSR_STAR 0xc0000081         /* Segment selector msr */
#define MSR_LSTAR 0xc0000082        /* Long mode SYSCALL target */
#define MSR_SYSCALL_MASK 0xc0000084 /* Mask for the eflags */

void syscall_init(void) {
	write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48  |
			((uint64_t)SEL_KCSEG) << 32);
	write_msr(MSR_LSTAR, (uint64_t) syscall_entry);

	/* The interrupt service rountine should not serve any interrupts
	 * until the syscall_entry swaps the userland stack to the kernel
	 * mode stack. Therefore, we masked the FLAG_FL. */
	write_msr(MSR_SYSCALL_MASK,
			FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);
}

/* The main system call interface */
void syscall_handler(struct intr_frame *f)
{ 
	/* 포인터 레지스터인 rsp 주소 확인 필요*/
	check_address(&f->rsp);

	switch (f->R.rax) // rax값이 들어가야함.
	{
	case SYS_HALT:
		halt();
		break;
	case SYS_EXIT:
		exit(args[1]);
		break;
	case SYS_FORK:
		fork(args[1]);
		break;
	case SYS_EXEC:
		exec(args[1]);
		break;
	case SYS_WAIT:
		wait(args[1]);
		break;
	case SYS_CREATE:
		create(args[1], args[2]);
		break;
	case SYS_REMOVE:
		remove(args[1]);
		break;
	case SYS_OPEN:
		open(args[1]);
		break;
	case SYS_FILESIZE:
		filesize(args[1]);
		break;
	case SYS_READ:
		read(args[1], args[2], args[3]);
		break;
	case SYS_WRITE:
		write(args[1], args[2], args[3]);
		break;
	case SYS_SEEK:
		seek(args[1], args[2]);
		break;
	case SYS_TELL:
		tell(args[1]);
		break;
	case SYS_CLOSE:
		close(args[1]);
		break;
	case SYS_DUP2:
		dup2(args[1], args[2]);
		break;
	case SYS_MMAP:
		mmap(args[1], args[2], args[3], args[4], args[5]);
		break;
	case SYS_MUNMAP:
		munmap(args[1]);
		break;
	case SYS_CHDIR:
		chdir(args[1]);
		break;
	case SYS_MKDIR:
		mkdir(args[1]);
		break;
	case SYS_READDIR:
		readdir(args[1], args[2]);
		break;
	case SYS_ISDIR:
		isdir(args[1]);
		break;
	case SYS_INUMBER:
		inumber(args[1]);
		break;
	case SYS_SYMLINK:
		symlink(args[1], args[2]);
		break;
	case SYS_MOUNT:
		mount(args[1], args[2], args[3]);
		break;
	case SYS_UMOUNT:
		umount(args[1]);
		break;
	default:
		thread_exit();
	}

	printf("system call!\n");
	thread_exit();
}

void check_address(const void *addr)
{
	/* 주소가 유효하지 않으면 예외 처리 ,주소가 유저 영역이 아니면 예외 처리*/
	if (addr == NULL || pml4_get_page(&thread_current()->pml4, addr) == NULL || is_kernel_vaddr(addr))
	{
		exit(1);
	}
}

void get_argument(void *rsp, int **arg, int count)
{
	rsp = (int64_t *)rsp + 2; // 원래 stack pointer에서 2칸(16byte) 올라감 : |argc|"argv"|...
	for (int i = 0; i < count; i++)
	{
		check_address(rsp); // 진교 추가
		arg[i] = rsp;
		rsp = (int64_t *)rsp + 1;
	}
}