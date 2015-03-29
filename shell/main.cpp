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

// tbbmalloc_proxy: 
// Replace the standard memory allocation routines in Microsoft* C/C++ RTL 
// (malloc/free, global new/delete, etc.) with the TBB memory allocator.

#include <tbb/tbbmalloc_proxy.h>

#include "server.h"

#include <unistd.h>	// sleep()
#include <iostream>

#include <protocol/amcp/AMCPProtocolStrategy.h>

#include <modules/bluefish/bluefish.h>
#include <modules/decklink/decklink.h>
//#include <modules/flash/flash.h>
#include <modules/ffmpeg/ffmpeg.h>
#include <modules/image/image.h>
//#include <modules/newtek/util/air_send.h>
//#include <modules/html/html.h>

#include <common/env.h>
//#include <common/exception/win32_exception.h>
//#include <common/exception/exceptions.h>
#include <common/log/log.h>
#include <common/gl/gl_check.h>
#include <common/os/windows/current_version.h>
#include <common/os/windows/system_info.h>
#include <common/concurrency/executor.h>

#include <tbb/task_scheduler_init.h>
#include <tbb/task_scheduler_observer.h>

#include <boost/property_tree/detail/file_parser_error.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/foreach.hpp>
#include <boost/thread.hpp>
#include <boost/thread/future.hpp>
#include <boost/locale.hpp>
#include <boost/algorithm/string/case_conv.hpp>
#include <boost/logic/tribool.hpp>

//#include <functional>

// NOTE: This is needed in order to make CComObject work since this is not a real ATL project.
/*CComModule _AtlModule;
extern __declspec(selectany) CAtlModule* _pAtlModule = &_AtlModule;

void change_icon( const HICON hNewIcon )
{
   auto hMod = ::LoadLibrary(L"Kernel32.dll"); 
   typedef DWORD(__stdcall *SCI)(HICON);
   auto pfnSetConsoleIcon = reinterpret_cast<SCI>(::GetProcAddress(hMod, "SetConsoleIcon")); 
   pfnSetConsoleIcon(hNewIcon); 
   ::FreeLibrary(hMod);
} */

void setup_global_locale()
{
	boost::locale::generator gen;
	gen.categories(boost::locale::codepage_facet);

	std::locale::global(gen(""));
}

/*void setup_console_window()
{	 
	auto hOut = GetStdHandle(STD_OUTPUT_HANDLE);

	// Disable close button in console to avoid shutdown without cleanup.
	EnableMenuItem(GetSystemMenu(GetConsoleWindow(), FALSE), SC_CLOSE , MF_GRAYED);
	DrawMenuBar(GetConsoleWindow());
	//SetConsoleCtrlHandler(HandlerRoutine, true);

	// Configure console size and position.
	auto coord = GetLargestConsoleWindowSize(hOut);
	coord.X /= 2;

	SetConsoleScreenBufferSize(hOut, coord);

	SMALL_RECT DisplayArea = {0, 0, 0, 0};
	DisplayArea.Right = coord.X-1;
	DisplayArea.Bottom = (coord.Y-1)/2;
	SetConsoleWindowInfo(hOut, TRUE, &DisplayArea);
		 
	//change_icon(::LoadIcon(GetModuleHandle(0), MAKEINTRESOURCE(101)));

	// Set console title.
	std::wstringstream str;
	str << "CasparCG Server " << caspar::env::version();
#ifdef COMPILE_RELEASE
	str << " Release";
#elif  COMPILE_PROFILE
	str << " Profile";
#elif  COMPILE_DEVELOP
	str << " Develop";
#elif  COMPILE_DEBUG
	str << " Debug";
#endif
	SetConsoleTitle(str.str().c_str());
} */

