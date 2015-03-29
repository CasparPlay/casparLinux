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

#include "../StdAfx.h"

#include "Thread.h"
#include "AsyncEventServer.h"

//#include <common/exception/win32_exception.h>
#include <exception>

#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>

namespace caspar {

#define	MAXCLIENTS	10

Event::Event(bool bManualReset, bool bInitialState) : handle_(0)
{
	//handle_ = CreateEvent(0, bManualReset, bInitialState, 0);
	//if(handle_ == 0) {
	//	throw std::exception("Failed to create event");
	//}
	pthread_cond_init(&cond_, 0);
//		throw std::exception("Failed to create event");

	pthread_mutex_init(&lock_, 0);
//		throw std::exception("Failed to init event mutex");
}

void Event::Set() {
	pthread_cond_signal(&cond_);
	//SetEvent(handle_);
	// TODO: huh?
	//BOOL res = SetEvent(handle_);
	//if(res == FALSE) {
	//	DWORD error = GetLastError();
	//}
}

void Event::Reset() {
	pthread_cond_init(&cond_, 0);
	pthread_mutex_init(&lock_, 0);
	//ResetEvent(handle_);
}

Event::~Event()
{
	//CloseHandle(handle_);
	pthread_cond_destroy(&cond_);
	pthread_mutex_destroy(&lock_);
}

//Thread::Thread() : pRunnable_(0), hThread_(0), stopEvent_(TRUE, FALSE), timeout_(10000)  {
//}

// stopEvent_ - manualReset = TRUE, initialState = FALSE
Thread::Thread() : pRunnable_(0), hThread_(0), threads_(0), stopEvent_(TRUE, FALSE), timeout_(10000)
{
}

Thread::~Thread() {
	Stop();
}

bool Thread::IsRunning() {
	/*if(hThread_ != 0) {
		if(WaitForSingleObject(hThread_, 0) == WAIT_OBJECT_0) {
			CloseHandle(hThread_);
			hThread_ = 0;
			pRunnable_ = 0;
		}
	}*/

	//return (hThread_ != 0);
	return (threads_ != 0);
}

// tentative prototype:
//bool Thread::Start(IRunnable* pRunnable) {
bool Thread::Start(IRunnable* pRunnable, int socket) {
	if (threads_ == 0) {
		pthread_attr_t attr;
		int ret = pthread_attr_init(&attr);
		if (ret != 0)
			return false;	// failed to init pthread attrs

		// make this thread detached
		ret = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
		if (ret != 0)
			return false;

		//pRunnable_ = pRunnable;
		stopEvent_.Reset();	// noop for Linux
		//hThread_ = CreateThread(0, 0, ThreadEntrypoint, this, 0, 0); - replace with Linux equivalent
		// TODO: Pass pAsyncServer as Thread Param
		ret = pthread_create(&threads_, &attr, ThreadEntrypoint, socket);
		if (ret == 0) {
			CASPAR_LOG(info) << L"pthread_create successfully";
			pRunnable_ = 1;
			return true;
		}
	}

	return false;
}

bool Thread::Stop(bool bWait) {
	bool returnValue = true;

	//if(hThread_ != 0) {	// means thread is running
	if (threads_) {
		stopEvent_.Set();

		if(bWait) {
			timespec tmptimeout;
			if (0 != timeout_)
			{
				// set timeout
				timeval now_;
				gettimeofday(&now_, 0);
				tmptimeout.tv_sec = now_.tv_sec + timeout_ / 1000;
				tmptimeout.tv_nsec = (((timeout_ % 1000) * 1000 + now_.tv_usec) % 1000000) * 1000;
			}
			//DWORD successCode = WaitForSingleObject(hThread_, timeout_);
			//if(successCode != WAIT_OBJECT_0)
			//	returnValue = false;
			pthread_mutex_t m = stopEvent_.getLock();
			pthread_cond_t c = stopEvent_.getCond();
			returnValue = pthread_cond_timedwait(&c, &m, &tmptimeout);
		}
		// CloseHandle(hThread_);
		// Stoping thread requires detaching thread and then canceling the thread
		if (pthread_detach(threads_) != 0)
			pthread_cancel(threads_);

		hThread_ = 0;
		pRunnable_ = 0;
	}

	return returnValue;
}

static void Thread::setnonblocking(int fd)
{
	int ret = fcntl(fd, F_GETFL);
	if (!ret) {
		ret |= O_NONBLOCK;
		if (fcntl(fd, F_SETFL, ret)) {
			CASPAR_LOG(error) << L"Failed to Mark non-blocking";
		}
	}
}

static void Thread::handleRead(int socket)
{
	char buffer[1024];
	int ret = recv(socket, buffer, 1023, 0);
	if (ret >= 0)
		CASPAR_LOG(info) << L"Got Something to read on " << socket;

	return;
}

// Make it Linux equivelent
static void * Thread::ThreadEntrypoint(void *pParam)
{
	int epollfd, epollcounter = 0, clientSocket;
	struct epoll_event ev, evs[MAXCLIENTS];

	int socket = (int) pParam; //AsyncEvs->pListenSocketInfo_->socket;

	CASPAR_LOG(info) << L"socket => " << socket;

	epollfd = epoll_create(10);
	if (epollfd == -1)
		CASPAR_LOG(error) << L"epoll_create() failed";

	evs[0].events = EPOLLIN|EPOLLET;
	evs[0].data.fd = socket;

	if (epoll_ctl(epollfd, EPOLL_CTL_ADD, socket, &evs[0])) {
		CASPAR_LOG(error) << "Failed to epoll_ctl";
	}
	epollcounter++;

	bzero(evs, sizeof(struct epoll_event) * MAXCLIENTS);

	while (1) {
		int waitResult = epoll_wait(epollfd, evs, epollcounter, -1);

		if (waitResult == -1)
			continue;
		else {
			for (int n = 0; n < waitResult; n++) {
				//CASPAR_LOG(info) << L"waitResults on " << n;
				if ((evs[n].events & EPOLLERR))
				{
					 //An error has occured on this fd, or the socket is not
					 //ready for reading (why were we notified then?)
					CASPAR_LOG(error) << L"epoll error..";
					close(evs[n].data.fd);
					continue;
				}			

				if (evs[n].data.fd == socket) {
					CASPAR_LOG(debug) << "OnAccept ";
					sockaddr_in	clientAddr;
					socklen_t addrSize = sizeof(clientAddr);
					clientSocket = accept(socket, NULL, NULL);
					if (clientSocket == -1) {
						CASPAR_LOG(error) << L"Accept error";
						break;
					} else
						CASPAR_LOG(info) << L"Accept returns" << clientSocket;

					setnonblocking(clientSocket);
					evs[epollcounter].events = EPOLLIN|EPOLLET|EPOLLPRI;
					evs[epollcounter].data.fd = clientSocket;
					if (epoll_ctl(epollfd, EPOLL_CTL_ADD, clientSocket, &evs[epollcounter]) == -1) {
						CASPAR_LOG(error) << "Accept: epoll_ctl failed ";
						break;
					} else
						CASPAR_LOG(error) << "Accept: epoll_ctl successed!";
					epollcounter++;
					auto ipv4_address = inet_ntoa(clientAddr.sin_addr);

					CASPAR_LOG(info) << "Accepted connection from " << ipv4_address;
				} else {
					CASPAR_LOG(info) << "ReadData ";
					handleRead(evs[epollcounter].data.fd);
				}
			}
		}
	}

	return 0;
}

void Thread::Run() {
	bool bDoRestart = false;

	do {
		try {
			bDoRestart = false;
			//stopEvent_.Reset();
			//pRunnable_->Run(stopEvent_);
		}
		catch(const std::exception& e) {
			bDoRestart = pRunnable_->OnUnhandledException(e);
		}
	} while(bDoRestart);
}

}	//namespace caspar
