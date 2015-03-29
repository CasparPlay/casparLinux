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

#include "StdAfx.h"

#include "video_channel.h"

#include "video_format.h"

#include "consumer/output.h"
#include "mixer/mixer.h"
#include "mixer/gpu/ogl_device.h"
#include "mixer/audio/audio_util.h"
#include "producer/stage.h"

#include <common/diagnostics/graph.h>
#include <common/env.h>

#include <boost/property_tree/ptree.hpp>

#include <string>

namespace caspar { namespace core {

struct video_channel::implementation : boost::noncopyable
{
	video_channel&					self_;
	const int					index_;
	video_format_desc				format_desc_;
	const safe_ptr<ogl_device>			ogl_;
	const safe_ptr<diagnostics::graph>		graph_;

	const safe_ptr<caspar::core::output>		output_;
	const safe_ptr<caspar::core::mixer>		mixer_;
	const safe_ptr<caspar::core::stage>		stage_;

	safe_ptr<monitor::subject>			monitor_subject_;
	
public:
	implementation(video_channel& self, int index, const video_format_desc& format_desc, const safe_ptr<ogl_device>& ogl, const channel_layout& audio_channel_layout)  
		: self_(self)
		, index_(index)
		, format_desc_(format_desc)
		, ogl_(ogl)
		, output_(new caspar::core::output(graph_, format_desc, audio_channel_layout, index))
		, mixer_(new caspar::core::mixer(graph_, output_, format_desc, ogl, audio_channel_layout, index))
		, stage_(new caspar::core::stage(graph_, mixer_, format_desc, index))
		, monitor_subject_(make_safe<monitor::subject>("/channel/" + boost::lexical_cast<std::string>(index)))
	{
		graph_->set_text(print());
		diagnostics::register_graph(graph_);

		for(int n = 0; n < std::max(1, env::properties().get(L"configuration.pipeline-tokens", 2)); ++n)
			stage_->spawn_token();

		stage_->monitor_output().attach_parent(monitor_subject_);
		mixer_->monitor_output().attach_parent(monitor_subject_);
		output_->monitor_output().attach_parent(monitor_subject_);

		CASPAR_LOG(info) << print() << " Successfully Initialized.";
	}

	void set_video_format_desc(const video_format_desc& format_desc)
	{
		if(format_desc.format == core::video_format::invalid)
			BOOST_THROW_EXCEPTION(invalid_argument() << msg_info("Invalid video-format"));

		try
		{
			output_->set_video_format_desc(format_desc);
			mixer_->set_video_format_desc(format_desc);
			stage_->set_video_format_desc(format_desc);
			ogl_->gc();
		}
		catch(...)
		{
			output_->set_video_format_desc(format_desc_);
			mixer_->set_video_format_desc(format_desc_);
			stage_->set_video_format_desc(format_desc_);
			throw;
		}
		format_desc_ = format_desc;
	}
		
	std::wstring print() const
	{
		return L"video_channel[" + boost::lexical_cast<std::wstring>(index_) + L"|" +  format_desc_.name + L"]";
	}

	boost::property_tree::wptree info() const
	{
		boost::property_tree::wptree info;

		auto stage_info  = stage_->info();
		auto mixer_info  = mixer_->info();
		auto output_info = output_->info();

		info.add(L"video-mode", format_desc_.name);

		if (stage_info.timed_wait(boost::posix_time::seconds(2)))
			info.add_child(L"stage", stage_info.get());

		if (mixer_info.timed_wait(boost::posix_time::seconds(2)))
			info.add_child(L"mixer", mixer_info.get());

		if (output_info.timed_wait(boost::posix_time::seconds(2)))
			info.add_child(L"output", output_info.get());
   
		return info;			   
	}

	boost::property_tree::wptree delay_info() const
	{
		boost::property_tree::wptree info;

		auto stage_info  = stage_->delay_info();
		auto mixer_info  = mixer_->delay_info();
		auto output_info = output_->delay_info();

		if (stage_info.timed_wait(boost::posix_time::seconds(2)))
			info.add_child(L"layers", stage_info.get());

		if (mixer_info.timed_wait(boost::posix_time::seconds(2)))
			info.add_child(L"mix-time", mixer_info.get());

		if (output_info.timed_wait(boost::posix_time::seconds(2)))
			info.add_child(L"consumers", output_info.get());

		return info;
	}
};

video_channel::video_channel(int index, const video_format_desc& format_desc, const safe_ptr<ogl_device>& ogl, const channel_layout& audio_channel_layout)
	: impl_(new implementation(*this, index, format_desc, ogl, audio_channel_layout)){}
safe_ptr<stage> video_channel::stage() { return impl_->stage_;} 
safe_ptr<mixer> video_channel::mixer() { return impl_->mixer_;} 
safe_ptr<output> video_channel::output() { return impl_->output_;} 
video_format_desc video_channel::get_video_format_desc() const{return impl_->format_desc_;}
void video_channel::set_video_format_desc(const video_format_desc& format_desc){impl_->set_video_format_desc(format_desc);}
boost::property_tree::wptree video_channel::info() const{return impl_->info();}
int video_channel::index() const {return impl_->index_;}
monitor::subject& video_channel::monitor_output(){return *impl_->monitor_subject_;}
boost::property_tree::wptree video_channel::delay_info() const { return impl_->delay_info(); }
}}
