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
 
#include "AMCPProtocolStrategy.h"

#include "../util/AsyncEventServer.h"
#include "AMCPCommandsImpl.h"

#include <stdio.h>
//#include <crtdbg.h>
#include <string.h>
#include <algorithm>
#include <cctype>

#include <sys/types.h>
#include <sys/socket.h>

#include <boost/algorithm/string/trim.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/lexical_cast.hpp>

namespace caspar { namespace protocol { namespace amcp {

using IO::ClientInfoPtr;

const std::wstring AMCPProtocolStrategy::MessageDelimiter = L"\r\n";

inline std::shared_ptr<core::video_channel> GetChannelSafe(unsigned int index, 
						const std::vector<safe_ptr<core::video_channel>>& channels)
{
	return index < channels.size() ? std::shared_ptr<core::video_channel>(channels[index]) : nullptr;
}

std::wstring TEXT(const char* src)
{
	return std::wstring(src, src + strlen(src));
}

AMCPProtocolStrategy::AMCPProtocolStrategy(
		const std::wstring& name,
		const std::vector<safe_ptr<core::video_channel>>& channels,
		const std::shared_ptr<core::thumbnail_generator>& thumb_gen,
		const safe_ptr<core::media_info_repository>& media_info_repo,
		const safe_ptr<core::ogl_device>& ogl_device,
		const std::function<void (bool)>& shutdown_server_now)
	: channels_(channels), thumb_gen_(thumb_gen), media_info_repo_(media_info_repo), ogl_(ogl_device)
	, shutdown_server_now_(shutdown_server_now)
{
	AMCPCommandQueuePtr pGeneralCommandQueue(new AMCPCommandQueue(L"General Queue for " + name));
	commandQueues_.push_back(pGeneralCommandQueue);


	std::shared_ptr<core::video_channel> pChannel;
	unsigned int index = -1;
	//Create a commandpump for each video_channel
	while((pChannel = GetChannelSafe(++index, channels_)) != 0) {
		AMCPCommandQueuePtr pChannelCommandQueue(new AMCPCommandQueue(L"Channel " + boost::lexical_cast<std::wstring>(index + 1) + L" for " + name));
		std::wstring title = L"video_channel ";

		//HACK: Perform real conversion from int to string
		TCHAR num = L'1'+static_cast<TCHAR>(index);
		title += num;
		
		commandQueues_.push_back(pChannelCommandQueue);
	}
}

AMCPProtocolStrategy::~AMCPProtocolStrategy() {
}

//void AMCPProtocolStrategy::Parse(const TCHAR* pData, int charCount, ClientInfoPtr pClientInfo)
void AMCPProtocolStrategy::Parse(const TCHAR* pData, int charCount, ClientInfoPtr pClientInfo, const int sock)
{
	size_t pos;
	size_t oldLength = pClientInfo->currentMessage_.length();

	if(pClientInfo->currentMessage_.capacity() < (oldLength + charCount))
		pClientInfo->currentMessage_.reserve(oldLength + 8192 * 4);

	pClientInfo->currentMessage_.append(pData, charCount);

	while(true) {
		pos = pClientInfo->currentMessage_.find(MessageDelimiter, oldLength > MessageDelimiter.size() - 1
						? oldLength - (MessageDelimiter.size() - 1) : 0);

		if(pos != std::wstring::npos)
		{
			int spos = 0;
			while (!iswalpha(pClientInfo->currentMessage_[spos])) {
				if (spos >= charCount)
					return;
				spos++;
			}
			
			std::wstring message = pClientInfo->currentMessage_.substr(spos, pos);

			//This is where a complete message gets taken care of
			if(message.length() > 0) {
				CASPAR_LOG(info) << L"message is: " << message;
				//ProcessMessage(message, pClientInfo);
				ProcessMessage(message, pClientInfo, sock);
			}

			std::size_t nextStartPos = pos + MessageDelimiter.length();
			if(nextStartPos < pClientInfo->currentMessage_.length())
			{
				pClientInfo->currentMessage_ = pClientInfo->currentMessage_.substr(nextStartPos);
				oldLength = 0;
			}
			else
			{
				pClientInfo->currentMessage_.clear();
				break;
			}
		}
		else
		{
			break;
		}
	}
}

void AMCPProtocolStrategy::ProcessMessage(const std::wstring& message, ClientInfoPtr& pClientInfo, const int sock)
{	
	if(message.length() < 512)
		CASPAR_LOG(info) << L"Received message from " << pClientInfo->print() << L": " << message << L"\\r\\n";
	else
		CASPAR_LOG(info) << L"Received long message from " << pClientInfo->print() << L": " << message.substr(0, 510) << L" [...]\\r\\n";
	
	bool bError = true;
	MessageParserState state = New;

	AMCPCommandPtr pCommand;

	pCommand = InterpretCommandString(message, &state);

	if(pCommand != 0) {
		pCommand->SetClientInfo(pClientInfo);	//changed due to avoid compiler waringin
//		pCommand->SetClientInfo();
		if(QueueCommand(pCommand))
			bError = false;
		else
			state = GetChannel;
	}

	if (bError == true) {
		std::wstringstream answer;

		switch (state)
		{
			case GetCommand:
				answer << TEXT("400 ERROR\r\n") + message << "\r\n";
				break;
			case GetChannel:
				answer << TEXT("401 ERROR\r\n");
				break;
			case GetParameters:
				answer << TEXT("402 ERROR\r\n");
				break;
			default:
				answer << TEXT("500 FAILED\r\n");
				break;
		}

		//CASPAR_LOG(info) << L"Sending response to the client";
		pClientInfo->Send(answer.str(), sock);
	} 
}

void AMCPProtocolStrategy::Response(const std::wstring& data, const int sock)
{
	size_t totalbytesToSend = data.length(), totalSent = 0, index = 0, bytesToSend;

	//if (!bytesToSend)
	//	return;

	CASPAR_LOG(info) << totalbytesToSend << L" need to Sent";

	bytesToSend = totalbytesToSend;

	while (totalSent < totalbytesToSend) {
		//tbb::mutex::scoped_lock lock(socketInfo->mutex_);

		int sentBytes = send(sock, data[index], bytesToSend, 0);

		CASPAR_LOG(info) << sentBytes << L"bytes of data sent";

		if (sentBytes == -1) {
			int errorCode = errno;
			CASPAR_LOG(debug) <<  L"Failed to sent " << errorCode;
			if(errno == EAGAIN || errno == EWOULDBLOCK) {
				CASPAR_LOG(debug) <<  L"would block, sending later";
				break;
			} else {
				break;
			}
		} else {
			if(sentBytes == bytesToSend) {		
				if(sentBytes < 512)
				{
					//boost::replace_all(socketInfo->sendQueue_.front(), L"\n", L"\\n");
					//boost::replace_all(socketInfo->sendQueue_.front(), L"\r", L"\\r");
					CASPAR_LOG(info) << L"Sent message to " << data;
				}
				break;
			} else {
				//socketInfo->currentlySendingOffset_ += sentBytes;
				totalSent += sentBytes;
				index += sentBytes;
				CASPAR_LOG(info) << "Sent partial message to ";
			}
		}
	}

}

AMCPCommandPtr AMCPProtocolStrategy::InterpretCommandString(const std::wstring& message, MessageParserState* pOutState)
{
	std::vector<std::wstring> tokens;
	unsigned int currentToken = 0;
	std::wstring commandSwitch;

	AMCPCommandPtr pCommand;
	MessageParserState state = New;

	std::size_t tokensInMessage = TokenizeMessage(message, &tokens);

	CASPAR_LOG(info) << L"Interpreting command => " << message;

	//parse the message one token at the time
	while(currentToken < tokensInMessage)
	{
		switch(state)
		{
		case New:
			//if(tokens[currentToken][0] == TEXT('/'))
			if(tokens[currentToken][0] == L'/')
				state = GetSwitch;
			else
				state = GetCommand;
			break;

		case GetSwitch:
			commandSwitch = tokens[currentToken];
			state = GetCommand;
			++currentToken;
			break;

		case GetCommand:
			pCommand = CommandFactory(tokens[currentToken]);
			CASPAR_LOG(info) << L"GetCommand " << pCommand;

			if(pCommand == 0) {
				goto ParseFinnished;
			}
			else
			{
				pCommand->SetChannels(channels_);
				pCommand->SetThumbGenerator(thumb_gen_);
				pCommand->SetMediaInfoRepo(media_info_repo_);
				pCommand->SetOglDevice(ogl_);
				pCommand->SetShutdownServerNow(shutdown_server_now_);
				//Set scheduling
				if(commandSwitch.size() > 0) {
					transform(commandSwitch.begin(), commandSwitch.end(), commandSwitch.begin(), toupper);

					if(commandSwitch == TEXT("/APP"))
						pCommand->SetScheduling(AddToQueue);
				}

				if(pCommand->NeedChannel())
					state = GetChannel;
				else
					state = GetParameters;
			}
			++currentToken;
			break;

		case GetParameters:
			{
				_ASSERTE(pCommand != 0);
				int parameterCount=0;
				while (currentToken < tokensInMessage)
				{
					pCommand->AddParameter(tokens[currentToken++]);
					++parameterCount;
				}

				if(parameterCount < pCommand->GetMinimumParameters()) {
					goto ParseFinnished;
				}

				state = Done;
				break;
			}

		case GetChannel:
			{
//				assert(pCommand != 0);

				std::wstring str = boost::trim_copy(tokens[currentToken]);
				std::vector<std::wstring> split;
				boost::split(split, str, boost::is_any_of("-"));
					
				int channelIndex = -1;
				int layerIndex = -1;
				try
				{
					channelIndex = boost::lexical_cast<int>(split[0]) - 1;

					if(split.size() > 1)
						layerIndex = boost::lexical_cast<int>(split[1]);
				}
				catch(...)
				{
					goto ParseFinnished;
				}

				std::shared_ptr<core::video_channel> pChannel = GetChannelSafe(channelIndex, channels_);
				if(pChannel == 0) {
					goto ParseFinnished;
				}

				pCommand->SetChannel(pChannel);
				pCommand->SetChannelIndex(channelIndex);
				pCommand->SetLayerIntex(layerIndex);

				state = GetParameters;
				++currentToken;
				break;
			}

		default:	//Done and unexpected
			goto ParseFinnished;
		}
	}

ParseFinnished:
	if(state == GetParameters && pCommand->GetMinimumParameters()==0)
		state = Done;

	if(state != Done) {
		pCommand.reset();
	}

	if(pOutState != 0) {
		*pOutState = state;
	}

	return pCommand;
}

bool AMCPProtocolStrategy::QueueCommand(AMCPCommandPtr pCommand) {
	if(pCommand->NeedChannel()) {
		CASPAR_LOG(info) << "QueueCommand NeedChannel...";
		unsigned int channelIndex = pCommand->GetChannelIndex() + 1;
		if(commandQueues_.size() > channelIndex) {
			CASPAR_LOG(info) << "QueueCommand AddingCommand on channelIndex...";
			commandQueues_[channelIndex]->AddCommand(pCommand);
		}
		else {
			CASPAR_LOG(info) << "QueueCommand returning false";
			return false;
		}
	}
	else {
		commandQueues_[0]->AddCommand(pCommand);
	}
	return true;
}

AMCPCommandPtr AMCPProtocolStrategy::CommandFactory(const std::wstring& str)
{
	CASPAR_LOG(info) << L"CommandFactory " << str;
	std::wstring s = str;
	transform(s.begin(), s.end(), s.begin(), toupper);
	
	if 	(s == TEXT("MIXER"))			return std::make_shared<MixerCommand>();
	else if(s == TEXT("DIAG"))			return std::make_shared<DiagnosticsCommand>();
	else if(s == TEXT("CHANNEL_GRID"))		return std::make_shared<ChannelGridCommand>();
	else if(s == TEXT("CALL"))			return std::make_shared<CallCommand>();
	else if(s == TEXT("SWAP"))			return std::make_shared<SwapCommand>();
	else if(s == TEXT("ROUTE"))			return std::make_shared<RouteCommand>();
	else if(s == TEXT("LOAD"))			return std::make_shared<LoadCommand>();
	else if(s == TEXT("LOADBG"))			return std::make_shared<LoadbgCommand>();
	else if(s == TEXT("ADD"))			return std::make_shared<AddCommand>();
	else if(s == TEXT("REMOVE"))			return std::make_shared<RemoveCommand>();
	else if(s == TEXT("PAUSE"))			return std::make_shared<PauseCommand>();
	else if(s == TEXT("RESUME"))			return std::make_shared<ResumeCommand>();
	else if(s == TEXT("PLAY"))			return std::make_shared<PlayCommand>();
	else if(s == TEXT("STOP"))			return std::make_shared<StopCommand>();
	else if(s == TEXT("CLEAR"))			return std::make_shared<ClearCommand>();
	else if(s == TEXT("PRINT"))			return std::make_shared<PrintCommand>();
	else if(s == TEXT("LOG"))			return std::make_shared<LogCommand>();
//	else if(s == TEXT("CG"))			return std::make_shared<CGCommand>(); we're not dealing with CG ATM
	else if(s == TEXT("DATA"))			return std::make_shared<DataCommand>();
	else if(s == TEXT("CINF"))			return std::make_shared<CinfCommand>();
	else if(s == TEXT("INFO"))			return std::make_shared<InfoCommand>(channels_);
	else if(s == TEXT("CLS"))			return std::make_shared<ClsCommand>();
	else if(s == TEXT("TLS"))			return std::make_shared<TlsCommand>();
	else if(s == TEXT("VERSION"))			return std::make_shared<VersionCommand>();
	else if(s == TEXT("BYE"))			return std::make_shared<ByeCommand>();
	else if(s == TEXT("SET"))			return std::make_shared<SetCommand>();
	else if(s == TEXT("GL"))			return std::make_shared<GlCommand>();
	else if(s == TEXT("THUMBNAIL"))			return std::make_shared<ThumbnailCommand>();
	//else if(s == TEXT("MONITOR"))
	//{
	//	result = AMCPCommandPtr(new MonitorCommand());
	//}
	else if(s == TEXT("KILL"))			return std::make_shared<KillCommand>();
	else if(s == TEXT("RESTART"))			return std::make_shared<RestartCommand>();
	return nullptr;
}

std::size_t AMCPProtocolStrategy::TokenizeMessage(const std::wstring& message, std::vector<std::wstring>* pTokenVector)
{
	//split on whitespace but keep strings within quotationmarks
	//treat \ as the start of an escape-sequence: the following char will indicate what to actually put in the string

	std::wstring currentToken;

	char inQuote = 0;
	bool getSpecialCode = false;

	for(unsigned int charIndex=0; charIndex<message.size(); ++charIndex)
	{
		if(getSpecialCode)
		{
			//insert code-handling here
			switch(message[charIndex])
			{
//			case TEXT('\\'):
			case L'\\':
				currentToken += L"\\";
				break;
			case L'\"':
				currentToken += L"\"";
				break;
			case L'n':
				currentToken += L"\n";
				break;
			default:
				break;
			};
			getSpecialCode = false;
			continue;
		}

		if(message[charIndex]==L'\\')
		{
			getSpecialCode = true;
			continue;
		}

		if(message[charIndex]==' ' && inQuote==false)
		{
			if(currentToken.size()>0)
			{
				pTokenVector->push_back(currentToken);
				currentToken.clear();
			}
			continue;
		}

		if(message[charIndex]=='\"')
		{
			inQuote ^= 1;

			if(currentToken.size()>0)
			{
				pTokenVector->push_back(currentToken);
				currentToken.clear();
			}
			continue;
		}

		currentToken += message[charIndex];
	}

	if(currentToken.size()>0)
	{
		pTokenVector->push_back(currentToken);
		currentToken.clear();
	}

	return pTokenVector->size();
}

}	//namespace amcp
}}	//namespace caspar
