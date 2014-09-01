/******************************************************************************
 ** Coypright(C) 2013-2014 Xundao technology Co., Ltd
 **
 ** 文件名: thread_pool.c
 ** 版本号: 1.0
 ** 描  述: 线程池模块的实现.
 **         通过线程池模块, 可有效的简化多线程编程的处理, 加快开发速度, 同时有
 **         效增强模块的复用性和程序的稳定性。
 ** 作  者: # Qifeng.zou # 2012.12.26 #
 ******************************************************************************/
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <memory.h>
#include <stdarg.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/types.h>

#include "thread_pool.h"

static int thread_create_detach(thread_pool_t *tp, int idx);
static void *thread_routine(void *arg);

/******************************************************************************
 ** Name : thread_pool_init
 ** Desc : Initalize thread pool
 ** Input: 
 **     num: The num of threads in pool
 ** Output: 
 **     tp: Thread pool
 ** Return: 0: success !0: failed
 ** Process:
 **     1. Alloc thread pool space, and initalize it.
 **     2. Create the num of threads
 ** Note :
 ** Author: # Qifeng.zou # 2012.12.26 #
 ******************************************************************************/
int thread_pool_init(thread_pool_t **tp, int num)
{
	int idx=0, ret=0;

	/* 1. 分配线程池空间，并初始化 */
	*tp = (thread_pool_t*)calloc(1, sizeof(thread_pool_t));
	if (NULL == *tp)
	{
		return -1;
	}

	pthread_mutex_init(&((*tp)->queue_lock), NULL);
	pthread_cond_init(&((*tp)->queue_ready), NULL);
	(*tp)->head = NULL;
	(*tp)->queue_size = 0;
	(*tp)->shutdown = 0;
	(*tp)->tid = (pthread_t*)calloc(1, num*sizeof(pthread_t));
	if (NULL == (*tp)->tid)
	{
		free(*tp);
		(*tp) = NULL;
		return -1;
	}

	/* 2. 创建指定数目的线程 */
	for (idx=0; idx<num; idx++)
	{
		ret = thread_create_detach(*tp, idx);
		if (0 != ret)
		{
			return -1;
		}
		(*tp)->num++;
	}

	return 0;
}

/******************************************************************************
 ** Name : thread_pool_add_worker
 ** Desc : Register routine callback function
 ** Input: 
 **     tp: Thread pool
 **     process: Callback function
 **     arg: The paramter of callback function
 ** Output: NONE
 ** Return: 0: success !0: failed
 ** Process:
 **     1. Create new task node
 **     2. Add callback function into work queue
 **     3. Wakeup waitting process 
 ** Note :
 ** Author: # Qifeng.zou # 2012.12.26 #
 ******************************************************************************/
int thread_pool_add_worker(thread_pool_t *tp, void *(*process)(void *arg), void *arg)
{
	thread_worker_t *worker=NULL, *member=NULL;

	/* 1. 创建新任务节点 */
	worker = (thread_worker_t*)calloc(1, sizeof(thread_worker_t));
	if (NULL == worker)
	{
		return -1;
	}

	worker->process = process;
	worker->arg = arg;
	worker->next = NULL;

	/* 2. 将回调函数加入工作队列 */
	pthread_mutex_lock(&(tp->queue_lock));

	member = tp->head;
	if (NULL != member)
	{
		while (NULL != member->next)
		{
			member = member->next;
		}
		member->next = worker;
	}
	else
	{
		tp->head = worker;
	}

	tp->queue_size++;

	pthread_mutex_unlock(&(tp->queue_lock));

	/* 3. 唤醒正在等待的线程 */
	pthread_cond_signal(&(tp->queue_ready));

	return 0;
}

/******************************************************************************
 ** Name : thread_pool_keepalive
 ** Desc : Keepalive thread
 ** Input: 
 **     tp: Thread-pool
 ** Output: NONE
 ** Return: 0: success !0: failed
 ** Process:
 **     1. Judge the thread whether exist?
 **     2. The thread was dead if it's not exist.
 ** Note :
 ** Author: # Qifeng.zou # 2012.12.26 #
 ******************************************************************************/
