#ifndef __SEM_TIMEDWAIT_H
#define __SEM_TIMEDWAIT_H

#ifdef __APPLE__
int sem_timedwait(sem_t *sem, const struct timespec *abs_timeout);
#endif

#endif
