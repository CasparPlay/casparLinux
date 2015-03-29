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

#include "channel_producer.h"

#include "../../monitor/monitor.h"
#include "../../consumer/frame_consumer.h"
#include "../../consumer/output.h"
#include "../../video_channel.h"

#include "../frame/basic_frame.h"
#include "../frame/frame_factory.h"
#include "../../mixer/write_frame.h"
#include "../../mixer/read_frame.h"

#include <boost/thread/once.hpp>

#include <common/exception/exceptions.h>
//#include <common/memory/memcpy.h>
#include <common/concurrency/future_util.h>

#include <tbb/concurrent_queue.h>
#include <queue>

namespace caspar { namespace core {

class channel_consumer : public frame_consumer
{	
	tbb::concurrent_bounded_queue<std::shared_ptr<read_frame>>	frame_buffer_;
	core::video_format_desc						format_desc_;
	int								channel_index_;
	int								consumer_index_;
	tbb::atomic<bool>						is_running_;
	tbb::atomic<int64_t>						current_age_;
	boost::promise<void>						first_frame_promise_;
	boost::unique_future<void>					first_frame_available_;
	bool								first_frame_reported_;

public:
	channel_consumer()
		: consumer_index_(next_consumer_index())
		, first_frame_available_(first_frame_promise_.get_future())
		, first_frame_reported_(false)
	{
		is_running_ = true;
		current_age_ = 0;
		frame_buffer_.set_capacity(3);
	}

	static int next_consumer_index()
	{
		static tbb::atomic<int> consumer_index_counter;
		static boost::once_flag consumer_index_counter_initialized;

		boost::call_once(consumer_index_counter_initialized, [&]()
		{
			consumer_index_counter = 0;
		});

		return ++consumer_index_counter;
	}

	~channel_consumer()
	{
		stop();
	}

	// frame_consumer

	virtual boost::unique_future<bool> send(const safe_ptr<read_frame>& frame) override
	{
		bool pushed = frame_buffer_.try_push(frame);

		if (pushed && !first_frame_reported_)
		{
			first_frame_promise_.set_value();
			first_frame_reported_ = true;
		}

		return caspar::wrap_as_future(is_running_.load());
	}

	virtual void initialize(
			const video_format_desc& format_desc,
			const channel_layout& audio_channel_layout,
			int channel_index) override
	{
		format_desc_    = format_desc;
		channel_index_  = channel_index;
	}

	virtual int64_t presentation_frame_age_millis() const override
	{
		return current_age_;
	}

	virtual std::wstring print() const override
	{
		return L"[channel-consumer|" + boost::lexical_cast<std::wstring>(channel_index_) + L"]";
	}

	virtual boost::property_tree::wptree info() const override
	{
		boost::property_tree::wptree info;
		info.add(L"type", L"channel-consumer");
		info.add(L"channel-index", channel_index_);
		return info;
	}
	
	virtual bool has_synchronization_clock() const override
	{
		return false;
	}

	virtual int buffer_depth() const override
	{
		return -1;
	}

	virtual int index() const override
	{
		return 78500 + consumer_index_;
	}

	// channel_consumer

	void stop()
	{
		is_running_ = false;
		frame_buffer_.try_push(make_safe<read_frame>());
	}
	
	const core::video_format_desc& get_video_format_desc()
	{
		return format_desc_;
	}

	void block_until_first_frame_available()
	{
		if (!first_frame_available_.timed_wait(boost::posix_time::seconds(2)))
			CASPAR_LOG(warning)
					<< print() << L" Timed out while waiting for first frame";
	}

	std::shared_ptr<read_frame> receive()
	{
		if(!is_running_)
			return make_safe<read_frame>();
		std::shared_ptr<read_frame> frame;
		
		if (frame_buffer_.try_pop(frame))
			current_age_ = frame->get_age_millis();

		return frame;
	}
};
	
class channel_producer : public frame_producer
{
	monitor::subject			monitor_subject_;

	const safe_ptr<frame_factory>		frame_factory_;
	const safe_ptr<channel_consumer>	consumer_;

	std::queue<safe_ptr<basic_frame>>	frame_buffer_;
	safe_ptr<basic_frame>			last_frame_;
	uint64_t				frame_number_;

public:
	explicit channel_producer(const safe_ptr<frame_factory>& frame_factory, const safe_ptr<video_channel>& channel) 
		: frame_factory_(frame_factory)
		, consumer_(make_safe<channel_consumer>())
		, last_frame_(basic_frame::empty())
		, frame_number_(0)
	{
		channel->output()->add(consumer_);
		consumer_->block_until_first_frame_available();
		CASPAR_LOG(info) << print() << L" Initialized";
	}

	~channel_producer()
	{
		consumer_->stop();
		CASPAR_LOG(info) << print() << L" Uninitialized";
	}

	// frame_producer
	virtual safe_ptr<basic_frame> receive(int) override
	{
		auto format_desc = consumer_->get_video_format_desc();

		if(frame_buffer_.size() > 0)
		{
			auto frame = frame_buffer_.front();
			frame_buffer_.pop();
			return last_frame_ = std::move(frame); //comment out to fix compile failure
			//return frame;
		}
		
		auto read_frame = consumer_->receive();
		if(!read_frame || read_frame->image_data().empty())
			return basic_frame::late();		

		frame_number_++;
		
		core::pixel_format_desc desc;
		bool double_speed = std::abs(frame_factory_->get_video_format_desc().fps / 2.0 - format_desc.fps) < 0.01;		
		bool half_speed	= std::abs(format_desc.fps / 2.0 - frame_factory_->get_video_format_desc().fps) < 0.01;

		if(half_speed && frame_number_ % 2 == 0) // Skip frame
			return receive(0);

		desc.pix_fmt = core::pixel_format::bgra;
		desc.planes.push_back(core::pixel_format_desc::plane(format_desc.width, format_desc.height, 4));
		auto frame = frame_factory_->create_frame(this, desc, read_frame->multichannel_view().channel_layout());

		bool copy_audio = !double_speed && !half_speed;

		if (copy_audio)
		{
			frame->audio_data().reserve(read_frame->audio_data().size());
			boost::copy(read_frame->audio_data(), std::back_inserter(frame->audio_data()));
		}

//		fast_memcpy(frame->image_data().begin(), read_frame->image_data().begin(), read_frame->image_data().size());
		memcpy(frame->image_data().begin(), read_frame->image_data().begin(), read_frame->image_data().size());
		frame->commit();

		frame_buffer_.push(frame);	
		
		if(double_speed)	
			frame_buffer_.push(frame);

		return receive(0);
	}	

	virtual safe_ptr<basic_frame> last_frame() const override
	{
		return last_frame_; 
	}	

	virtual std::wstring print() const override
	{
		return L"channel[]";
	}

	virtual boost::property_tree::wptree info() const override
	{
		boost::property_tree::wptree info;
		info.add(L"type", L"channel-producer");
		return info;
	}

	monitor::subject& monitor_output() 
	{
		return monitor_subject_;
	}
};

safe_ptr<frame_producer> create_channel_producer(const safe_ptr<core::frame_factory>& frame_factory, const safe_ptr<video_channel>& channel)
{
	return create_producer_print_proxy(
			make_safe<channel_producer>(frame_factory, channel));
}

}}
