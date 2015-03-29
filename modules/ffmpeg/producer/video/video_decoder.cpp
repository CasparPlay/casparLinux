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
* Author: Robert Nagy, ronag89@gmail.com
*/

#include "../../StdAfx.h"

#include "video_decoder.h"

#include "../util/util.h"

#include "../../ffmpeg_error.h"

#include <core/producer/frame/frame_transform.h>
#include <core/producer/frame/frame_factory.h>

#include <boost/range/algorithm_ext/push_back.hpp>
#include <boost/filesystem.hpp>

#include <queue>

extern "C" 
{
	#include <libavcodec/avcodec.h>
	#include <libavformat/avformat.h>
}

namespace caspar { namespace ffmpeg {
	
struct video_decoder::implementation : boost::noncopyable
{
	int						index_;
	const safe_ptr<AVCodecContext>			codec_context_;

	std::queue<safe_ptr<AVPacket>>			packets_;
	
	const uint32_t					nb_frames_;

	const size_t					width_;
	const size_t					height_;
	bool						is_progressive_;
	tbb::atomic<size_t>				file_frame_number_;

public:
	explicit implementation(const safe_ptr<AVFormatContext>& context) 
		: codec_context_(open_codec(*context, AVMEDIA_TYPE_VIDEO, index_))
		, nb_frames_(static_cast<uint32_t>(context->streams[index_]->nb_frames))
		, width_(codec_context_->width)
		, height_(codec_context_->height)
	{
		file_frame_number_ = 0;

		codec_context_->refcounted_frames = 1;
	}

	void push(const std::shared_ptr<AVPacket>& packet)
	{
		if(!packet)
			return;

		if(packet->stream_index == index_ || packet->data == nullptr)
			packets_.push(make_safe_ptr(packet));
	}

	std::shared_ptr<AVFrame> poll()
	{		
		if(packets_.empty())
			return nullptr;
		
		auto packet = packets_.front();
					
		if(packet->data == nullptr)
		{			
			if(codec_context_->codec->capabilities & CODEC_CAP_DELAY)
			{
				auto video = decode(packet);
				if(video)
					return video;
			}
					
			packets_.pop();
			file_frame_number_ = static_cast<size_t>(packet->pos);
			avcodec_flush_buffers(codec_context_.get());
			return flush_video();	
		}
			
		packets_.pop();
		return decode(packet);
	}

	std::shared_ptr<AVFrame> decode(safe_ptr<AVPacket> pkt)
	{
		auto decoded_frame = std::shared_ptr<AVFrame>(av_frame_alloc(), [](AVFrame* frame)
		{
			av_frame_free(&frame);
		});
		
		int frame_finished = 0;
		THROW_ON_ERROR2(avcodec_decode_video2(codec_context_.get(), decoded_frame.get(), &frame_finished, pkt.get()), "[video_decoder]");
		//avcodec_decode_video2(codec_context_.get(), decoded_frame.get(), &frame_finished, pkt.get());
		
		// If a decoder consumes less then the whole packet then something is wrong
		// that might be just harmless padding at the end, or a problem with the
		// AVParser or demuxer which puted more then one frame in a AVPacket.

		CASPAR_LOG(info) << L"[video-decoding done";

		if(frame_finished == 0)	
			return nullptr;

		is_progressive_ = !decoded_frame->interlaced_frame;

		if(decoded_frame->repeat_pict > 0)
			CASPAR_LOG(warning) << "[video_decoder] Field repeat_pict not implemented.";
		
		++file_frame_number_;

		// This ties the life of the decoded_frame to the packet that it came from. For the
		// current version of ffmpeg (0.8 or c17808c) the RAW_VIDEO codec returns frame data
		// owned by the packet.
		return std::shared_ptr<AVFrame>(decoded_frame.get(), [decoded_frame, pkt](AVFrame*){});
	}
	
	bool ready() const
	{
		return packets_.size() >= 8;
	}

	uint32_t nb_frames() const
	{
		return std::max<uint32_t>(nb_frames_, file_frame_number_);
	}

	std::wstring print() const
	{		
		return L"[video-decoder] " + widen(codec_context_->codec->long_name);
	}
};

video_decoder::video_decoder(const safe_ptr<AVFormatContext>& context) : impl_(new implementation(context)){}
void video_decoder::push(const std::shared_ptr<AVPacket>& packet){impl_->push(packet);}
std::shared_ptr<AVFrame> video_decoder::poll(){return impl_->poll();}
bool video_decoder::ready() const{return impl_->ready();}
size_t video_decoder::width() const{return impl_->width_;}
size_t video_decoder::height() const{return impl_->height_;}
uint32_t video_decoder::nb_frames() const{return impl_->nb_frames();}
uint32_t video_decoder::file_frame_number() const{return impl_->file_frame_number_;}
bool	video_decoder::is_progressive() const{return impl_->is_progressive_;}
std::wstring video_decoder::print() const{return impl_->print();}

}}
