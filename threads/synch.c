#include "threads/synch.h"
#include <stdio.h>
#include <string.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
/* --------------------[project1]-----------------------*/
void donate_priority(void);
void remove_with_lock(struct lock *lock);
void refresh_priority(void);

bool sem_priority_less(const struct list_elem *a, const struct list_elem *b,
					   void *aux UNUSED);
bool donate_priority_less(struct list_elem *a, struct list_elem *b,
						  void *aux UNUSED);
/* --------------------[project1]-----------------------*/

void sema_init(struct semaphore *sema, unsigned value)
{
	ASSERT(sema != NULL);

	sema->value = value;
	list_init(&sema->waiters);
}

void sema_down(struct semaphore *sema)
{
	enum intr_level old_level;

	ASSERT(sema != NULL);
	ASSERT(!intr_context());

	old_level = intr_disable();
	while (sema->value == 0)
	{
		list_insert_ordered(&sema->waiters, &thread_current()->elem, priority_less, NULL);
		thread_block();
	}
	sema->value--;
	intr_set_level(old_level);
}

bool sema_try_down(struct semaphore *sema)
{
	enum intr_level old_level;
	bool success;

	ASSERT(sema != NULL);

	old_level = intr_disable();
	if (sema->value > 0)
	{
		sema->value--;
		success = true;
	}
	else
		success = false;
	intr_set_level(old_level);

	return success;
}

void sema_up(struct semaphore *sema)
{
	enum intr_level old_level;

	ASSERT(sema != NULL);

	old_level = intr_disable();
	if (!list_empty(&sema->waiters))
	{
		list_sort(&sema->waiters, donate_priority_less, 0); // 🤔
		thread_unblock(list_entry(list_pop_front(&sema->waiters),
								  struct thread, elem));
	}

	sema->value++;
	test_max_priority();
	intr_set_level(old_level);
}

static void sema_test_helper(void *sema_);

void sema_self_test(void)
{
	struct semaphore sema[2];
	int i;

	printf("Testing semaphores...");
	sema_init(&sema[0], 0);
	sema_init(&sema[1], 0);
	thread_create("sema-test", PRI_DEFAULT, sema_test_helper, &sema);
	for (i = 0; i < 10; i++)
	{
		sema_up(&sema[0]);
		sema_down(&sema[1]);
	}
	printf("done.\n");
}

static void
sema_test_helper(void *sema_)
{
	struct semaphore *sema = sema_;
	int i;

	for (i = 0; i < 10; i++)
	{
		sema_down(&sema[0]);
		sema_up(&sema[1]);
	}
}

void lock_init(struct lock *lock)
{
	ASSERT(lock != NULL);

	lock->holder = NULL;
	sema_init(&lock->semaphore, 1);
}

void lock_acquire(struct lock *lock)
{
	ASSERT(lock != NULL);
	ASSERT(!intr_context());
	ASSERT(!lock_held_by_current_thread(lock));

	struct thread *cur_t = thread_current();

	if (lock->holder != NULL)
	{
		cur_t->wait_on_lock = lock;
		list_insert_ordered(&lock->holder->donations, &cur_t->donation_elem,
							donate_priority_less, 0); // 추가 🤔
		donate_priority();
	}

	sema_down(&lock->semaphore);
	cur_t->wait_on_lock = NULL;
	lock->holder = thread_current();
}

bool lock_try_acquire(struct lock *lock)
{
	bool success;

	ASSERT(lock != NULL);
	ASSERT(!lock_held_by_current_thread(lock));

	success = sema_try_down(&lock->semaphore);
	if (success)
		lock->holder = thread_current();
	return success;
}

void lock_release(struct lock *lock)
{
	ASSERT(lock != NULL);
	ASSERT(lock_held_by_current_thread(lock));

	lock->holder = NULL;
	remove_with_lock(lock);
	refresh_priority();
	sema_up(&lock->semaphore);
}

bool lock_held_by_current_thread(const struct lock *lock)
{
	ASSERT(lock != NULL);

	return lock->holder == thread_current();
}

