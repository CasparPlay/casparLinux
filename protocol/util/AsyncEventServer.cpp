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

 
// AsyncEventServer.cpp: implementation of the AsyncEventServer class.
//
//////////////////////////////////////////////////////////////////////

#include "../StdAfx.h"

#include "SocketInfo.h"

#include "AsyncEventServer.h"

#include <common/log/log.h>
#include <string>
#include <algorithm>
#include <boost/algorithm/string/replace.hpp>
#include <unistd.h>
#include <fcntl.h>

#include <stdlib.h>

#if defined(_MSC_VER)
#pragma warning (push, 1) // TODO: Legacy code, just disable warnings, will replace with boost::asio in future
#endif

namespace caspar { namespace IO {
	
#define CASPAR_MAXIMUM_SOCKET_CLIENTS	(MAXIMUM_WAIT_OBJECTS-1)
#define	MAXCLIENTS	10
#define	BUFSIZE		1024

long AsyncEventServer::instanceCount_ = 0;

std::wstring TEXT(const char* src)
{
	return std::wstring(src, src + strlen(src));
}

//////////////////////////////
// AsyncEventServer constructor
// PARAMS: port(TCP-port the server should listen to)
// COMMENT: Initializes the WinSock2 library
AsyncEventServer::AsyncEventServer(const safe_ptr<IProtocolStrategy>& pProtocol, int port) : port_(port),	pProtocolStrategy_(pProtocol)
{
//	pProtocolStrategy_ = std::move(pProtocol);
	//epollcounter = 0;
	/*if(instanceCount_ == 0) {
		WSADATA wsaData;
		if(WSAStartup(MAKEWORD(2,2), &wsaData) != NO_ERROR)
			throw std::exception("Error initializing WinSock2");
		else {
			CASPAR_LOG(info) << "WinSock2 Initialized.";
		}
	}

	InterlockedIncrement(&instanceCount_); */
}

/////////////////////////////
// AsyncEventServer destructor
AsyncEventServer::~AsyncEventServer() {
	Stop();

}

void AsyncEventServer::SetClientDisconnectHandler(ClientDisconnectEvent handler) {
	socketInfoCollection_.onSocketInfoRemoved = handler;
}

void AsyncEventServer::add_lifecycle_factory(const lifecycle_factory_t& factory)
{
	tbb::mutex::scoped_lock lock(mutex_);

	lifecycle_factories_.push_back(factory);
}

//////////////////////////////
// AsyncEventServer::Start
// RETURNS: true at successful startup
bool AsyncEventServer::Start() {
	CASPAR_LOG(error) << "About to Start AsyncEventServer...";
	if(listenThread_.IsRunning())
		return false;

	socketInfoCollection_.Clear();

	sockaddr_in sockAddr;
//	ZeroMemory(&sockAddr, sizeof(sockAddr));
	bzero(&sockAddr, sizeof(sockAddr));
	
	sockAddr.sin_family = AF_INET;
	sockAddr.sin_addr.s_addr = INADDR_ANY;
	sockAddr.sin_port = htons(port_);
	
//	SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP); linux equ
	int listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if(listenSocket == -1) {
		CASPAR_LOG(error) << "Failed to create listenSocket";
		return false;
	}

	int yes = 1;

	if (setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int))) {
		CASPAR_LOG(info) << L"Failed to setsockopt()";
	}

	pListenSocketInfo_ = SocketInfoPtr(new SocketInfo(listenSocket, this));

	//if(WSAEventSelect(pListenSocketInfo_->socket_, pListenSocketInfo_->event_, FD_ACCEPT|FD_CLOSE) == SOCKET_ERROR) {
	//	CASPAR_LOG(error) << "Failed to enter EventSelect-mode for listenSocket";
	//	return false;
	//}
	//

//	if(bind(pListenSocketInfo_->socket_, (sockaddr*)&sockAddr, sizeof(sockAddr)) == SOCKET_ERROR) {
	if(bind(pListenSocketInfo_->socket_, (sockaddr*)&sockAddr, sizeof(sockAddr)) == -1) {
		CASPAR_LOG(error) << "Failed to bind listenSocket";
		return false;
	}

	if(listen(pListenSocketInfo_->socket_, SOMAXCONN) == -1) {
		CASPAR_LOG(error) << "Failed to listen";
		return false;
	}

