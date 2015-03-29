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

#include "image_loader.h"

#include <common/exception/exceptions.h>
#include <common/utility/string.h>
#include <common/log/log.h>	// for CASPAR_LOG thinggy..

#if defined(_MSC_VER)
#pragma warning (disable : 4714) // marked as __forceinline not inlined
#endif

#include <boost/exception/errinfo_file_name.hpp>
#include <boost/filesystem.hpp>

#include "image_algorithms.h"
#include "image_view.h"

namespace caspar { namespace image {

std::shared_ptr<FIBITMAP> load_image(const std::wstring& filename)
{
	const wchar_t * tmp = reinterpret_cast<const wchar_t*>(filename.c_str());
	char fname[100]= {0};// = reinterpret_cast<const char*>(tmp);

	std::wcstombs(fname, tmp, filename.size());

	CASPAR_LOG(info) << "Looking for file => " << fname;
	if(!boost::filesystem::exists(fname))
		BOOST_THROW_EXCEPTION(file_not_found() << boost::errinfo_file_name(narrow(fname)));

	FREE_IMAGE_FORMAT fif = FIF_UNKNOWN;
	fif = FreeImage_GetFileType(fname, 0);

	if (fif == FIF_UNKNOWN)	{ // time of guess
		fif = FreeImage_GetFIFFromFilename(fname);
		
		if(fif == FIF_UNKNOWN || !FreeImage_FIFSupportsReading(fif))
			BOOST_THROW_EXCEPTION(invalid_argument() << msg_info("Unsupported image format."));
	}

	auto bitmap = std::shared_ptr<FIBITMAP>(FreeImage_Load(fif, fname, 0), FreeImage_Unload);

	/*fif = FreeImage_GetFileTypeU(filename.c_str(), 0);
	if(fif == FIF_UNKNOWN)	
		fif = FreeImage_GetFIFFromFilenameU(filename.c_str()); this function only works on windows
		
	if(fif == FIF_UNKNOWN || !FreeImage_FIFSupportsReading(fif))
		BOOST_THROW_EXCEPTION(invalid_argument() << msg_info("Unsupported image format.")); 
		
	auto bitmap = std::shared_ptr<FIBITMAP>(FreeImage_LoadU(fif, filename.c_str(), 0), FreeImage_Unload);*/
		  
	if(FreeImage_GetBPP(bitmap.get()) != 32)
	{
		bitmap = std::shared_ptr<FIBITMAP>(FreeImage_ConvertTo32Bits(bitmap.get()), FreeImage_Unload);
		if(!bitmap)
			BOOST_THROW_EXCEPTION(invalid_argument() << msg_info("Unsupported image format."));
	}

	//PNG-images need to be premultiplied with their alpha
	if(fif == FIF_PNG)
	{
		image_view<bgra_pixel> original_view(FreeImage_GetBits(bitmap.get()), FreeImage_GetWidth(bitmap.get()), FreeImage_GetHeight(bitmap.get()));
		premultiply(original_view);
	}
	
	return bitmap;
}

std::shared_ptr<FIBITMAP> load_image(const std::string& filename)
{
	return load_image(widen(filename));
}

std::shared_ptr<FIBITMAP> load_png_from_memory(const void* memory_location, size_t size)
{
	FREE_IMAGE_FORMAT fif = FIF_PNG;

	auto memory = std::unique_ptr<FIMEMORY, decltype(&FreeImage_CloseMemory)>(
			FreeImage_OpenMemory(static_cast<BYTE*>(const_cast<void*>(memory_location)), size),
			FreeImage_CloseMemory);
	auto bitmap = std::shared_ptr<FIBITMAP>(FreeImage_LoadFromMemory(fif, memory.get(), 0), FreeImage_Unload);

	if (FreeImage_GetBPP(bitmap.get()) != 32)
	{
		bitmap = std::shared_ptr<FIBITMAP>(FreeImage_ConvertTo32Bits(bitmap.get()), FreeImage_Unload);

		if (!bitmap)
			BOOST_THROW_EXCEPTION(invalid_argument() << msg_info("Unsupported image format."));
	}

	return bitmap;
}

}}
