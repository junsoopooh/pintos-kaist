#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"
#include "include/lib/user/syscall.h"
#include "filesys.h"
#include "userprog/process.h"
#include "devices/input.h"
#include "lib/kernel/console.c"

void syscall_entry(void);
void syscall_handler(struct intr_frame *);
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

#define MSR_STAR 0xc0000081			/* Segment selector msr */
#define MSR_LSTAR 0xc0000082		/* Long mode SYSCALL target */
#define MSR_SYSCALL_MASK 0xc0000084 /* Mask for the eflags */

void syscall_init(void)
{
	write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48 |
							((uint64_t)SEL_KCSEG) << 32);
	write_msr(MSR_LSTAR, (uint64_t)syscall_entry);

	/* The interrupt service rountine should not serve any interrupts
	 * until the syscall_entry swaps the userland stack to the kernel
	 * mode stack. Therefore, we masked the FLAG_FL. */
	write_msr(MSR_SYSCALL_MASK,
			  FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);

	/* project2 */
	lock_init(&filesys_lock);
	/* project2 */
}

/* 🤔 */
struct lock
{
	struct thread *holder;		/* Thread holding lock (for debugging). */
	struct semaphore semaphore; /* Binary semaphore controlling access. */
};

void lock_init(struct lock *lock)
{
	ASSERT(lock != NULL);

	lock->holder = NULL;
	sema_init(&lock->semaphore, 1);
}

void sema_init(struct semaphore *sema, unsigned value)
{
	ASSERT(sema != NULL);

	sema->value = value;
	list_init(&sema->waiters);
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
		exit(f->R.rdi);
		break;
	case SYS_FORK:
		fork(f->R.rdi);
		break;
	case SYS_EXEC:
		exec(f->R.rdi);
		break;
	case SYS_WAIT:
		wait(f->R.rdi);
		break;
	case SYS_CREATE:
		f->R.rax = create(f->R.rdi, f->R.rsi);
		break;
	case SYS_REMOVE:
		f->R.rax = remove(f->R.rdi);
		break;
	case SYS_OPEN:
		open(f->R.rdi);
		break;
	case SYS_FILESIZE:
		f->R.rax = filesize(f->R.rdi);
		break;
	case SYS_READ:
		read(f->R.rdi, f->R.rsi, f->R.rdx);
		break;
	case SYS_WRITE:
		write(f->R.rdi, f->R.rsi, f->R.rdx);
		break;
	case SYS_SEEK:
		seek(f->R.rdi, f->R.rsi);
		break;
	case SYS_TELL:
		tell(f->R.rdi);
		break;
	case SYS_CLOSE:
		close(f->R.rdi);
		break;
	case SYS_DUP2:
		dup2(f->R.rdi, f->R.rsi);
		break;
	case SYS_MMAP:
		mmap(f->R.rdi, f->R.rsi, f->R.rdx, f->R.r10, f->R.r8);
		break;
	case SYS_MUNMAP:
		munmap(f->R.rdi);
		break;
	case SYS_CHDIR:
		chdir(f->R.rdi);
		break;
	case SYS_MKDIR:
		mkdir(f->R.rdi);
		break;
	case SYS_READDIR:
		readdir(f->R.rdi, f->R.rsi);
		break;
	case SYS_ISDIR:
		isdir(f->R.rdi);
		break;
	case SYS_INUMBER:
		inumber(f->R.rdi);
		break;
	case SYS_SYMLINK:
		symlink(f->R.rdi, f->R.rsi);
		break;
	case SYS_MOUNT:
		mount(f->R.rdi, f->R.rsi, f->R.rdx);
		break;
	case SYS_UMOUNT:
		umount(f->R.rdi);
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

/* pintos 종료시키는 함수 */
void halt(void)
{
	power_off();
}

void exit(int status)
{ // 종료 status를 입력받는다. exit(0)이면 성공, 아님 실패
	struct thread *cur = thread_current();
	cur->exit_status = status;
	printf("%s: exit(%d)\n", cur->name, status);
	thread_exit(); // 스레드가 죽는다.
}

bool create(const char *file, unsigned initial_size)
{
	check_address(file);
	return filesys_create(file, initial_size);
}

bool remove(const char *file)
{
	check_address(file);
	return filesys_remove(file);
}

int filesize(int fd)
{
	struct file *fileobj = find_file_from_fd(fd);

	if (fileobj == NULL)
		return -1;

	return file_length(fileobj);
}

pid_t exec(const *cmd_line)
{
	struct thread *curr = thread_current();
	tid_t pid = process_create_initd(cmd_line);	   // cmd_line parsing해서 file_name 추출해서 넣음.
												   /* sema_down 옛터  */
	struct thread *child = get_child_process(pid); // 생성된 자식 프로세스의 디스크립터 검색 미구현 😡
	int result = process_wait(child);			   // 자식 프로세스의 프로그램 적재 대기😡
	list_push_back(&curr->children_list, &child->child_elem);

	if (result = 1) // 적재 성공시
	{
		return pid;
	}
	else
	{ // 적재 실패시
		return -1;
	}
}

/* 🤔 */
int open(const char *file)
{
	struct file *fileobj = filesys_open(file);
	if (fileobj == NULL)
	{
		return -1;
	}
	int fd = add_file_to_fdt(fileobj); // 해당 파일을 가리키는 포인터를 fdt에 넣어주고 식별자 리턴

	// struct thread *curr = thread_current();
	// curr->fdt[curr->next_fd] = file;

	if (fd == -1)
	{
		file_close(fileobj);
	}
	return fd;
}

/* 이건 다시 생각해봐야 할 것 같아... 힌트) 표준 입력 */
int read(int fd, void *buffer, unsigned size)
{
	lock_acquire(&filesys_lock);
	if (fd)
	{
		if (!file_read(process_get_file(fd), buffer, size))
		{
			return -1;
		}
		return file_read(process_get_file(fd), buffer, size);
	}
	else
	{
		buffer = input_getc();
		return sizeof(buffer);
	}
}

/* 이건 다시 생각해봐야 할 것 같아... 힌트) 표준 출력 */
int write(int fd, void *buffer, unsigned size)
{
	lock_acquire(&filesys_lock);
	if (fd == 1)
	{
		putbuf(buffer, size);
		return sizeof(buffer);
	}
	else
	{
		file_write(process_get_file(fd), buffer, size);
		return size; // size? filesize? 😡
	}
}

void seek(int fd, unsigned position)
{
	file_seek(process_get_file(fd), position);
}

unsigned tell(int fd)
{
	struct file *fileobj = process_get_file(fd);

	if (fileobj == NULL || fileobj <= 2) /* 조건 빠짐 */
		return -1;

	file_tell(fileobj);
}

void close(int fd)
{
	struct thread *cur = thread_current();
	struct file *fileobj = process_get_file(fd);

	/* 추가해야 함 🤔
	if (fileobj == STDIN)
	{
		cur->stdin_count--;
	}
	if (fileobj == STDOUT)
	{
		cur->stdout_count--;
	} */

	process_close_file(fd);
}