	socketInfoCollection_.AddSocketInfo(pListenSocketInfo_);

	//start thread: the entrypoint is Run(EVENT stopEvent)
	//if(!listenThread_.Start(this, listenSocket)) {
	//	CASPAR_LOG(error) << "Failed to create ListenThread";
	//	return false;
	//}

	listenSocket_ = listenSocket;
//	thisProtocolStrategy = std::move(pProtocolStrategy_);

	if (!startThread(listenSocket))
	{
		CASPAR_LOG(error) << "Failed to create ListenThread";
		return false;
	}

	CASPAR_LOG(info) << "Listener successfully initialized";

	return true;
}

static void AsyncEventServer::handleRead(safe_ptr<IProtocolStrategy> pStrategy, SocketInfoPtr pListenSock, int socket)
{
	char buffer[BUFSIZE], wbuffer[BUFSIZE];
	int ret = recv(socket, buffer, BUFSIZE, 0);

	if (ret == -1) {
		close(socket);
		//socketInfoCollection_.RemoveSocketInfo(pListenSock);
		return;
	}

	if (!ret)
		return;

	buffer[ret] = '\0';

	const size_t cSize = strlen(buffer)+1;
	wchar_t* wc = new wchar_t[cSize];
	
	ret = mbstowcs(wc, buffer, cSize);
	//CASPAR_LOG(info) << ret;

	ret = wcstombs(wbuffer, wc, cSize);
	//CASPAR_LOG(info) << ret;

	//CASPAR_LOG(info) << L"handleRead : " << wc << L"on " << socket;
	//wmemcpy(pListenSock->wideRecvBuffer_[0], (wchar_t*)wbuffer, cSize);
	pStrategy->Parse(wc, cSize, pListenSock, socket);
	//thisProtocolStrategy->Parse(pListenSocketInfo_->wideRecvBuffer_[0], 1023, pListenSocketInfo_);
}

static void * AsyncEventServer::ThreadEntrypoint(void *pParam)
{
	int epollfd, epollcounter = 0, clientSocket;
	struct epoll_event ev, evs[MAXCLIENTS];

	AsyncEventServer *Async = reinterpret_cast<AsyncEventServer*>(pParam);
	int socket = Async->listenSocket_;

	CASPAR_LOG(info) << L"socket => " << socket;

	epollfd = epoll_create(10);
	if (epollfd == -1)
		CASPAR_LOG(error) << L"epoll_create() failed";

	evs[0].events = EPOLLIN|EPOLLOUT|EPOLLERR|EPOLLET;
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
				if ((evs[n].events & EPOLLERR))
				{
					CASPAR_LOG(error) << L"epoll error..";
					close(evs[n].data.fd);
					continue;
				}			

				if (evs[n].data.fd == socket) {
					CASPAR_LOG(debug) << "OnAccept ";
					sockaddr_in	clientAddr;
					socklen_t addrSize = sizeof(clientAddr);
					clientSocket = accept(socket, (sockaddr*)&clientAddr, &addrSize);
					if (clientSocket == -1) {
						CASPAR_LOG(error) << L"Accept error";
						continue;
					} else
						CASPAR_LOG(info) << L"Accept returns" << clientSocket;

					setnonblocking(clientSocket);
					evs[epollcounter].events = EPOLLIN|EPOLLOUT|EPOLLET;
					evs[epollcounter].data.fd = clientSocket;
					if (epoll_ctl(epollfd, EPOLL_CTL_ADD, clientSocket, &evs[epollcounter]) == -1) {
						CASPAR_LOG(error) << "Accept: epoll_ctl failed ";
						continue;
					} else
						CASPAR_LOG(error) << "Accept: epoll_ctl successed!";

					auto ipv4_address = inet_ntoa(clientAddr.sin_addr);
					CASPAR_LOG(info) << "Accepted connection from " << ipv4_address;
				} else {
					if (evs[n].events & EPOLLIN)
						handleRead(Async->pProtocolStrategy_, Async->pListenSocketInfo_, evs[n].data.fd);
					if (evs[n].events & EPOLLOUT) {
						//CASPAR_LOG(info) << L"It's time to write";
						std::wstring data(L"");
						handleWrite(Async->pListenSocketInfo_, data, evs[n].data.fd);
					}
				}
			}
		}
	}
	return 0;
}

