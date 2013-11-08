/*!The Treasure Box Library
 * 
 * TBox is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 * 
 * TBox is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with TBox; 
 * If not, see <a href="http://www.gnu.org/licenses/"> http://www.gnu.org/licenses/</a>
 * 
 * Copyright (C) 2009 - 2012, ruki All rights reserved.
 *
 * @author		ruki
 * @file		kqueue.c
 *
 */
/* ///////////////////////////////////////////////////////////////////////
 * includes
 */
#include <errno.h>
#include <sys/event.h>
#include <sys/time.h>
#ifndef TB_CONFIG_OS_ANDROID
# 	include <unistd.h>
#endif

/* ///////////////////////////////////////////////////////////////////////
 * macros
 */

#ifndef EV_ENABLE
# 	define EV_ENABLE 	(0)
#endif

#ifndef NOTE_EOF
# 	define NOTE_EOF 	(0)
#endif

/* ///////////////////////////////////////////////////////////////////////
 * types
 */

// the kqueue reactor type
typedef struct __tb_aiop_reactor_kqueue_t
{
	// the reactor base
	tb_aiop_reactor_t 		base;

	// the kqueue fd
	tb_long_t 				kqfd;

	// the events
	struct kevent64_s* 			evts;
	tb_size_t 				evtn;
	
}tb_aiop_reactor_kqueue_t;

/* ///////////////////////////////////////////////////////////////////////
 * implementation
 */
