#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"
/*-------------------------[project 2]-------------------------*/
// #include "include/lib/user/syscall.h"
#include "filesys/filesys.h"
#include "userprog/process.h"
#include "devices/input.h"
#include "threads/palloc.h"

void syscall_entry(void);
void syscall_handler(struct intr_frame *);
void check_address(const void *addr);

void halt(void);
void exit(int status);
bool create(const char *file, unsigned initial_size);
bool remove(const char *file);
int filesize(int fd);
int exec(const char *cmd_line);
int open(const char *file);
int read(int fd, void *buffer, unsigned size);
int write(int fd, const void *buffer, unsigned size);
void seek(int fd, unsigned position);
void close(int fd);
tid_t fork(const char *thread_name, struct intr_frame *f);
int wait(tid_t pid);
unsigned tell(int fd);

struct file *process_get_file(int fd);
void process_close_file(int fd);
/*-------------------------[project 2]-------------------------*/

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

const int STDIN = 1;
const int STDOUT = 2;

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

/* The main system call interface */
void syscall_handler(struct intr_frame *f)
{
	switch (f->R.rax) // rax값이 들어가야함.
	{
	case SYS_HALT:
		halt();
		break;
	case SYS_EXIT:
		exit(f->R.rdi);
		break;
	case SYS_FORK:
		f->R.rax = fork(f->R.rdi, f);
		break;
	case SYS_EXEC: // Switch current process.
		if (exec(f->R.rdi) == -1)
		{
			exit(-1);
		}
		break;
	case SYS_WAIT:
		f->R.rax = wait(f->R.rdi);
		break;
	case SYS_CREATE:
		f->R.rax = create(f->R.rdi, f->R.rsi);
		break;
	case SYS_REMOVE:
		f->R.rax = remove(f->R.rdi);
		break;
	case SYS_OPEN:
		f->R.rax = open(f->R.rdi);
		break;
	case SYS_FILESIZE:
		f->R.rax = filesize(f->R.rdi);
		break;
	case SYS_READ:
		f->R.rax = read(f->R.rdi, f->R.rsi, f->R.rdx);
		break;
	case SYS_WRITE:
		f->R.rax = write(f->R.rdi, f->R.rsi, f->R.rdx);
		break;
	case SYS_SEEK:
		seek(f->R.rdi, f->R.rsi);
		break;
	case SYS_TELL:
		f->R.rax = tell(f->R.rdi);
		break;
	case SYS_CLOSE:
		close(f->R.rdi);
		break;
	// case SYS_DUP2:
	// 	dup2(f->R.rdi, f->R.rsi);
	// 	break;
	// case SYS_MMAP:
	// 	mmap(f->R.rdi, f->R.rsi, f->R.rdx, f->R.r10, f->R.r8);
	// 	break;
	// case SYS_MUNMAP:
	// 	munmap(f->R.rdi);
	// 	break;
	// case SYS_CHDIR:
	// 	chdir(f->R.rdi);
	// 	break;
	// case SYS_MKDIR:
	// 	mkdir(f->R.rdi);
	// 	break;
	// case SYS_READDIR:
	// 	readdir(f->R.rdi, f->R.rsi);
	// 	break;
	// case SYS_ISDIR:
	// 	isdir(f->R.rdi);
	// 	break;
	// case SYS_INUMBER:
	// 	inumber(f->R.rdi);
	// 	break;
	// case SYS_SYMLINK:
	// 	symlink(f->R.rdi, f->R.rsi);
	// 	break;
	// case SYS_MOUNT:
	// 	mount(f->R.rdi, f->R.rsi, f->R.rdx);
	// 	break;
	// case SYS_UMOUNT:
	// 	umount(f->R.rdi);
	// 	break;
	default:
		exit(-1);
		break;
	}

	// printf("system call!\n");
	// thread_exit();
}

void check_address(const void *addr)
{
	struct thread *curr = thread_current();

	if (is_kernel_vaddr(addr) || addr == NULL || pml4_get_page(curr->pml4, addr) == NULL)
	{
		exit(-1);
	}
}

// void get_argument(void *rsp, int **arg, int count)
// {
// 	rsp = (int64_t *)rsp + 2; // 원래 stack pointer에서 2칸(16byte) 올라감 : |argc|"argv"|...
// 	for (int i = 0; i < count; i++)
// 	{
// 		check_address(rsp); // 진교 추가
// 		arg[i] = rsp;
// 		rsp = (int64_t *)rsp + 1;
// 	}
// }

/* pintos 종료시키는 함수 */
void halt(void)
{
	power_off();
}

void exit(int status)
{ // 종료 status를 입력받는다. exit(0)이면 성공, 아님 실패
	struct thread *cur = thread_current();
	cur->exit_status = status;
	printf("%s: exit(%d)\n", thread_name(), status);

	thread_exit(); // 스레드가 죽는다.
}

bool create(const char *file, unsigned initial_size)
{
	check_address(file);
	if (filesys_create(file, initial_size))
	{
		return true;
	}
	else
	{
		return false;
	}
}

bool remove(const char *file)
{
	check_address(file);

	if (filesys_remove(file))
	{
		return true;
	}
	return false;
}

int filesize(int fd)
{
	struct file *fileobj = process_get_file(fd);

	if (fileobj == NULL)
		return -1;

	return file_length(fileobj);
}