void print_info()
{
	CASPAR_LOG(info) << L"############################################################################";
	CASPAR_LOG(info) << L"CasparCG Server is distributed by the Swedish Broadcasting Corporation (SVT)";
	CASPAR_LOG(info) << L"under the GNU General Public License GPLv3 or higher.";
	CASPAR_LOG(info) << L"Please see LICENSE.TXT for details.";
	CASPAR_LOG(info) << L"http://www.casparcg.com/";
	CASPAR_LOG(info) << L"############################################################################";
	CASPAR_LOG(info) << L"Starting CasparCG Video and Graphics Playout Server " << caspar::env::version();
	CASPAR_LOG(info) << L"on " << caspar::get_win_product_name() << L" " << caspar::get_win_sp_version();
	CASPAR_LOG(info) << caspar::get_cpu_info();
	CASPAR_LOG(info) << caspar::get_system_product_name();
	
	// following is commented out to avoid compiler err
//	CASPAR_LOG(info) << L"Decklink " << caspar::decklink::get_version();
//	BOOST_FOREACH(auto device, caspar::decklink::get_device_list())
//		CASPAR_LOG(info) << L" - " << device;	
		
//	CASPAR_LOG(info) << L"Bluefish " << caspar::bluefish::get_version();
//	BOOST_FOREACH(auto device, caspar::bluefish::get_device_list())
//		CASPAR_LOG(info) << L" - " << device;	
	
	CASPAR_LOG(info) << L"FreeImage "	<< caspar::image::get_version();
	CASPAR_LOG(info) << L"FFMPEG-avcodec "  << caspar::ffmpeg::get_avcodec_version();
	CASPAR_LOG(info) << L"FFMPEG-avformat " << caspar::ffmpeg::get_avformat_version();
	CASPAR_LOG(info) << L"FFMPEG-avfilter " << caspar::ffmpeg::get_avfilter_version();
	CASPAR_LOG(info) << L"FFMPEG-avutil "	<< caspar::ffmpeg::get_avutil_version();
	CASPAR_LOG(info) << L"FFMPEG-swscale "  << caspar::ffmpeg::get_swscale_version();
//	CASPAR_LOG(info) << L"Flash "		<< caspar::flash::get_version();
//	CASPAR_LOG(info) << L"Template-Host "	<< caspar::flash::get_cg_version();
//	CASPAR_LOG(info) << L"NewTek iVGA "		<< (caspar::newtek::airsend::is_available() ? L"available" : L"unavailable (" + caspar::newtek::airsend::dll_name() + L")");
}

/*LONG WINAPI UserUnhandledExceptionFilter(EXCEPTION_POINTERS* info)
{
	try
	{
		CASPAR_LOG(fatal) << L"#######################\n UNHANDLED EXCEPTION: \n" 
			<< L"Adress:" << info->ExceptionRecord->ExceptionAddress << L"\n"
			<< L"Code:" << info->ExceptionRecord->ExceptionCode << L"\n"
			<< L"Flag:" << info->ExceptionRecord->ExceptionFlags << L"\n"
			<< L"Info:" << info->ExceptionRecord->ExceptionInformation << L"\n"
			<< L"Continuing execution. \n#######################";
	}
	catch(...){}

    return EXCEPTION_EXECUTE_HANDLER;
} */

std::wstring make_upper_case(const std::wstring& str)
{
	return boost::to_upper_copy(str);
}

struct init_t
{
	std::wstring name;
	std::function<void()> uninit;

	init_t(std::wstring name, std::function<void()> init, std::function<void()> uninit)
		: name(name), uninit(uninit)
	{
		if (init)
			init();
		CASPAR_LOG(info) << L"Initialized " << name << L" module.";
	}

	~init_t()
	{
		if (uninit)
			uninit();
		CASPAR_LOG(info) << L"Uninitialized " << name << L" module.";
	}
};

