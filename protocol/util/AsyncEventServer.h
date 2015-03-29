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

 
// AsyncEventServer.h: interface for the AsyncServer class.
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_ASYNCEVENTSERVER_H__0BFA29CB_BE4C_46A0_9CAE_E233ED27A8EC__INCLUDED_)
#define AFX_ASYNCEVENTSERVER_H__0BFA29CB_BE4C_46A0_9CAE_E233ED27A8EC__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif

#include <common/memory/safe_ptr.h>

#include <string>
#include <map>
#include <vector>
#include <functional>

#include <tr1/unordered_map>
#include <tr1/memory>

#include <protocol/util/Thread.h>

#include "ProtocolStrategy.h"

#include <tbb/mutex.h>

#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/socket.h>
//#include <linux/in.h>
#include <arpa/inet.h>

namespace caspar {
namespace IO {

#define MAXIMUM_WAIT_OBJECTS 64
#define	WAIT_OBJECT_0 0x00000000L

#define	HANDLE	void *
#define	TCHAR	wchar_t

class SocketInfo;
typedef std::shared_ptr<SocketInfo> SocketInfoPtr;

typedef std::function<void(caspar::IO::SocketInfoPtr)> ClientDisconnectEvent;
typedef std::function<std::shared_ptr<void> (const std::string& ipv4_address)>	lifecycle_factory_t;

class AsyncEventServer : public IRunnable
{
	static long instanceCount_;

	static void setnonblocking(int fd);
	bool startThread(int fd);
	static void * ThreadEntrypoint(LPVOID pParam);
	static void handleRead(safe_ptr<IProtocolStrategy> pStrategy,  SocketInfoPtr pListenSock, int);
	static void handleWrite(SocketInfoPtr pSI, const std::wstring& data, int sock);
	safe_ptr<IProtocolStrategy> pProtocolStrategy_;

	AsyncEventServer();
	AsyncEventServer(const AsyncEventServer&);
	AsyncEventServer& operator=(const AsyncEventServer&);

public:
	explicit AsyncEventServer(const safe_ptr<IProtocolStrategy>& pProtocol, int port);
	~AsyncEventServer();

	bool Start();
	//void SetProtocolStrategy(safe_ptr<IProtocolStrategy> pPS) {
	//	pProtocolStrategy_ = pPS;
	//}

	void Stop();

	void SetClientDisconnectHandler(ClientDisconnectEvent handler);
	
	void add_lifecycle_factory(const lifecycle_factory_t& lifecycle_factory);

private:
	Thread	listenThread_;
	int listenSocket_;
	int  port_;

	SocketInfoPtr	pListenSocketInfo_;
	void Run(HANDLE stopEvent);
	bool OnUnhandledException(const std::exception&) throw();

	bool OnAccept(SocketInfoPtr&);
	bool OnRead(SocketInfoPtr&);
	void OnWrite(SocketInfoPtr&);
	void DoSend(SocketInfo&);
	void OnClose(SocketInfoPtr&);
	void OnError(HANDLE waitEvent, int errorCode);

	friend class SocketInfo;
	void DisconnectClient(SocketInfo&);

	void LogSocketError(const TCHAR* pStr, int socketError = 0);

	class SocketInfoCollection
	{
		SocketInfoCollection(const SocketInfoCollection&);
		SocketInfoCollection& operator=(const SocketInfoCollection&);

		typedef std::map<HANDLE, SocketInfoPtr> SocketInfoMap;
		typedef std::vector<HANDLE> HandleVector;

	public:
		SocketInfoCollection();
		~SocketInfoCollection();

		bool AddSocketInfo(SocketInfoPtr& pSocketInfo);
		void RemoveSocketInfo(SocketInfoPtr& pSocketInfo);
		void RemoveSocketInfo(HANDLE);
		void CopyCollectionToArray(HANDLE*, int maxCount);

		bool FindSocketInfo(HANDLE, SocketInfoPtr& pResult);

		bool IsDirty() {
			return bDirty_;
		}
		void ClearDirty() {
			bDirty_ = false;
		}

		std::size_t Size() {
			return waitEvents_.size();
		}
		void Clear();

		ClientDisconnectEvent onSocketInfoRemoved;
		
		HandleVector waitEvents_;
		tbb::mutex mutex_;
		std::list<int> clientfds;
	private:
		SocketInfoMap socketInfoMap_;
		bool bDirty_;
	};

	std::vector<lifecycle_factory_t> lifecycle_factories_;
	SocketInfoCollection socketInfoCollection_;
	tbb::mutex mutex_;
};

typedef std::tr1::shared_ptr<AsyncEventServer> AsyncEventServerPtr;

}	//namespace IO
}	//namespace caspar

#endif // !defined(AFX_ASYNCEVENTSERVER_H__0BFA29CB_BE4C_46A0_9CAE_E233ED27A8EC__INCLUDED_)