bool AsyncEventServer::startThread(int listenSocket)
{
	pthread_t	threads_;	// thread handler
	pthread_attr_t attr;
	int ret = pthread_attr_init(&attr);
	if (ret != 0)
		return false;	// failed to init pthread attrs

	// make this thread detached
	ret = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	if (ret != 0)
		return false;

	ret = pthread_create(&threads_, &attr, ThreadEntrypoint, this);
	if (ret == 0) {
		CASPAR_LOG(info) << L"pthread_create successfully";
		return true;
	}
	return false;
}

static void AsyncEventServer::setnonblocking(int fd)
{
	int ret = fcntl(fd, F_GETFL);
	if (!ret) {
		ret |= O_NONBLOCK;
		if (fcntl(fd, F_SETFL, ret)) {
			CASPAR_LOG(error) << L"Failed to Mark non-blocking";
		}
	}
}

void AsyncEventServer::Run(HANDLE stopEvent)
{
	CASPAR_LOG(info) << L"before epoll_wait() ...";

	/* while(true) {
	int waitResult = epoll_wait(epollfd, &ev, epollcounter, -1);

		if (waitResult == -1)
			continue;
		else {
			DWORD eventIndex = waitResult - WAIT_OBJECT_0;

			HANDLE waitEvent = waitHandlesCopy[eventIndex];
			SocketInfoPtr pSocketInfo;

			for (int n = 0; n < waitResult; n++) {
				if (ev.data.fd = pSocketInfo->socket_)
					//if(networkEvents.iErrorCode[FD_ACCEPT_BIT] == 0)
					OnAccept(pSocketInfo);
				else
					CASPAR_LOG(debug) << "OnAccept (ErrorCode:)";

				if (ev.events & EPOLLIN) {
					CASPAR_LOG(debug) << "Got Something to read";
					OnRead(pSocketInfo);
				}

				if (ev.events & EPOLLOUT)
					OnWrite(pSocketInfo);
			}
		}
	}*/
}

bool AsyncEventServer::OnUnhandledException(const std::exception& ex) throw() {
	bool bDoRestart = true;

	try 
	{
		CASPAR_LOG(fatal) << "UNHANDLED EXCEPTION in TCPServers listeningthread. Message: " << ex.what();
	}
	catch(...)
	{
		bDoRestart = false;
	}

	return bDoRestart;
}

///////////////////////////////
// AsyncEventServer:Stop
// COMMENT: Shuts down
void AsyncEventServer::Stop()
{
	//TODO: initiate shutdown on all clients connected
//	for(int i=0; i < _totalActiveSockets; ++i) {
//		shutdown(_pSocketInfo[i]->_socket, SD_SEND);
//	}

//	if(!listenThread_.Stop()) {
//		CASPAR_LOG(warning) << "Wait for listenThread timed out.";
//	}

	socketInfoCollection_.Clear();
}

////////////////////////////////////////////////////////////////////
//
// MESSAGE HANDLERS   
//
////////////////////////////////////////////////////////////////////


//////////////////////////////
// AsyncEventServer::OnAccept
// PARAMS: ...
// COMMENT: Called when a new client connects
bool AsyncEventServer::OnAccept(SocketInfoPtr& pSI) 
{
	return true;
}