int thread_pool_keepalive(thread_pool_t *tp)
{
	int idx=0, ret=0;

 	for (idx=0; idx<tp->num; idx++)
	{
		ret = pthread_kill(tp->tid[idx], 0);
		if (ESRCH == ret)
		{
			ret = thread_create_detach(tp, idx);
			if (ret < 0)
			{
				return -1;
			}
		}
	}

	return 0;
}

/******************************************************************************
 ** Name : thread_pool_keepalive_ext
 ** Desc : Keepalive thread
 ** Input: 
 **     tp: Thread-pool
 ** Output: NONE
 ** Return: 0: success !0: failed
 ** Process:
 **     1. Judge the thread whether exist?
 **     2. The thread was dead if it's not exist.
 ** Note :
 ** Author: # Qifeng.zou # 2012.12.26 #
 ******************************************************************************/
int thread_pool_keepalive_ext(thread_pool_t *tp, void *(*process)(void *arg), void *arg)
{
	int idx=0, ret=0;

 	for (idx=0; idx<tp->num; idx++)
	{
		ret = pthread_kill(tp->tid[idx], 0);
		if (ESRCH == ret)
		{
			ret = thread_create_detach(tp, idx);
			if (ret < 0)
			{
				return -1;
			}

            thread_pool_add_worker(tp, process, arg);
		}

	}

	return 0;
}
/******************************************************************************
 ** Name : thread_pool_get_tidx
 ** Desc : Get thread index of current thread.
 ** Input: 
 **     tp: The object of thread-pool
 ** Output: NONE
 ** Return: 0: success !0: failed
 ** Process:
 ** Note :
 ** Author: # Qifeng.zou # 2014.04.24 #
 ******************************************************************************/
int thread_pool_get_tidx(thread_pool_t *tp)
{
    int idx = 0;
    pthread_t tid = pthread_self();

    for (idx=0; idx<tp->num; ++idx)
    {
        if (tp->tid[idx] == tid)
        {
            return idx;
        }
    }

    return -1;
}

/******************************************************************************
 ** Name : thread_pool_destroy
 ** Desc : Destroy thread pool
 ** Input: 
 **     tp: Thread-pool
 ** Output: NONE
 ** Return: 0: success !0: failed
 ** Process:
 **     1. Close thread-pool flag
 **     2. Wakeuup all the threads which is waitting
 **     3. Wait all threads end of run
 **     4. Release all threads space
 ** Note :
 ** Author: # Qifeng.zou # 2012.12.26 #
 ******************************************************************************/
int thread_pool_destroy(thread_pool_t *tp)
{
	int idx=0, ret=0;
	thread_worker_t *member = NULL;


	if (0 != tp->shutdown)
	{
		return -1;
	}

	/* 1. 设置关闭线程池标志 */
	tp->shutdown = 1;

	/* 2. 唤醒所有等待的线程 */
	pthread_cond_broadcast(&(tp->queue_ready));

	/* 3. 等待线程结束 */
	while (idx < tp->num)
	{
		ret = pthread_kill(tp->tid[idx], 0);
		if (ESRCH == ret)
		{
			idx++;
			continue;
		}
		pthread_cancel(tp->tid[idx]);
		idx++;
	}

	/* 4. 释放线程池对象空间 */
	free(tp->tid);
	tp->tid = NULL;

	while (NULL != tp->head)
	{
		member = tp->head;
		tp->head = member->next;
		free(member);
	}

	pthread_mutex_destroy(&(tp->queue_lock));
	pthread_cond_destroy(&(tp->queue_ready));
	free(tp);
	
	return 0;
}

