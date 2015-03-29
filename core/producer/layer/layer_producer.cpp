/*
* Copyright (c) 2011 Sveriges Television AB <info@casparcg.com>
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
* Author: Cambell Prince, cambell.prince@gmail.com
*/

#include "../../StdAfx.h"

#include "layer_producer.h"

#include "../../consumer/write_frame_consumer.h"
#include "../../consumer/output.h"
#include "../../video_channel.h"

#include "../stage.h"
#include "../frame/basic_frame.h"
#include "../frame/frame_factory.h"
#include "../../mixer/write_frame.h"
#include "../../mixer/read_frame.h"

#include <common/exception/exceptions.h>
//#include <common/memory/memcpy.h> blocked until implemented and fast_memcpy is replaced with memcpy
#include <common/concurrency/future_util.h>

#include <boost/format.hpp>

namespace caspar { namespace core {

class layer_consumer : public write_frame_consumer
{
//	To avoid compiler err with concurrent_bounded_queue std::queue is used - change later on.
	tbb::concurrent_bounded_queue<safe_ptr<basic_frame>>	frame_buffer_;
//	std::queue<safe_ptr<basic_frame>>			frame_buffer_;
	boost::promise<void>					first_frame_promise_;
	boost::unique_future<void>				first_frame_available_;
	bool	first_frame_reported_;

public:
	layer_consumer() : first_frame_reported_(false)
	{
		first_frame_available_ = first_frame_promise_.get_future();
		frame_buffer_.set_capacity(2);
	}

	~layer_consumer()
	{
	}

	// write_frame_consumer

	virtual void send(const safe_ptr<basic_frame>& src_frame) override
	{
		bool pushed = frame_buffer_.try_push(src_frame);
//		frame_buffer_.push(src_frame);

		if (pushed && !first_frame_reported_) //changed to fix compilation
		//if (!first_frame_reported_)
		{
			first_frame_promise_.set_value();
			first_frame_reported_ = true;
		}
	}

	virtual std::wstring print() const override
	{
		return L"[layer_consumer]";
	}

	safe_ptr<basic_frame> receive()
	{
//		comment out to avoid compiler err
		safe_ptr<basic_frame> frame;
//		bool ispoped = frame_buffer_.try_pop(frame);
//		if (frame_buffer_.size() > 0) {
			//frame_buffer_.pop();
//			return basic_frame::late();
//		}
		if (!frame_buffer_.try_pop(frame))
		{
			return basic_frame::late();
		}
		return frame;
	}

	void block_until_first_frame_available()
	{
		if (!first_frame_available_.timed_wait(boost::posix_time::seconds(2)))
			CASPAR_LOG(warning) << print() << L" Timed out while waiting for first frame";
	}
};

class layer_producer : public frame_producer
{
	monitor::subject			monitor_subject_;

	const safe_ptr<frame_factory>		frame_factory_;
	int					layer_;
	const std::shared_ptr<layer_consumer>	consumer_;
	safe_ptr<basic_frame>			last_frame_;
	uint64_t				frame_number_;
	const safe_ptr<stage>			stage_;

public:
	explicit layer_producer(const safe_ptr<frame_factory>& frame_factory, const safe_ptr<stage>& stage, int layer) 
		: frame_factory_(frame_factory)
		, stage_(stage)
		, consumer_(new layer_consumer())
		, last_frame_(basic_frame::empty())
		, frame_number_(0)
	{
		layer_ = layer;
		stage_->add_layer_consumer(this, layer_, consumer_);
		consumer_->block_until_first_frame_available();
		CASPAR_LOG(info) << print() << L" Initialized";
	}

	~layer_producer()
	{
		stage_->remove_layer_consumer(this, layer_);
		CASPAR_LOG(info) << print() << L" Uninitialized";
	}

	// frame_producer
	virtual safe_ptr<basic_frame> receive(int) override
	{
		auto consumer_frame = consumer_->receive();
		if (consumer_frame == basic_frame::late())
			return last_frame_;

		frame_number_++;
		return consumer_frame;
	}

	virtual safe_ptr<basic_frame> last_frame() const override
	{
		return last_frame_; 
	}	

	virtual std::wstring print() const override
	{
		return L"layer-producer[" + boost::lexical_cast<std::wstring>(layer_) + L"]";
	}

	virtual boost::property_tree::wptree info() const override
	{
		boost::property_tree::wptree info;
		info.add(L"type", L"layer-producer");
		return info;
	}

	monitor::subject& monitor_output() 
	{
		return monitor_subject_;
	}

};

safe_ptr<frame_producer> create_layer_producer(const safe_ptr<core::frame_factory>& frame_factory,
								const safe_ptr<stage>& stage, int layer)
{
	return create_producer_print_proxy(make_safe<layer_producer>(frame_factory, stage, layer));
}

}}
