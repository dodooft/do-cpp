/**
 * Copyright (C) 2020, Javier E. Soto
 *
 * All rights reserved.

 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:

 *     * Redistributions of source code must retain the above copyright notice,
 *       this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright notice,
 *       this list of conditions and the following disclaimer in the documentation
 *       and/or other materials provided with the distribution.
 *     * Neither the name of {{ project }} nor the names of its contributors
 *       may be used to endorse or promote products derived from this software
 *       without specific prior written permission.

 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <iostream>
#include <unistd.h>
#include "dodooft/epoll-handler.h"

/**
 * @brief Construct a new epoll handler object
 *
 * @param max_events: amount of events that can be handle the epoll
 * @param event_check: timeout of epoll_wait
 */
epoll_handler::epoll_handler (int max_events, int event_check)
{
	this->finish_flag = false;
	this->max_events = max_events;
	this->event_check = event_check;
	this->events = static_cast<struct epoll_event *> (calloc (max_events, sizeof (struct epoll_event)));

	// Create epoll
	this->efd = epoll_create1 (0);
}

/**
 * @brief Destroy the epoll handler object
 */
epoll_handler::~epoll_handler ()
{
}

/**
 * @brief Register a file descriptor on epoll
 *
 * @param fd: smart pointer to a file descriptor handler
 * @param cb: callback
 * @return std::shared_ptr<epoll_event>: pointer to event
 */
std::shared_ptr<epoll_event> epoll_handler::register_event (std::weak_ptr<fd_handler> fd,
    std::function<void (std::weak_ptr<fd_handler>, uint32_t)> cb)
{
	auto ev = std::make_shared <struct epoll_event> ();
	auto fd_ptr = fd.lock ();
	ev->events = fd_ptr->get_events ();
	ev->data.fd = fd_ptr->get_fd ();

	if (epoll_ctl (this->efd, EPOLL_CTL_ADD, ev->data.fd, ev.get ()) == -1)
	{
		std::cerr << "Error registering event\n";
		close (this->efd);
		close (efd);
		return nullptr;
	}

	if (this->data.find (fd_ptr->get_fd ()) != this->data.end ())
		this->data [fd_ptr->get_fd ()] = fd;
	else
		this->data.insert (std::make_pair (fd_ptr->get_fd (), fd));

	fd_ptr->callback = cb;
	this->event_list.push_back (ev);

	std::cout << "Event registered to epoll: " << ev->data.fd << "\n";

	return ev;
}

/**
 * @brief Blocking epoll loop
 */
void epoll_handler::listen_loop ()
{
	while (1)
	{
		int nfds = epoll_wait (efd, events, this->max_events, this->event_check);

		if (nfds == -1)
		{
			close (this->efd);
			return;
		}

		for (ssize_t n = 0; n < nfds; ++n)
		{
			// Call callback
			if (data[events[n].data.fd].expired ()) continue;
			auto handler = data[events[n].data.fd].lock ();
			handler->callback (handler, events[n].events);
		}

		if (this->finish_flag) break;
	}
}

/**
 * @brief Set finish flag
 */
void epoll_handler::finish ()
{
	this->finish_flag = true;
}