/******************************************************************************
 ** Name : thread_pool_destroy_ext
 ** Desc : Destroy thread pool
 ** Input: 
 **     tp: 线程池对象
 **     args_destroy: 释放tp->data的空间
 **     cntx: 其他相关参数
 ** Output: NONE
 ** Return: 0: success !0: failed
 ** Process:
 **     1. 设置关闭标志
 **     2. 唤醒所有线程
 **     3. 杀死所有线程
 **     4. 释放参数空间
 **     5. 释放线程池对象
 ** Note :
 ** Author: # Qifeng.zou # 2014.04.28 #
 ******************************************************************************/
int thread_pool_destroy_ext(thread_pool_t *tp, void (*args_destroy)(void *cntx, void *args), void *cntx)
{
	int idx;
	thread_worker_t *member;


	if (0 != tp->shutdown)
	{
		return -1;
	}

	/* 1. 设置关闭标志 */
	tp->shutdown = 1;

	/* 2. 唤醒所有线程 */
	pthread_cond_broadcast(&(tp->queue_ready));

	/* 3. 杀死所有线程 */
	for (idx=0; idx < tp->num; ++idx)
	{
		pthread_cancel(tp->tid[idx]);
	}

	/* 4. 释放参数空间 */
    args_destroy(cntx, tp->data);

	/* 5. 释放对象空间 */
	free(tp->tid);
	tp->tid = NULL;

	while (NULL != tp->head)
	{
		member = tp->head;
		tp->head = member->next;
		free(member);
	}

	pthread_mutex_destroy(&(tp->queue_lock));
	pthread_cond_destroy(&(tp->queue_ready));
	free(tp);
	
	return 0;
}

/******************************************************************************
 ** Name : thread_create_detach
 ** Desc : Create detach thread
 ** Input: 
 **     tp: Thread pool
 **     idx: The index of thead
 ** Output: NONE
 ** Return: 0: success !0: failed
 ** Process:
 ** Note :
 **     1. Init thread attribute
 **     2. Set thread attribute
 **     3. Create thread
 **     4. Destroy thread attribute
 ** Author: # Qifeng.zou # 2014.05.10 #
 ******************************************************************************/
static int thread_create_detach(thread_pool_t *tp, int idx)
{
	int ret = 0;
	pthread_attr_t attr;

	do
	{
		ret = pthread_attr_init(&attr);
		if (0 != ret)
		{
			break;
		}
		
		ret = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
		if (0 != ret)
		{
			break;
		}
		
		ret = pthread_attr_setstacksize(&attr, 0x800000);
		if (ret < 0)
		{
			break;
		}
		
		ret = pthread_create(&(tp->tid[idx]), &attr, thread_routine, tp);
		if (0 != ret)
		{
			if (EINTR == errno)
			{
			    pthread_attr_destroy(&attr);
				continue;
			}
			
			break;
		}

		pthread_attr_destroy(&attr);
        return 0;
	}while (1);

    pthread_attr_destroy(&attr);
	return -1;
}

/******************************************************************************
 ** Name : thread_routine
 ** Desc : The entry of routine
 ** Input: 
 **     arg: the parmater of this function
 ** Output: NONE
 ** Return: 0: success !0: failed
 ** Process:
 ** Note :
 ** Author: # Qifeng.zou # 2013.12.26 #
 ******************************************************************************/
static void *thread_routine(void *arg)
{
	thread_worker_t *worker = NULL;
	thread_pool_t *tp = (thread_pool_t*)arg;

	while (1)
	{
		pthread_mutex_lock(&(tp->queue_lock));
		while ((0 == tp->shutdown)
			&& (0 == tp->queue_size))
		{
			pthread_cond_wait(&(tp->queue_ready), &(tp->queue_lock));
		}

		if (0 != tp->shutdown)
		{
			pthread_mutex_unlock(&(tp->queue_lock));
			pthread_exit(NULL);
		}

		tp->queue_size--;
		worker = tp->head;
		tp->head = worker->next;
		pthread_mutex_unlock(&(tp->queue_lock));

		(*(worker->process))(worker->arg);

		free(worker);
		worker = NULL;
	}
}
