/*
* Copyright 2013 Sveriges Television AB http://casparcg.com/
*
* This file is part of CasparCG (www.casparcg.com).
*
* CasparCG is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* CasparCG is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with CasparCG. If not, see <http://www.gnu.org/licenses/>.
*
* Author: Nicklas P Andersson
*/

#pragma once

#include "SocketInfo.h"
#include "Thread.h"
#include "ProtocolStrategy.h"

#include <exception>
#include <memory>
#include <pthread.h>

#define	HANDLE	void *
#define	DWORD	unsigned long
#define	LPVOID	void *

/*typedef struct _HANDLE_ {
	mutex		_mtx;
	pthread_cond_t	_cond;
	bool		_manual_reset;
	bool		_signaled;
} * HANDLE; */

#define	TRUE	1
#define	FALSE	0

namespace caspar {
	
class IRunnable
{
public:
	virtual ~IRunnable() {}
//	virtual void Run(HANDLE stopEvent) = 0;
//	replace with Linux equvalent of HANDLE
	virtual void Run(void *stopEvent) = 0;

	virtual bool OnUnhandledException(const std::exception&) throw() = 0;
};

class Event
{
public:
	Event(bool bManualReset, bool bInitialState);
	~Event();

	operator const HANDLE() const {
		return handle_;
	}

//	HANDLE Handle() const - replaceed with Linux equivalent
	void *Handle() const
	{
		return handle_;
	}

	void Set();
		void Reset();

	pthread_cond_t getCond() const
	{
		return cond_;
	}

	pthread_mutex_t getLock() const
	{
		return lock_;
	}

private:
//	HANDLE handle_;
	void *handle_;
	pthread_cond_t	cond_;
	pthread_mutex_t lock_;
};

typedef std::shared_ptr<Event> EventPtr;

class Thread
{
	Thread(const Thread&);
	Thread& operator=(const Thread&);
public:
	Thread();
	~Thread();

	static void handleRead(int);
	static void setnonblocking(int fd);
//	safe_ptr<IProtocolStrategy>	pProtocolStrategy_;
	bool Start(IRunnable* pRunnable, int fd);
	bool Stop(bool bWait = true);

	bool IsRunning();

	void SetTimeout(DWORD timeout) {
		timeout_ = timeout;
	}
	
	DWORD GetTimeout() {
		return timeout_;
	}

	static void * ThreadEntrypoint(void *pParam);
private:
	// epoll related declartion.
//	static void * ThreadEntrypoint(LPVOID pParam);
	void Run();

	HANDLE			hThread_;
	pthread_t		threads_;	// thread handler
	IRunnable*		pRunnable_;
	Event			stopEvent_;
	DWORD			timeout_;
};
}