/*bool ConvertMultiByteToWideChar(UINT codePage, char* pSource, int sourceLength, std::vector<wchar_t>& wideBuffer,
					int& countLeftovers)
{
	if(codePage == CP_UTF8) {
		countLeftovers = 0;
		//check from the end of pSource for ev. uncompleted UTF-8 byte sequence
		if(pSource[sourceLength-1] & 0x80) {
			//The last byte is part of a multibyte sequence. If the sequence is not complete, we need to save the partial sequence
			int bytesToCheck = std::min(4, sourceLength);	//a sequence contains a maximum of 4 bytes
			int currentLeftoverIndex = sourceLength-1;
			for(; bytesToCheck > 0; --bytesToCheck, --currentLeftoverIndex) {
				++countLeftovers;
				if(pSource[currentLeftoverIndex] & 0x80) {
					if(pSource[currentLeftoverIndex] & 0x40) { //The two high-bits are set, this is the "header"
						int expectedSequenceLength = 2;
						if(pSource[currentLeftoverIndex] & 0x20)
							++expectedSequenceLength;
						if(pSource[currentLeftoverIndex] & 0x10)
							++expectedSequenceLength;

						if(countLeftovers < expectedSequenceLength) {
							//The sequence is incomplete. Leave the leftovers to be interpreted with the next call
							break;
						}
						//The sequence is complete, there are no leftovers. 
						//...OR...
						//error. Let the conversion-function take the hit.
						countLeftovers = 0;
						break;
					}
				}
				else {
					//error. Let the conversion-function take the hit.
					countLeftovers = 0;
					break;
				}
			}
			if(countLeftovers == 4) {
				//error. Let the conversion-function take the hit.
				countLeftovers = 0;
			}
		}
	}

	int charsWritten = 0;
	int sourceBytesToProcess = sourceLength-countLeftovers;
	int wideBufferCapacity = MultiByteToWideChar(codePage, 0, pSource, sourceBytesToProcess, NULL, NULL);
	if(wideBufferCapacity > 0) 
	{
		wideBuffer.resize(wideBufferCapacity);
		charsWritten = MultiByteToWideChar(codePage, 0, pSource, sourceBytesToProcess, &wideBuffer[0], wideBuffer.size());
	}
	//copy the leftovers to the front of the buffer
	if(countLeftovers > 0) {
		memcpy(pSource, &(pSource[sourceBytesToProcess]), countLeftovers);
	}

	wideBuffer.resize(charsWritten);
	return (charsWritten > 0);
} */

//////////////////////////////
// AsyncEventServer::OnWrite
// PARAMS: ...
// COMMENT: Called when the socket is ready to send more data
void AsyncEventServer::OnWrite(SocketInfoPtr& pSI) {
//	DoSend(&pSI);
}

/*
bool ConvertWideCharToMultiByte(UINT codePage, const std::wstring& wideString, std::vector<char>& destBuffer)
{
	int bytesWritten = 0;
	int multibyteBufferCapacity = WideCharToMultiByte(codePage, 0, wideString.c_str(), static_cast<int>(wideString.length()), 0, 0, NULL, NULL);
	if(multibyteBufferCapacity > 0) 
	{
		destBuffer.resize(multibyteBufferCapacity);
		bytesWritten = WideCharToMultiByte(codePage, 0, wideString.c_str(), static_cast<int>(wideString.length()), &destBuffer[0], destBuffer.size(), NULL, NULL);
	}
	destBuffer.resize(bytesWritten);
	return (bytesWritten > 0);
}
*/

static void AsyncEventServer::handleWrite(SocketInfoPtr socketInfo, const std::wstring& data, int sock)
{
	CASPAR_LOG(info) << "InTO handleWrite";
	size_t totalbytesToSend = data.length(), totalSent = 0, index = 0, bytesToSend;

	if (!bytesToSend)
		return;

	CASPAR_LOG(info) << totalbytesToSend << L" need to Sent";

	bytesToSend = totalbytesToSend;

	while (totalSent < totalbytesToSend) {
		tbb::mutex::scoped_lock lock(socketInfo->mutex_);

		int sentBytes = send(sock, data[index], bytesToSend, 0);

		CASPAR_LOG(info) << sentBytes << L"bytes of data sent";

		if (sentBytes == -1) {
			int errorCode = errno;
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				CASPAR_LOG(debug) << L"Send to " << socketInfo->host_.c_str() << L"would block, sending later";
				break;
			} else {
				socketInfo->currentlySending_.resize(0);
				socketInfo->currentlySendingOffset_ = 0;
				socketInfo->sendQueue_.pop();
				break;
			}
		} else {
			if(sentBytes == bytesToSend) {		
				if(sentBytes < 512)
				{
					//boost::replace_all(socketInfo->sendQueue_.front(), L"\n", L"\\n");
					//boost::replace_all(socketInfo->sendQueue_.front(), L"\r", L"\\r");
					CASPAR_LOG(info) << L"Sent message to " << socketInfo->host_.c_str() << L": " << data;
				} else
				      CASPAR_LOG(info) << "Sent more than 512 bytes to " << socketInfo->host_.c_str();

				socketInfo->currentlySending_.resize(0);
				socketInfo->currentlySendingOffset_ = 0;
				socketInfo->sendQueue_.pop();
				break;
			} else {
				//socketInfo->currentlySendingOffset_ += sentBytes;
				totalSent += sentBytes;
				index += sentBytes;
				CASPAR_LOG(info) << "Sent partial message to " << socketInfo->host_.c_str();
			}
		}
	}
}