int main(int argc, char* argv[])
{
	//if (!caspar::html::init())
	//	return 0;

	// OFF FOR NOW.
	//static_assert(sizeof(void*) == 4, "64-bit code generation is not supported.");

	//SetUnhandledExceptionFilter(UserUnhandledExceptionFilter);

	setup_global_locale();

	std::wcout << L"Type \"q\" to close application." << std::endl;
	
	// Increase process priotity.
	//SetPriorityClass(GetCurrentProcess(), above_normal_priority_class);
	struct sched_param param;

	pthread_setschedparam(pthread_self(), SCHED_FIFO, &param);

	// Install structured exception handler.
	caspar::win32_exception::ensure_handler_installed_for_thread("main-thread");
				
	// Increase time precision. This will increase accuracy of function like Sleep(1) from 10 ms to 1 ms.
	/*struct inc_prec
	{
		inc_prec(){timeBeginPeriod(1);}
		~inc_prec(){timeEndPeriod(1);}
	} inc_prec; */

	// Install unstructured exception handlers into all tbb threads.
	struct tbb_thread_installer : public tbb::task_scheduler_observer
	{
		tbb_thread_installer(){observe(true);}
		void on_scheduler_entry(bool is_worker)
		{
			CASPAR_LOG(info) << L"on_scheduler_entry";
			caspar::win32_exception::ensure_handler_installed_for_thread("tbb-worker-thread");
		}
	} tbb_thread_installer;

	boost::tribool restart = false;
	tbb::task_scheduler_init init;
	std::wstring config_file_name(L"casparcg.config");
	
	try 
	{
		// Configure environment properties from configuration.
		if (argc >= 2)
		{
			config_file_name = caspar::widen(argv[1]);
		}

		caspar::env::configure(config_file_name);

		caspar::log::set_log_level(caspar::env::properties().get(L"configuration.log-level", L"debug"));

	#ifdef _DEBUG
		if(caspar::env::properties().get(L"configuration.debugging.remote", false))
			MessageBox(nullptr, TEXT("Now is the time to connect for remote debugging..."), TEXT("Debug"), MB_OK | MB_TOPMOST);
	#endif
		// Start logging to file.
		caspar::log::add_file_sink(caspar::env::log_folder());

		std::wcout << L"Logging [info] or higher severity to "<< caspar::env::log_folder() << std::endl << std::endl;
		
		// Setup console window.
		// setup_console_window();

		// Print environment information.
		print_info();

		std::wstringstream str;
		boost::property_tree::xml_writer_settings<std::wstring> w(' ', 3);

		boost::property_tree::write_xml(str, caspar::env::properties(), w);

		CASPAR_LOG(info) << config_file_name << L":\n-----------------------------------------\n" << str.str().c_str() << L"-----------------------------------------";

		tbb::atomic<bool> wait_for_keypress;
		wait_for_keypress = false;

		{
//			init_t html_init(L"html", nullptr, caspar::html::uninit);
			boost::promise<boost::tribool> shutdown_server_now;
			std::function<void (bool)> shutdown_server_now_func =
					[&shutdown_server_now](bool restart)
					{
						shutdown_server_now.set_value(restart);
					};
			auto shutdown_server = shutdown_server_now.get_future();

			// Create server object which initializes channels, protocols and controllers.
			caspar::server caspar_server(shutdown_server_now_func);

			// Create a amcp parser for console commands.
			caspar::protocol::amcp::AMCPProtocolStrategy amcpParser (
					L"Console",
					caspar_server.get_channels(),
					caspar_server.get_thumbnail_generator(),
					caspar_server.get_media_info_repo(),
					caspar_server.get_ogl_device(),
					shutdown_server_now_func);

			CASPAR_LOG(info) << L"amcp Parser has been setup...";

			// Create a dummy client which prints amcp responses to console.
			auto console_client = std::make_shared<caspar::IO::ConsoleClientInfo>();
			std::wstring wcmd, upper_cmd;

			// Use separate thread for the blocking console input, will be terminated 
			// anyway when the main thread terminates.
			boost::thread stdin_thread([&]
			{
				caspar::win32_exception::ensure_handler_installed_for_thread("stdin-thread");

				while(true)
				{
					std::getline(std::wcin, wcmd);	// TODO: It's blocking...

					//upper_cmd = boost::to_upper(wcmd);  // TODO COMPILER crashes on this line, Strange!
					upper_cmd = make_upper_case(wcmd);

					if(upper_cmd == L"EXIT" || upper_cmd == L"Q" || upper_cmd == L"QUIT" || upper_cmd == L"BYE")
					{
						CASPAR_LOG(info) << L"exit invoked...";
						wait_for_keypress = true;
						shutdown_server_now.set_value(boost::indeterminate);
						break;
					}

					try
					{
						//CASPAR_LOG(info) << L" trying dummy...";
						// This is just dummy code for testing.
						if(wcmd.substr(0, 1) == L"1")
							wcmd = L"LOADBG 1-1 " + wcmd.substr(1, wcmd.length()-1) + L" SLIDE 100 LOOP \r\nPLAY 1-1";
						else if(wcmd.substr(0, 1) == L"2")
							wcmd = L"MIXER 1-0 VIDEO IS_KEY 1";
						else if(wcmd.substr(0, 1) == L"3")
							wcmd = L"CG 1-2 ADD 1 BBTELEFONARE 1";
						else if(wcmd.substr(0, 1) == L"4")
							wcmd = L"PLAY 1-1 DV FILTER yadif=1:-1 LOOP";
						else if(wcmd.substr(0, 1) == L"5")
						{
							auto file = wcmd.substr(2, wcmd.length()-1);
							wcmd = L"PLAY 1-1 " + file + L" LOOP\r\n" 
									L"PLAY 1-2 " + file + L" LOOP\r\n" 
									L"PLAY 1-3 " + file + L" LOOP\r\n"
									L"PLAY 2-1 " + file + L" LOOP\r\n" 
									L"PLAY 2-2 " + file + L" LOOP\r\n" 
									L"PLAY 2-3 " + file + L" LOOP\r\n";
						}
						else if(upper_cmd.substr(0, 1) == L"X")
						{
							int num = 0;
							std::wstring file;
							try
							{
								num = boost::lexical_cast<int>(wcmd.substr(1, 2));
								file = wcmd.substr(4, wcmd.length()-1);
							}
							catch(...)
							{
								num = boost::lexical_cast<int>(wcmd.substr(1, 1));
								file = wcmd.substr(3, wcmd.length()-1);
							}

							int n = 0;
							int num2 = num;
							while(num2 > 0)
							{
								num2 >>= 1;
								n++;
							}

							wcmd = L"MIXER 1 GRID " + boost::lexical_cast<std::wstring>(n);

							for(int i = 1; i <= num; ++i)
								wcmd += L"\r\nPLAY 1-" + boost::lexical_cast<std::wstring>(i) + L" " + file + L" LOOP";// + L" SLIDE 100 LOOP";
						}
					}
					catch (...)
					{
						//CASPAR_LOG_CURRENT_EXCEPTION();
						CASPAR_LOG(fatal) << L"CASPAR_LOG_CURRENT_EXCEPTION()";
						continue;
					}

					wcmd += L"\r\n";
					amcpParser.Parse(wcmd.c_str(), wcmd.length(), console_client);

					if (shutdown_server.is_ready())
					{
						break;
					}
				}
			});
			stdin_thread.detach();
			restart = shutdown_server.get();

			if (restart == boost::indeterminate)
				sleep(2); // Give the console thread a chance to finish amcp.Parse if a KILL or RESTART was issued via the console.
		}
		sleep(500);
		CASPAR_LOG(info) << "Successfully shutdown CasparCG Server.";

		if (wait_for_keypress)
			system("pause");
	}
	catch(boost::property_tree::file_parser_error&)
	{
		//CASPAR_LOG_CURRENT_EXCEPTION();
		CASPAR_LOG(fatal) << L"Unhandled configuration error in main thread. Please check the configuration file (" << config_file_name << L") for errors.";
		system("pause");
	}
	catch(...)
	{
		//CASPAR_LOG_CURRENT_EXCEPTION();
		CASPAR_LOG(fatal) << L"Unhandled exception in main thread. Please report this error on the CasparCG forums (www.casparcg.com/forum).";
		sleep(10);
		std::wcout << L"\n\nCasparCG will automatically shutdown. See the log file located at the configured log-file folder for more information.\n\n";
		sleep(40);
	}
	
	return restart ? 5 : 0;
}
