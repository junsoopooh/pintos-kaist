#ifndef THREADS_THREAD_H
#define THREADS_THREAD_H

#include <debug.h>
#include <list.h>
#include <stdint.h>
#include "threads/interrupt.h"
#ifdef VM
#include "vm/vm.h"
#endif


/* States in a thread's life cycle. */
enum thread_status {
	THREAD_RUNNING,     /* Running thread. */
	THREAD_READY,       /* Not running but ready to run. */
	THREAD_BLOCKED,     /* Waiting for an event to trigger. */
	THREAD_DYING        /* About to be destroyed. */
};

/* Thread identifier type.
   You can redefine this to whatever type you like. */
typedef int tid_t;
#define TID_ERROR ((tid_t) -1)          /* Error value for tid_t. */

/* Thread priorities. */
#define PRI_MIN 0                       /* Lowest priority. */
#define PRI_DEFAULT 31                  /* Default priority. */
#define PRI_MAX 63                      /* Highest priority. */

/* A kernel thread or user process.
 *
 * Each thread structure is stored in its own 4 kB page.  The
 * thread structure itself sits at the very bottom of the page
 * (at offset 0).  The rest of the page is reserved for the
 * thread's kernel stack, which grows downward from the top of
 * the page (at offset 4 kB).  Here's an illustration:
 *
 *      4 kB +---------------------------------+
 *           |          kernel stack           |
 *           |                |                |
 *           |                |                |
 *           |                V                |
 *           |         grows downward          |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           +---------------------------------+
 *           |              magic              |
 *           |            intr_frame           |
 *           |                :                |
 *           |                :                |
 *           |               name              |
 *           |              status             |
 *      0 kB +---------------------------------+
 *
 * The upshot of this is twofold:
 *
 *    1. First, `struct thread' must not be allowed to grow too
 *       big.  If it does, then there will not be enough room for
 *       the kernel stack.  Our base `struct thread' is only a
 *       few bytes in size.  It probably should stay well under 1
 *       kB.
 *
 *    2. Second, kernel stacks must not be allowed to grow too
 *       large.  If a stack overflows, it will corrupt the thread
 *       state.  Thus, kernel functions should not allocate large
 *       structures or arrays as non-static local variables.  Use
 *       dynamic allocation with malloc() or palloc_get_page()
 *       instead.
 *
 * The first symptom of either of these problems will probably be
 * an assertion failure in thread_current(), which checks that
 * the `magic' member of the running thread's `struct thread' is
 * set to THREAD_MAGIC.  Stack overflow will normally change this
 * value, triggering the assertion. */
/* The `elem' member has a dual purpose.  It can be an element in
 * the run queue (thread.c), or it can be an element in a
 * semaphore wait list (synch.c).  It can be used these two ways
 * only because they are mutually exclusive: only a thread in the
 * ready state is on the run queue, whereas only a thread in the
 * blocked state is on a semaphore wait list. */

 //스레드를 위한 구조체
 // 스레드 구조체는 (스택)메모리의 머리에 있다. -> 스레드는 고유한 스택을 가지고 있음, 그 메모리의 처음 부분에 있는 것임
 // 스레드의 스택은 점점 힙영역으로 커짐
 // 스레드 구조체가 너무 크면 커널 스택을 위한 영역 부족 => 따라서 몇 바이트 정도 공간만 가지게 함(1kB미만)
 // 커널 스택도 너무 크지 않도록. 스택 오버플로우 발생하면 스레드 상태 변이 가능성 있음
 // 그래서 커널 함수는 큰 사이즈의 structure나 non-static 지역 변수를 가지면 안됨
 // malloc이나 palloc_get_page() 같은 동적 할당 사용
struct thread {
	/* Owned by thread.c. */
	tid_t tid;                          /* 모든 스레드가 수명동안 가지는 오름차순 정수로 된 고유 식별자 */
	enum thread_status status;          /* THREAD_RUNNING, THREAD_READY THREAD_BLOCKED THREAD_DYING */
	char name[16];                      /* 쓰레드 이름이나 축약어 */
	int priority;                       /* 스레드의 우선순위. 0~63 */

// RUNNING : 가동중, 딱 한 개의 스레드가 주어진 시간동안 가동중. thread_current()로 현재 가동중인 스레드 반환
// READY : 가동 준비, 가동은 안되는중. 스케쥴러로 가동 가능, ready_list라는 이중연결리스트 안에 보관
// BLOCKED : 대기(READY와 다름), thread_unblock()이 THREAD_READY로 바뀌어야 스케쥴에 할당(원시적 동기화방법)
// DYING : 다음 스레드오면 스케쥴러가 애를 없앨 거라는 뜻


	/* Shared between thread.c and synch.c. */
	struct list_elem elem;              /* 스레드를 이중연결리스트에 넣을려고. */

	// ready_list(위에서 ready중인 쓰레드의 리스트), sema_down()에 세마포어에서 waiting중인 스레드 리스트.
	// 같은 리스트에 두개에 다써도 되는 이유 : 쓰레드 in 세마포어 : ready상태 될수 없음, 쓰레드(ready) : 세마포어일 수 없음
	
	
	int64_t wakeup_tick; // 준코 : 현재의 tick(현재시간?)으로부터 몇tick후 깨어날지 변수로 추가
						 // 스레드가 sleep_list에서 wating할때, tick 체크해서 깨울 스레드 결정


#ifdef USERPROG
	/* Owned by userprog/process.c. */
	uint64_t *pml4;                     /* Page map level 4 */ //project1 에 안쓰임
#endif
#ifdef VM
	/* Table for whole virtual memory owned by thread. */
	struct supplemental_page_table spt;
#endif

	/* Owned by thread.c. */
	struct intr_frame tf;               /* Information for switching */
	unsigned magic;                     // 스택오버플로우 탐지. 오버플로우 일어나면 이 숫자 바뀜. 구조체의 말단 배치
};

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
extern bool thread_mlfqs;

void thread_init (void);
void thread_start (void);

void thread_tick (void);
void thread_print_stats (void);

typedef void thread_func (void *aux);
tid_t thread_create (const char *name, int priority, thread_func *, void *);

void thread_block (void);
void thread_unblock (struct thread *);

struct thread *thread_current (void);
tid_t thread_tid (void);
const char *thread_name (void);

void thread_exit (void) NO_RETURN;
void thread_yield (void);

int thread_get_priority (void);
void thread_set_priority (int);

int thread_get_nice (void);
void thread_set_nice (int);
int thread_get_recent_cpu (void);
int thread_get_load_avg (void);

void do_iret (struct intr_frame *tf);

//준코
//실행중인 thtread 재운다. 뒤에 ticks 는 재워서 sleep_list에 들어간 thread가 깨어날 시간
//ticks = 잘때 tick + 지정할 수면시간 tick
void thread_sleep(int64_t ticks);

//sleep_list 에서 자던애 깨우기
//list 순회하면서 스레드 중 wakeup_ticks 가 ticks보다 작으면 일어날 시간이 된거니 깨워줌
void thread_awake(int64_t ticks);

// sleep_list에서 가장 작은 wakeup_ticks 갱신
void update_next_tick_to_awake(int64_t ticks);

//next_tick_to_awake 반환
int64_t get_next_tick_to_awake(void);
#endif /* threads/thread.h */
