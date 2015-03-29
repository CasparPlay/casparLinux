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

#include "host_buffer.h"

#include "fence.h"
#include "device_buffer.h"
#include "ogl_device.h"

#include <common/exception/exceptions.h>
#include <common/gl/gl_check.h>

#include <GL/glew.h>

#include <tbb/atomic.h>

#include <boost/property_tree/ptree.hpp>

namespace caspar { namespace core {

static tbb::atomic<int> g_w_instance_id;
static tbb::atomic<int> g_w_total_count;
static tbb::atomic<int> g_w_total_size;
static tbb::atomic<int> g_r_instance_id;
static tbb::atomic<int> g_r_total_count;
static tbb::atomic<int> g_r_total_size;
																																								
struct host_buffer::implementation : boost::noncopyable
{
	int				instance_id_;
	GLuint			pbo_;
	const size_t	size_;
	void*			data_;
	usage_t			usage_;
	GLenum			target_;
	fence			fence_;

public:
	implementation(size_t size, usage_t usage) 
		: instance_id_(++(usage == write_only ? g_w_instance_id : g_r_instance_id))
		, size_(size)
		, data_(nullptr)
		, pbo_(0)
		, target_(usage == write_only ? GL_PIXEL_UNPACK_BUFFER : GL_PIXEL_PACK_BUFFER)
		, usage_(usage)
	{
		GL(glGenBuffers(1, &pbo_));
		GL(glBindBuffer(target_, pbo_));
		GL(glBufferData(target_, size_, NULL, usage_ == write_only ? GL_STREAM_DRAW : GL_STREAM_READ));
		GL(glBindBuffer(target_, 0));

		if(!pbo_)
			BOOST_THROW_EXCEPTION(caspar_exception() << msg_info("Failed to allocate buffer."));

		++(usage_ == write_only ? g_w_total_count : g_r_total_count);
		auto total_size = (usage_ == write_only ? g_w_total_size : g_r_total_size) += size_;
		CASPAR_LOG(trace) << "[host_buffer] [" << instance_id_ << L"] allocated size:" << size_ << " (total: " << total_size << ") usage: " << (usage_ == write_only ? "write_only" : "read_only");
	}	

	~implementation()
	{
		try
		{
			GL(glDeleteBuffers(1, &pbo_));
			--(usage_ == write_only ? g_w_total_count : g_r_total_count);
			auto total_size = (usage_ == write_only ? g_w_total_size : g_r_total_size) -= size_;
			CASPAR_LOG(trace) << "[host_buffer] [" << instance_id_ << L"] deallocated size:" << size_ << "(remaining total: " << total_size << ") usage: " << (usage_ == write_only ? "write_only" : "read_only");
		}
		catch(...)
		{
			CASPAR_LOG_CURRENT_EXCEPTION();
		}
	}

	void map()
	{
		if(data_)
			return;

		GL(glBindBuffer(target_, pbo_));

		//if(usage_ == write_only)			
		//	GL(glBufferData(target_, size_, NULL, GL_STREAM_DRAW));	// Notify OpenGL that we don't care about previous data.
		
		data_ = GL2(glMapBuffer(target_, usage_ == host_buffer::write_only ? GL_WRITE_ONLY : GL_READ_ONLY));

		GL(glBindBuffer(target_, 0));
		if(!data_)
			BOOST_THROW_EXCEPTION(invalid_operation() << msg_info("Failed to map target_ OpenGL Pixel Buffer Object."));
	}

	void wait(ogl_device& ogl)
	{
		fence_.wait(ogl);
	}

	void unmap()
	{
		if(!data_)
			return;
		
		GL(glBindBuffer(target_, pbo_));
		GL(glUnmapBuffer(target_));	
		data_ = nullptr;		
		GL(glBindBuffer(target_, 0));
	}

	void bind()
	{
		GL(glBindBuffer(target_, pbo_));
	}

	void unbind()
	{
		GL(glBindBuffer(target_, 0));
	}

	void begin_read(size_t width, size_t height, GLuint format)
	{
		unmap();
		bind();
		GL(glReadPixels(0, 0, width, height, format, GL_UNSIGNED_BYTE, NULL));
		unbind();
		fence_.set();
	}

	bool ready() const
	{
		return fence_.ready();
	}
};

host_buffer::host_buffer(size_t size, usage_t usage) : impl_(new implementation(size, usage)){}
const void* host_buffer::data() const {return impl_->data_;}
void* host_buffer::data() {return impl_->data_;}
void host_buffer::map(){impl_->map();}
void host_buffer::unmap(){impl_->unmap();}
void host_buffer::bind(){impl_->bind();}
void host_buffer::unbind(){impl_->unbind();}
void host_buffer::begin_read(size_t width, size_t height, GLuint format){impl_->begin_read(width, height, format);}
size_t host_buffer::size() const { return impl_->size_; }
bool host_buffer::ready() const{return impl_->ready();}
void host_buffer::wait(ogl_device& ogl){impl_->wait(ogl);}

boost::property_tree::wptree host_buffer::info()
{
	boost::property_tree::wptree info;

	info.add(L"total_read_count", g_r_total_count);
	info.add(L"total_write_count", g_w_total_count);
	info.add(L"total_read_size", g_r_total_size);
	info.add(L"total_write_size", g_w_total_size);

	return info;
}

}}