void AsyncEventServer::DoSend(SocketInfo& socketInfo) {
	//SocketInfoPtr ptr = static_cast<SocketInfoPtr>(socketInfo);
	//handleWrite(ptr);

	CASPAR_LOG(info) << L"into DoSend!!";
	return;

	//Locks the socketInfo-object so that no one else tampers with the sendqueue at the same time
	tbb::mutex::scoped_lock lock(mutex_);

	while(!socketInfo.sendQueue_.empty() || socketInfo.currentlySending_.size() > 0) {
		if(socketInfo.currentlySending_.size() == 0) {
			//Read the next string in the queue and convert to UTF-8
			/*if(!ConvertWideCharToMultiByte(pProtocolStrategy_->GetCodepage(), socketInfo.sendQueue_.front(), socketInfo.currentlySending_))
			{
	CASPAR_LOG(error) << "Send to " << socketInfo.host_.c_str() << TEXT(" failed, could not convert response to UTF-8");
			}*/
			socketInfo.currentlySendingOffset_ = 0;
		}

		if(socketInfo.currentlySending_.size() > 0) {
			int bytesToSend = static_cast<int>(socketInfo.currentlySending_.size()-socketInfo.currentlySendingOffset_);
			int sentBytes = send(socketInfo.socket_, &socketInfo.currentlySending_[0] + socketInfo.currentlySendingOffset_, bytesToSend, 0);
			//if(sentBytes == SOCKET_ERROR) {
			if(sentBytes == -1) {
				int errorCode = errno;// WSAGetLastError();
				if(errorCode == EWOULDBLOCK || errorCode == EAGAIN) {
					CASPAR_LOG(debug) << "Send to " << socketInfo.host_.c_str() << TEXT(" would block, sending later");
					break;
				}
				else {
					//LogSocketError(TEXT("Send"), errorCode);
					//OnError(socketInfo.event_, errorCode);
					socketInfo.currentlySending_.resize(0);
					socketInfo.currentlySendingOffset_ = 0;
					socketInfo.sendQueue_.pop();
					break;
				}
			}
			else {
				if(sentBytes == bytesToSend) {
					
					if(sentBytes < 512)
					{
						boost::replace_all(socketInfo.sendQueue_.front(), L"\n", L"\\n");
						boost::replace_all(socketInfo.sendQueue_.front(), L"\r", L"\\r");
						CASPAR_LOG(info) << L"Sent message to " << socketInfo.host_.c_str() << L": " << socketInfo.sendQueue_.front().c_str();
					}
					else
						CASPAR_LOG(info) << "Sent more than 512 bytes to " << socketInfo.host_.c_str();

					socketInfo.currentlySending_.resize(0);
					socketInfo.currentlySendingOffset_ = 0;
					socketInfo.sendQueue_.pop();
				}
				else {
					socketInfo.currentlySendingOffset_ += sentBytes;
					CASPAR_LOG(info) << "Sent partial message to " << socketInfo.host_.c_str();
				}
			}
		}
		else
			socketInfo.sendQueue_.pop();
	}
}


//////////////////////////////
// AsyncEventServer::OnClose
// PARAMS: ...
// COMMENT: Called when a client disconnects / is disconnected
void AsyncEventServer::OnClose(SocketInfoPtr& pSI) {
	CASPAR_LOG(info) << "Client " << pSI->host_.c_str() << L" was disconnected";

	socketInfoCollection_.RemoveSocketInfo(pSI);
}