int exec(const char *cmd_line)
{
	check_address(cmd_line);

	/* 인자로 받은 파일 이름 문자열을 복사하여 이 복사본을 인자로 process_exec() 실행*/
	int size = strlen(cmd_line) + 1;
	char *fn_copy = palloc_get_page(PAL_ZERO);
	if (fn_copy == NULL)
		exit(-1);
	strlcpy(fn_copy, cmd_line, size);

	if (process_exec(fn_copy) == -1) /* process_exec에서 free해줌 */
		return -1;

	/* Caller 프로세스는 do_iret() 후 돌아오지 못한다. */
	NOT_REACHED();

	return 0; // 이 값은 리턴되지 않는다. 즉, exec()은 오직 에러가 발생했을 때만 리턴한다.
}

/* 🤔 */
int open(const char *file)
{
	check_address(file);
	lock_acquire(&filesys_lock);
	struct file *fileobj = filesys_open(file);

	if (fileobj == NULL)
	{
		return -1;
	}
	int fd = process_add_file(fileobj); // 해당 파일을 가리키는 포인터를 fdt에 넣어주고 식별자 리턴

	// struct thread *curr = thread_current();
	// curr->fdt[curr->next_fd] = file;

	if (fd == -1)
	{
		file_close(fileobj);
	}

	lock_release(&filesys_lock);
	return fd;
}

int read(int fd, void *buffer, unsigned size)
{
	// 유효한 주소인지부터 체크
	check_address(buffer);			  // 버퍼 시작 주소 체크
	check_address(buffer + size - 1); // 버퍼 끝 주소도 유저 영역 내에 있는지 체크
	unsigned char *buf = buffer;
	int read_count;

	struct file *fileobj = process_get_file(fd);

	if (fileobj == NULL)
	{
		return -1;
	}

	if (size == 0)
	{
		return 0;
	}

	/* STDIN일 때: */
	if (fileobj == STDIN)
	{
		char key;
		for (int read_count = 0; read_count < size; read_count++)
		{
			key = input_getc();
			*buf++ = key;
			if (key == '\0')
			{ // 엔터값
				break;
			}
		}
	}
	/* STDOUT일 때: -1 반환 */
	else if (fileobj == STDOUT)
	{
		return -1;
	}

	else
	{
		lock_acquire(&filesys_lock);
		read_count = file_read(fileobj, buffer, size); // 파일 읽어들일 동안만 lock 걸어준다.
		lock_release(&filesys_lock);
	}
	return read_count;
}

int write(int fd, const void *buffer, unsigned size)
{
	check_address(buffer);
	int write_count;

	struct file *fileobj = process_get_file(fd);

	if (fd == 0)
	{
		return 0;
	}

	if (fileobj == NULL)
	{
		return -1;
	}
	if (fileobj == STDOUT)
	{
		putbuf(buffer, size);
		write_count = size;
	}
	else if (fileobj == STDIN)
	{
		return -1;
	}
	else
	{

		lock_acquire(&filesys_lock);
		write_count = file_write(fileobj, buffer, size);
		lock_release(&filesys_lock);
	}
	return write_count; // 출력한 데이터의 byte를 반환한다.
}

void seek(int fd, unsigned position)
{
	struct file *fileobj = process_get_file(fd);
	if (fd < 2)
	{
		return;
	}
	// check_address(fileobj);
	// if (fileobj == NULL)
	// 	return;

	file_seek(fileobj, position);
}

unsigned tell(int fd)
{
	struct file *fileobj = process_get_file(fd);
	if (fd < 2)
	{
		return;
	}
	// check_address(fileobj);
	// if (fileobj == NULL) /* 조건 빠짐 */
	// 	return -1;

	return file_tell(fileobj);
}

void close(int fd)
{
	if (fd <= 1)
		return;
	struct file *fileobj = process_get_file(fd);

	if (fileobj == NULL)
	{
		return;
	}

	// lock_acquire(&filesys_lock);
	// file_close(fileobj);
	// lock_release(&filesys_lock);

	// thread_current()->fdt[fd] = NULL;
	process_close_file(fd);
}

tid_t fork(const char *thread_name, struct intr_frame *f)
{
	// check_address(thread_name);
	return process_fork(thread_name, f);
}

int wait(tid_t pid)
{
	return process_wait(pid);
}

/* 준코 project2  */
/* 🤔 */
int process_add_file(struct file *f)
{
	struct thread *curr = thread_current();
	int findIdx = curr->next_fd; /* 탐색 포인터 */
	// ASSERT(f != NULL);

	while (findIdx < FDCOUNT_LIMIT && curr->fdt[findIdx])
	{
		findIdx++;
	}
	if (findIdx >= FDCOUNT_LIMIT)
	{
		return -1;
	}
	curr->fdt[findIdx] = f;
	return findIdx;
}

struct file *process_get_file(int fd)
{
	if (fd < 0 || fd >= FDCOUNT_LIMIT || fd == NULL)
	{
		return NULL;
	}
	struct thread *curr = thread_current();
	return curr->fdt[fd];
}

void process_close_file(int fd)
{
	struct thread *curr = thread_current();
	if (fd < 0 || fd >= FDCOUNT_LIMIT)
		return;

	curr->fdt[fd] = NULL;
}