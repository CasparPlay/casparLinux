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

#include "ClientInfo.h"

#include <common/memory/safe_ptr.h>

#include <tbb/mutex.h>

//#include <winsock2.h>
#include <queue>
#include <vector>

namespace caspar {
namespace IO {

class AsyncEventServer;
class SocketInfo : public ClientInfo
{
	SocketInfo(const SocketInfo&);
	SocketInfo& operator=(const SocketInfo&);

public:
	SocketInfo(int, AsyncEventServer*);
	
	virtual ~SocketInfo();

	//void Send(const std::wstring& data);
	void Send(const std::wstring& data, const int sock);
	void Disconnect();
	virtual std::wstring print() const override {return host_;}
	void bind_to_lifecycle(const std::shared_ptr<void>& lifecycle_bound);

//	SOCKET		socket_;
	int 		socket_;	// Linux socket fd
//	HANDLE		event_;	treat is as void * for Linux
	void 		*event_;
	
	std::wstring	host_;
	std::vector<wchar_t> wideRecvBuffer_;

private:
	tbb::mutex mutex_;
	friend class AsyncEventServer;

	std::queue<std::wstring> sendQueue_;
	AsyncEventServer* pServer_;
	std::vector<std::shared_ptr<void>> lifecycle_bound_items_;

	std::vector<char> currentlySending_;
	unsigned int currentlySendingOffset_;

	char recvBuffer_[8192];
	int recvLeftoverOffset_;
};

//typedef std::tr1::shared_ptr<SocketInfo> SocketInfoPtr;
//tr1 is deprecated
typedef std::shared_ptr<SocketInfo> SocketInfoPtr;

}	//namespace IO
}	//namespace caspar
