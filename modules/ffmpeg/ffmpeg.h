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

#include <string>
#include <memory>

namespace caspar { 
	namespace core {
		struct media_info_repository;
	}

	namespace ffmpeg {
		void init(const safe_ptr<core::media_info_repository>& media_info_repo);
		void uninit();
		void disable_logging_for_thread();
		std::shared_ptr<void> temporary_disable_logging_for_thread(bool disable);
		bool is_logging_disabled_for_thread();

		std::wstring get_avcodec_version();
		std::wstring get_avformat_version();
		std::wstring get_avutil_version();
		std::wstring get_avfilter_version();
		std::wstring get_swscale_version();
	}
}