static tb_bool_t tb_aiop_reactor_kqueue_sync(tb_aiop_reactor_t* reactor, struct kevent64_s* evts, tb_size_t evtn)
{
	tb_aiop_reactor_kqueue_t* rtor = (tb_aiop_reactor_kqueue_t*)reactor;
	tb_assert_and_check_return_val(rtor && rtor->kqfd >= 0, tb_false);
	tb_assert_and_check_return_val(evts && evtn, tb_false);

	// change events
	struct timespec t = {0};
	if (kevent64(rtor->kqfd, evts, evtn, tb_null, 0, 0, &t) < 0) return tb_false;

	// ok
	return tb_true;
}
static tb_bool_t tb_aiop_reactor_kqueue_addo(tb_aiop_reactor_t* reactor, tb_handle_t handle, tb_size_t code, tb_pointer_t data)
{
	tb_aiop_reactor_kqueue_t* rtor = (tb_aiop_reactor_kqueue_t*)reactor;
	tb_assert_and_check_return_val(rtor && rtor->kqfd >= 0 && handle, tb_false);

	// fd
	tb_long_t fd = ((tb_long_t)handle) - 1;

	// add event
	struct kevent64_s 	e[2];
	tb_size_t 			n = 0;
	if (code & TB_AIOE_CODE_RECV || code & TB_AIOE_CODE_ACPT) 
	{
		EV_SET64(&e[n], fd, EVFILT_READ, EV_ADD | EV_ENABLE, NOTE_EOF, tb_null, handle, code, data); n++;
	}
	if (code & TB_AIOE_CODE_SEND || code & TB_AIOE_CODE_CONN)
	{
		EV_SET64(&e[n], fd, EVFILT_WRITE, EV_ADD | EV_ENABLE, NOTE_EOF, tb_null, handle, code, data); n++;
	}

	// ok?
	return tb_aiop_reactor_kqueue_sync(reactor, e, n);
}
static tb_void_t tb_aiop_reactor_kqueue_delo(tb_aiop_reactor_t* reactor, tb_handle_t handle)
{
	tb_aiop_reactor_kqueue_t* rtor = (tb_aiop_reactor_kqueue_t*)reactor;
	tb_assert_and_check_return(rtor && rtor->kqfd >= 0 && handle);

	// fd
	tb_long_t fd = ((tb_long_t)handle) - 1;

	// del event
	struct kevent64_s e[2];
	EV_SET64(&e[0], fd, EVFILT_READ, EV_DELETE, 0, 0, 0, 0, 0);
	EV_SET64(&e[1], fd, EVFILT_WRITE, EV_DELETE, 0, 0, 0, 0, 0);

	// ok?
	return tb_aiop_reactor_kqueue_sync(reactor, e, 2);
}
static tb_bool_t tb_aiop_reactor_kqueue_post(tb_aiop_reactor_t* reactor, tb_aioe_t const* list, tb_size_t size)
{
	// check
	tb_aiop_reactor_poll_t* rtor = (tb_aiop_reactor_poll_t*)reactor;
	tb_assert_and_check_return_val(rtor && rtor->pfds && list && size && size <= TB_AIOP_POST_MAXN, tb_false);

	// init kevents
	struct kevent64_s 	e[TB_AIOP_POST_MAXN << 2];
	tb_size_t 		n = 0;

	// walk list
	tb_size_t i = 0;
	tb_size_t post = 0;
	for (i = 0; i < size; i++)
	{
		// the aioe
		tb_aioe_t const* aioe = &list[i];
		if (aioe && aioe->handle)
		{
			// fd
			tb_long_t fd = ((tb_long_t)aioe->handle) - 1;

			// reset
			EV_SET64(&e[n], fd, EVFILT_READ, EV_DELETE, 0, tb_null, aioe->handle, aioe->code, aioe->data); n++;
			EV_SET64(&e[n], fd, EVFILT_WRITE, EV_DELETE, 0, tb_null, aioe->handle, aioe->code, aioe->data); n++;

			// adde
			if (aioe->code & TB_AIOE_CODE_RECV || aioe->code & TB_AIOE_CODE_ACPT) 
			{
				EV_SET64(&e[n], fd, EVFILT_READ, EV_ADD | EV_ENABLE, NOTE_EOF, tb_null, handle, aioe->code, aioe->data); n++;
			}
			if (aioe->code & TB_AIOE_CODE_SEND || aioe->code & TB_AIOE_CODE_CONN)
			{
				EV_SET64(&e[n], fd, EVFILT_WRITE, EV_ADD | EV_ENABLE, NOTE_EOF, tb_null, handle, aioe->code, aioe->data); n++;
			}

			// post++
			post++;
		}
	}

	// check
	tb_assert_and_check_return_val(post == size, tb_false);

	// ok?
	return tb_aiop_reactor_kqueue_sync(reactor, e, n);
}
static tb_long_t tb_aiop_reactor_kqueue_wait(tb_aiop_reactor_t* reactor, tb_aioo_t* list, tb_size_t maxn, tb_long_t timeout)
{	
	// check
	tb_aiop_reactor_kqueue_t* rtor = (tb_aiop_reactor_kqueue_t*)reactor;
	tb_assert_and_check_return_val(rtor && rtor->kqfd >= 0 && reactor->aiop && list && maxn, -1);

	// init time
	struct timespec t = {0};
	if (timeout > 0)
	{
		t.tv_sec = timeout / 1000;
		t.tv_nsec+= (timeout % 1000) * 1000000;
	}

	// init grow
	tb_size_t grow = tb_align8((reactor->aiop->maxn >> 3) + 1);

	// init events
	if (!rtor->evts)
	{
		rtor->evtn = grow;
		rtor->evts = tb_nalloc0(rtor->evtn, sizeof(struct kevent64_s));
		tb_assert_and_check_return_val(rtor->evts, -1);
	}

	// wait events
	tb_long_t evtn = kevent64_s(rtor->kqfd, tb_null, 0, rtor->evts, rtor->evtn, timeout >= 0? &t : tb_null);
	tb_assert_and_check_return_val(evtn >= 0 && evtn <= rtor->evtn, -1);
	
	// timeout?
	tb_check_return_val(evtn, 0);

	// grow it if events is full
	if (evtn == rtor->evtn)
	{
		// grow size
		rtor->evtn += grow;
		if (rtor->evtn > reactor->aiop->maxn) rtor->evtn = reactor->aiop->maxn;

		// grow data
		rtor->evts = tb_ralloc(rtor->evts, rtor->evtn * sizeof(struct kevent64_s));
		tb_assert_and_check_return_val(rtor->evts, -1);
	}
	tb_assert(evtn <= rtor->evtn);

	// limit 
	evtn = tb_min(evtn, maxn);

	// sync
	tb_size_t i = 0;
	for (i = 0; i < evtn; i++)
	{
		// the kevent64_s 
		struct kevent64_s* e = rtor->evts + i;

		// init the aioe
		tb_aioe_t* aioe = list + i;
		aioe->handle = (tb_handle_t)e->udata;
		aioe->code = 0;
		aioe->data = e->ext[1];
		if (e->filter == EVFILT_READ) 
		{
			aioe->code |= TB_AIOE_CODE_RECV;
			if (e->ext[0] & TB_AIOE_CODE_ACPT) aioe->code |= TB_AIOE_CODE_ACPT;
		}
		if (e->filter == EVFILT_WRITE) 
		{
			aioe->code |= TB_AIOE_CODE_SEND;
			if (e->ext[0] & TB_AIOE_CODE_CONN) aioe->code |= TB_AIOE_CODE_CONN;
		}
		if (e->flags & EV_ERROR && !(aioe->code & TB_AIOE_CODE_RECV | TB_AIOE_CODE_SEND)) 
			aioe->code |= TB_AIOE_CODE_RECV | TB_AIOE_CODE_SEND;
	}

	// ok
	return evtn;
}
static tb_void_t tb_aiop_reactor_kqueue_exit(tb_aiop_reactor_t* reactor)
{
	tb_aiop_reactor_kqueue_t* rtor = (tb_aiop_reactor_kqueue_t*)reactor;
	if (rtor)
	{
		// free events
		if (rtor->evts) tb_free(rtor->evts);

		// close kqfd
		if (rtor->kqfd >= 0) close(rtor->kqfd);

		// free it
		tb_free(rtor);
	}
}
static tb_void_t tb_aiop_reactor_kqueue_cler(tb_aiop_reactor_t* reactor)
{
	tb_aiop_reactor_kqueue_t* rtor = (tb_aiop_reactor_kqueue_t*)reactor;
	if (rtor)
	{
		// close kqfd
		if (rtor->kqfd >= 0)
		{
			// FIXME 
			close(rtor->kqfd);
			rtor->kqfd = kqueue();
		}
	}
}
static tb_aiop_reactor_t* tb_aiop_reactor_kqueue_init(tb_aiop_t* aiop)
{
	// check
	tb_assert_and_check_return_val(aiop && aiop->maxn, tb_null);

	// alloc reactor
	tb_aiop_reactor_kqueue_t* rtor = tb_malloc0(sizeof(tb_aiop_reactor_kqueue_t));
	tb_assert_and_check_return_val(rtor, tb_null);

	// init base
	rtor->base.aiop = aiop;
	rtor->base.exit = tb_aiop_reactor_kqueue_exit;
	rtor->base.cler = tb_aiop_reactor_kqueue_cler;
	rtor->base.addo = tb_aiop_reactor_kqueue_addo;
	rtor->base.delo = tb_aiop_reactor_kqueue_delo;
	rtor->base.post = tb_aiop_reactor_kqueue_post;
	rtor->base.wait = tb_aiop_reactor_kqueue_wait;

	// init kqueue
	rtor->kqfd = kqueue();
	tb_assert_and_check_goto(rtor->kqfd >= 0, fail);

	// ok
	return (tb_aiop_reactor_t*)rtor;

fail:
	if (rtor) tb_aiop_reactor_kqueue_exit(rtor);
	return tb_null;
}
