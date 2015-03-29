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

#include <core/producer/frame/basic_frame.h>
#include <core/video_format.h>
#include <core/mixer/audio/audio_mixer.h>
#include <core/mixer/audio/audio_util.h>

#include <boost/noncopyable.hpp>
#include <boost/range/iterator_range.hpp>

#include <cstdint>
#include <vector>

namespace caspar { namespace core {

class device_buffer;
struct frame_visitor;
struct pixel_format_desc;
class ogl_device;	

class write_frame : public core::basic_frame, boost::noncopyable
{
public:	
	explicit write_frame(const void* tag, const channel_layout& channel_layout);
	explicit write_frame(const safe_ptr<ogl_device>& ogl, const void* tag, const core::pixel_format_desc& desc, const channel_layout& channel_layout, bool mipmapping);

	write_frame(const write_frame& other);
	write_frame(write_frame&& other);

	write_frame& operator=(const write_frame& other);
	write_frame& operator=(write_frame&& other);
			
	// basic_frame

	virtual void accept(frame_visitor& visitor) override;
	virtual int64_t get_and_record_age_millis() override;

	// write _frame

	void swap(write_frame& other);
			
	boost::iterator_range<uint8_t*> image_data(size_t plane_index = 0);	
	audio_buffer& audio_data();
	
	void commit(size_t plane_index);
	void commit();
	
	void set_type(const field_mode::type& mode);
	field_mode::type get_type() const;
	
	const void* tag() const;

	const core::pixel_format_desc& get_pixel_format_desc() const;
	const channel_layout& get_channel_layout() const;
	multichannel_view<int32_t, audio_buffer::iterator> get_multichannel_view();
private:
	friend class image_mixer;
	
	const std::vector<safe_ptr<device_buffer>>& get_textures() const;

	struct implementation;
	safe_ptr<implementation> impl_;
};


}}
