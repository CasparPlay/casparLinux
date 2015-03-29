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

#include "newtek.h"

#include "consumer/newtek_ivga_consumer.h"
#include "util/air_send.h"

#include <core/parameters/parameters.h>
#include <core/consumer/frame_consumer.h>

namespace caspar { namespace newtek {

void init()
{
	try
	{
		if (airsend::is_available())
			core::register_consumer_factory([](const core::parameters& params)
			{
				return newtek::create_ivga_consumer(params);
			});
	}
	catch(...){}
}

}}