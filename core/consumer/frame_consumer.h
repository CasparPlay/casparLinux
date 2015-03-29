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

#pragma once

#include <common/memory/safe_ptr.h>

#include <boost/noncopyable.hpp>
#include <boost/property_tree/ptree_fwd.hpp>
#include <boost/thread/future.hpp>

#include <functional>
#include <string>
#include <vector>

namespace caspar { namespace core {
	
class read_frame;
class parameters;
struct video_format_desc;
struct channel_layout;

struct frame_consumer : boost::noncopyable
{
	virtual ~frame_consumer() {}
	
	virtual boost::unique_future<bool> send(const safe_ptr<read_frame>& frame) = 0;
	virtual void initialize(const video_format_desc& format_desc, const channel_layout& audio_channel_layout, int channel_index) = 0;
	virtual int64_t presentation_frame_age_millis() const = 0;
	virtual std::wstring print() const = 0;
	virtual boost::property_tree::wptree info() const = 0;
	virtual bool has_synchronization_clock() const {return true;}
	virtual int buffer_depth() const = 0; // -1 to not participate in frame presentation synchronization
	virtual int index() const = 0;

	static const safe_ptr<frame_consumer>& empty();
};

safe_ptr<frame_consumer> create_consumer_cadence_guard(const safe_ptr<frame_consumer>& consumer);

typedef std::function<safe_ptr<core::frame_consumer>(const core::parameters&)> consumer_factory_t;

void register_consumer_factory(const consumer_factory_t& factory);
safe_ptr<core::frame_consumer> create_consumer(const core::parameters& params);

}}
