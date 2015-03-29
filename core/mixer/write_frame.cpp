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

#include "../StdAfx.h"

#include "write_frame.h"

#include "gpu/ogl_device.h"
#include "gpu/host_buffer.h"
#include "gpu/device_buffer.h"

#include <core/producer/frame/frame_visitor.h>
#include <core/producer/frame/pixel_format.h>
#include <core/mixer/audio/audio_util.h>

#include <boost/lexical_cast.hpp>
#include <boost/timer.hpp>

#include <tbb/atomic.h>

namespace caspar { namespace core {
																																							
struct write_frame::implementation
{				
	std::shared_ptr<ogl_device>			ogl_;
	std::vector<std::shared_ptr<host_buffer>>	buffers_;
	std::vector<safe_ptr<device_buffer>>		textures_;
	audio_buffer					audio_data_;
	const core::pixel_format_desc			desc_;
	const channel_layout				channel_layout_;
	const void*					tag_;
	core::field_mode::type				mode_;
	boost::timer					since_created_timer_;
	tbb::atomic<int64_t>				recorded_frame_age_;

	implementation(const void* tag, const channel_layout& channel_layout)
		: channel_layout_(channel_layout)
		, tag_(tag)
	{
		recorded_frame_age_ = -1;
	}

	implementation(const safe_ptr<ogl_device>& ogl, const void* tag, const core::pixel_format_desc& desc, const channel_layout& channel_layout, bool mipmapping) 
		: ogl_(ogl)
		, desc_(desc)
		, channel_layout_(channel_layout)
		, tag_(tag)
		, mode_(core::field_mode::progressive)
	{
		std::transform(desc.planes.begin(), desc.planes.end(), std::back_inserter(buffers_), [&](const core::pixel_format_desc::plane& plane)
		{
			return ogl_->create_host_buffer(plane.size, host_buffer::write_only);
		});
		std::transform(desc.planes.begin(), desc.planes.end(), std::back_inserter(textures_), [&](const core::pixel_format_desc::plane& plane)
		{
			return ogl_->create_device_buffer(plane.width, plane.height, plane.channels, mipmapping);	
		});

		recorded_frame_age_ = -1;
	}
			
	void accept(write_frame& self, core::frame_visitor& visitor)
	{
		visitor.begin(self);
		visitor.visit(self);
		visitor.end();
	}

	int64_t get_and_record_age_millis()
	{
		if (recorded_frame_age_ == -1)
			recorded_frame_age_ = static_cast<int64_t>(since_created_timer_.elapsed() * 1000.0);

		return recorded_frame_age_;
	}

	boost::iterator_range<uint8_t*> image_data(size_t index)
	{
		if(index >= buffers_.size() || !buffers_[index]->data())
			return boost::iterator_range<uint8_t*>();
		auto ptr = static_cast<uint8_t*>(buffers_[index]->data());
		return boost::iterator_range<uint8_t*>(ptr, ptr+buffers_[index]->size());
	}
	
	void commit()
	{
		for(size_t n = 0; n < buffers_.size(); ++n)
			commit(n);
	}

	void commit(size_t plane_index)
	{
		if(plane_index >= buffers_.size())
			return;
				
		auto buffer = std::move(buffers_[plane_index]); // Release buffer once done.

		if(!buffer)
			return;

		auto texture = textures_.at(plane_index);
		
		ogl_->begin_invoke([=]
		{			
			buffer->unmap();
			buffer->bind();
			texture->begin_read();
			buffer->unbind();
		}, high_priority);
	}
};
	
write_frame::write_frame(const void* tag, const channel_layout& channel_layout)
	: impl_(new implementation(tag, channel_layout))
{
}
write_frame::write_frame(
		const safe_ptr<ogl_device>& ogl,
		const void* tag,
		const core::pixel_format_desc& desc,
		const channel_layout& channel_layout,
		bool mipmapping)
	: impl_(new implementation(ogl, tag, desc, channel_layout, mipmapping))
{
}
write_frame::write_frame(const write_frame& other) : impl_(new implementation(*other.impl_)){}
write_frame::write_frame(write_frame&& other) : impl_(std::move(other.impl_)){}
write_frame& write_frame::operator=(const write_frame& other)
{
	basic_frame temp(other); // TODO: This is nonsense...
	temp.swap(*this);
	return *this;
}
write_frame& write_frame::operator=(write_frame&& other)
{
	write_frame temp(std::move(other));
	temp.swap(*this);
	return *this;
}
void write_frame::swap(write_frame& other){impl_.swap(other.impl_);}

boost::iterator_range<uint8_t*> write_frame::image_data(size_t index){return impl_->image_data(index);}
audio_buffer& write_frame::audio_data() { return impl_->audio_data_; }
const void* write_frame::tag() const {return impl_->tag_;}
const core::pixel_format_desc& write_frame::get_pixel_format_desc() const{return impl_->desc_;}
const channel_layout& write_frame::get_channel_layout() const{return impl_->channel_layout_;}
multichannel_view<int32_t, audio_buffer::iterator> write_frame::get_multichannel_view()
{
	return make_multichannel_view<int32_t>(impl_->audio_data_.begin(), impl_->audio_data_.end(), impl_->channel_layout_);
}
const std::vector<safe_ptr<device_buffer>>& write_frame::get_textures() const {return impl_->textures_;}
void write_frame::commit(size_t plane_index) {impl_->commit(plane_index);}
void write_frame::commit(){impl_->commit();}
void write_frame::set_type(const field_mode::type& mode){impl_->mode_ = mode;}
core::field_mode::type write_frame::get_type() const{return impl_->mode_;}
void write_frame::accept(core::frame_visitor& visitor){impl_->accept(*this, visitor);}
int64_t write_frame::get_and_record_age_millis() { return impl_->get_and_record_age_millis(); }

}}