//////////////////////////////
// AsyncEventServer::OnError
// PARAMS: ...
// COMMENT: Called when an errorcode is recieved
/*void AsyncEventServer::OnError(HANDLE waitEvent, int errorCode) {
	if(errorCode == WSAENETDOWN || errorCode == WSAECONNABORTED || errorCode == WSAECONNRESET || errorCode == WSAESHUTDOWN || errorCode == WSAETIMEDOUT || errorCode == WSAENOTCONN || errorCode == WSAENETRESET) {
		SocketInfoPtr pSocketInfo;
		if(socketInfoCollection_.FindSocketInfo(waitEvent, pSocketInfo)) {
			CASPAR_LOG(info) << "Client " << pSocketInfo->host_.c_str() << L" was disconnected, Errorcode " << errorCode;
		}

		socketInfoCollection_.RemoveSocketInfo(waitEvent);
	}
} */

//////////////////////////////
// AsyncEventServer::DisconnectClient
// PARAMS: ...
// COMMENT: The client is removed from the actual client-list when an FD_CLOSE notification is recieved
void AsyncEventServer::DisconnectClient(SocketInfo& socketInfo) {
	//int result = shutdown(socketInfo.socket_, SD_SEND);
	int result = shutdown(socketInfo.socket_, SHUT_RDWR);
	if(result == -1)
		CASPAR_LOG(info) << "Socket Shutdown failed (DisconnectClient)";
}

//////////////////////////////
// AsyncEventServer::LogSocketError
void AsyncEventServer::LogSocketError(const TCHAR* pStr, int socketError) {
//	if(socketError == 0)
//		socketError = WSAGetLastError();

	CASPAR_LOG(error) << "Failed to " << pStr << L" Errorcode: " << socketError;
}


//////////////////////////////
//  SocketInfoCollection
//////////////////////////////

AsyncEventServer::SocketInfoCollection::SocketInfoCollection() : bDirty_(false) {
}

AsyncEventServer::SocketInfoCollection::~SocketInfoCollection() {
}

bool AsyncEventServer::SocketInfoCollection::AddSocketInfo(SocketInfoPtr& pSocketInfo) {
	tbb::mutex::scoped_lock lock(mutex_);

	bool bSuccess = true;
	//bool bSuccess = socketInfoMap_.insert(it, pSocketInfo);
	if(bSuccess) {
		bDirty_ = true;
	}

	return bSuccess;
}

void AsyncEventServer::SocketInfoCollection::RemoveSocketInfo(SocketInfoPtr& pSocketInfo) {
//	if(pSocketInfo != 0) {
//		RemoveSocketInfo(pSocketInfo->event_);
//	}
}

void AsyncEventServer::SocketInfoCollection::RemoveSocketInfo(HANDLE waitEvent) {
	tbb::mutex::scoped_lock lock(mutex_);

	//Find instance
	SocketInfoPtr pSocketInfo;
	SocketInfoMap::iterator it = socketInfoMap_.find(waitEvent);
	SocketInfoMap::iterator end = socketInfoMap_.end();
	if(it != end)
		pSocketInfo = it->second;

	if(pSocketInfo) {
		pSocketInfo->pServer_ = NULL;

		socketInfoMap_.erase(waitEvent);

		HandleVector::iterator it = std::find(waitEvents_.begin(), waitEvents_.end(), waitEvent);
		if(it != waitEvents_.end()) {
			std::swap((*it), waitEvents_.back());
			waitEvents_.resize(waitEvents_.size()-1);

			bDirty_ = true;
		}
	}
	if(onSocketInfoRemoved)
		onSocketInfoRemoved(pSocketInfo);
}

bool AsyncEventServer::SocketInfoCollection::FindSocketInfo(HANDLE key, SocketInfoPtr& pResult) {
	tbb::mutex::scoped_lock lock(mutex_);

	SocketInfoMap::iterator it = socketInfoMap_.find(key);
	SocketInfoMap::iterator end = socketInfoMap_.end();
	if(it != end)
		pResult = it->second;

	return (it != end);
}

void AsyncEventServer::SocketInfoCollection::CopyCollectionToArray(HANDLE* pDest, int maxCount) {
	tbb::mutex::scoped_lock lock(mutex_);

	memcpy(pDest, &(waitEvents_[0]), std::min( maxCount, static_cast<int>(waitEvents_.size()) ) * sizeof(HANDLE) );
}

void AsyncEventServer::SocketInfoCollection::Clear() {
	tbb::mutex::scoped_lock lock(mutex_);

	socketInfoMap_.clear();
	waitEvents_.clear();
}

}	//namespace IO
}	//namespace caspar