/* --------------------[project1]-----------------------*/
// 🤔
void donate_priority(void)
{
	struct thread *cur = thread_current();
	int depth;
	for (depth = 0; depth < 8; depth++)
	{ // 최대 8단계까지 수행
		// depth++;
		if (!cur->wait_on_lock) // 추가 🤔
			break;

		struct thread *holder = cur->wait_on_lock->holder;
		// if (holder == NULL)
		// {
		// 	break;
		// }
		holder->priority = cur->priority;
		// if (holder->wait_on_lock != NULL)
		// {
		// 	lock = (holder->wait_on_lock);
		// }
		// else
		// {
		// 	break;
		// }
		cur = holder;
	}
}

void remove_with_lock(struct lock *lock)
{
	// 🤔
	// if (lock->holder != NULL)
	// {
	// struct list *donation_list = &(thread_current()->donations);
	// struct list *donation_list = &lock->holder->donations;
	struct list_elem *find;
	struct thread *curr = thread_current();
	for (find = list_begin(&curr->donations); find != list_end(&curr->donations); find = list_next(find))
	{
		struct thread *t = list_entry(find, struct thread, donation_elem);
		if (t->wait_on_lock == lock)
		{
			list_remove(&t->donation_elem); // 인자 🤔
		}
	}
	// }
}

void refresh_priority(void)
{
	struct thread *curr = thread_current();
	curr->priority = curr->init_priority;

	if (!list_empty(&curr->donations))
	{
		list_sort(&curr->donations, donate_priority_less, 0);

		struct thread *front = list_entry(list_front(&curr->donations), struct thread, donation_elem);

		if (front->priority > curr->priority)
			curr->priority = front->priority;
	}

	// 🤔
	// int don_max_pri = 0;
	// if (!list_empty(&curr->donations))
	// {
	// 	list_entry(list_max(&(thread_current()->donations), priority_less, NULL), struct thread, elem)->priority;
	// }
	// if (don_max_pri > curr->priority)
	// {
	// 	curr->priority = don_max_pri;
	// };
}
/* --------------------[project1]-----------------------*/

/* ===============================[condition variable]=============================== */

struct semaphore_elem
{
	struct list_elem elem;
	struct semaphore semaphore;
};

void cond_init(struct condition *cond)
{
	ASSERT(cond != NULL);

	list_init(&cond->waiters);
}

void cond_wait(struct condition *cond, struct lock *lock)
{
	struct semaphore_elem waiter;

	ASSERT(cond != NULL);
	ASSERT(lock != NULL);
	ASSERT(!intr_context());
	ASSERT(lock_held_by_current_thread(lock));

	sema_init(&waiter.semaphore, 0);
	// list_push_back (&cond->waiters, &waiter.elem); 🤔
	list_insert_ordered(&cond->waiters, &waiter.elem, sem_priority_less, NULL);
	lock_release(lock);
	sema_down(&waiter.semaphore);
	lock_acquire(lock);
}

void cond_signal(struct condition *cond, struct lock *lock UNUSED)
{
	ASSERT(cond != NULL);
	ASSERT(lock != NULL);
	ASSERT(!intr_context());
	ASSERT(lock_held_by_current_thread(lock));

	if (!list_empty(&cond->waiters))
	{
		list_sort(&cond->waiters, sem_priority_less, NULL);
		sema_up(&list_entry(list_pop_front(&cond->waiters),
							struct semaphore_elem, elem)
					 ->semaphore);
	}
}

void cond_broadcast(struct condition *cond, struct lock *lock)
{
	ASSERT(cond != NULL);
	ASSERT(lock != NULL);

	while (!list_empty(&cond->waiters))
		cond_signal(cond, lock);
}

/* --------------------[project1]-----------------------*/
bool sem_priority_less(const struct list_elem *a, const struct list_elem *b,
					   void *aux UNUSED)
{
	struct semaphore_elem *a_sema = list_entry(a, struct semaphore_elem, elem);
	struct semaphore_elem *b_sema = list_entry(b, struct semaphore_elem, elem);

	struct list *waiter_a_sema = &(a_sema->semaphore.waiters);
	struct list *waiter_b_sema = &(b_sema->semaphore.waiters);

	return list_entry(list_begin(waiter_a_sema), struct thread, elem)->priority > list_entry(list_begin(waiter_b_sema), struct thread, elem)->priority;
}

bool donate_priority_less(struct list_elem *a, struct list_elem *b,
						  void *aux UNUSED)
{
	struct thread *t_a = list_entry(a, struct thread, elem);
	struct thread *t_b = list_entry(b, struct thread, elem);
	return (t_a->priority) > (t_b->priority);
}
/* --------------------[project1]-----------------------*/