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

#include "../util/ProtocolStrategy.h"
#include <core/video_channel.h>
#include <core/thumbnail_generator.h>
#include <core/producer/media_info/media_info_repository.h>

#include "AMCPCommand.h"
#include "AMCPCommandQueue.h"

#include <boost/noncopyable.hpp>
#include <boost/thread/future.hpp>

#include <assert.h>

#define	UINT	unsigned int
#define	TCHAR	wchar_t

#define	_ASSERTE	assert

namespace caspar { namespace protocol { namespace amcp {

class AMCPProtocolStrategy : public IO::IProtocolStrategy, boost::noncopyable
{
	enum MessageParserState {
		New = 0,
		GetSwitch,
		GetCommand,
		GetParameters,
		GetChannel,
		Done
	};

	AMCPProtocolStrategy(const AMCPProtocolStrategy&);
	AMCPProtocolStrategy& operator=(const AMCPProtocolStrategy&);

public:
	AMCPProtocolStrategy(
			const std::wstring& name,
			const std::vector<safe_ptr<core::video_channel>>& channels,
			const std::shared_ptr<core::thumbnail_generator>& thumb_gen,
			const safe_ptr<core::media_info_repository>& media_info_repo,
			const safe_ptr<core::ogl_device>& ogl_device,
			const std::function<void (bool)>& shutdown_server_now);
	virtual ~AMCPProtocolStrategy();

	//virtual void Parse(const TCHAR* pData, int charCount, IO::ClientInfoPtr pClientInfo);
	virtual void Parse(const TCHAR* pData, int charCount, IO::ClientInfoPtr pClientInfo, const int sock=-1);
	virtual UINT GetCodepage() {
		return 65001;//CP_UTF8;	to avoid compile error
	}

	AMCPCommandPtr InterpretCommandString(const std::wstring& str, MessageParserState* pOutState=0);
	void Response(const std::wstring& msg, const int sock);

private:
	friend class AMCPCommand;

	void ProcessMessage(const std::wstring& message, IO::ClientInfoPtr& pClientInfo, int sock);
	std::size_t TokenizeMessage(const std::wstring& message, std::vector<std::wstring>* pTokenVector);
	AMCPCommandPtr CommandFactory(const std::wstring& str);

	bool QueueCommand(AMCPCommandPtr);

	std::vector<safe_ptr<core::video_channel>> channels_;
	std::shared_ptr<core::thumbnail_generator> thumb_gen_;
	safe_ptr<core::media_info_repository> media_info_repo_;
	safe_ptr<core::ogl_device> ogl_;
	std::function<void (bool)> shutdown_server_now_;
	std::vector<AMCPCommandQueuePtr> commandQueues_;
	static const std::wstring MessageDelimiter;
};

}}}
