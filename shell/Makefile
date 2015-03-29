include Makefile.inc

CC 	= g++
CFLAGS  = -g3 -std=gnu++11 -Wall -O2 -fpermissive -DBOOST_LOG_DYN_LINK

DIRS	= ../common ../core ../modules/ffmpeg ../modules/ogl \
	  ../modules/oal ../modules/image ../protocol

EXE	= casparcg

OBJS	= ../protocol/amcp/AMCPProtocolStrategy.o ../protocol/amcp/AMCPCommandsImpl.o \
	../protocol/util/stateful_protocol_strategy_wrapper.o ../protocol/util/AsyncEventServer.o \
	../protocol/util/SocketInfo.o ../core/producer/layer/layer_producer.o ../protocol/util/Thread.o \
	../protocol/amcp/AMCPCommandQueue.o ../protocol/osc/client.o \
	../protocol/osc/oscpack/OscPrintReceivedElements.o ../protocol/osc/oscpack/OscOutboundPacketStream.o \
	../protocol/osc/oscpack/OscReceivedElements.o ../protocol/osc/oscpack/OscTypes.o \
	../protocol/clk/clk_command_processor.o  ../protocol/clk/clk_commands.o  \
	../protocol/clk/CLKProtocolStrategy.o \
	../core/video_format.o ../core/consumer/output.o ../core/consumer/frame_consumer.o \
	../core/monitor/monitor.o ../core/video_channel.o  ../core/mixer/mixer.o \
	../core/mixer/write_frame.o ../core/mixer/gpu/host_buffer.o ../core/mixer/gpu/device_buffer.o \
	../core/mixer/gpu/shader.o ../core/mixer/gpu/fence.o ../core/mixer/gpu/ogl_device.o \
	../core/mixer/image/image_kernel.o ../core/mixer/image/image_mixer.o \
	../core/mixer/image/shader/image_shader.o ../core/mixer/image/blend_modes.o \
	../core/mixer/audio/audio_util.o ../core/mixer/audio/audio_mixer.o ../core/mixer/read_frame.o \
	../core/thumbnail_generator.o ../core/parameters/parameters.o ../core/producer/stage.o \
	../core/producer/frame_producer.o ../core/producer/layer.o \
	../core/producer/separated/separated_producer.o \
	../core/producer/media_info/in_memory_media_info_repository.o \
	../core/producer/channel/channel_producer.o ../core/producer/frame/basic_frame.o \
	../core/producer/frame/frame_transform.o ../core/producer/transition/transition_producer.o \
	../core/producer/color/color_producer.o ../modules/image/image.o \
	../modules/ffmpeg/consumer/streaming_consumer.o ../modules/ffmpeg/consumer/ffmpeg_consumer.o \
	../modules/ffmpeg/ffmpeg.o ../modules/ffmpeg/util/error.o ../modules/ffmpeg/producer/input/input.o \
	../modules/ffmpeg/producer/tbb_avcodec.o ../modules/ffmpeg/producer/muxer/frame_muxer.o  \
	../modules/ffmpeg/producer/ffmpeg_producer.o ../modules/ffmpeg/producer/audio/audio_decoder.o \
	../modules/ffmpeg/producer/audio/audio_resampler.o ../modules/ffmpeg/producer/video/video_decoder.o \
	../modules/ffmpeg/producer/util/flv.o ../modules/ffmpeg/producer/util/util.o \
	../modules/ffmpeg/producer/filter/filter.o ../modules/image/consumer/image_consumer.o \
	../modules/image/util/image_loader.o ../modules/image/util/image_algorithms.o \
	../modules/image/producer/image_producer.o ../modules/image/producer/image_scroll_producer.o \
	../modules/ogl/consumer/ogl_consumer.o ../modules/ogl/ogl.o \
	../common/gl/gl_check.o ../common/env.o ../modules/oal/oal.o ../modules/oal/consumer/oal_consumer.o \
	../common/filesystem/polling_filesystem_monitor.o ../common/exception/win32_exception.o \
	../common/diagnostics/graph.o ../common/concurrency/thread_info.o ../common/utility/base64.o \
	../common/utility/tweener.o ../common/utility/string.o ../common/log/log.o server.o main.o 

LDFLAGS = -L../dependencies/mesa/lib \
		-L../dependencies/boost/stage/lib \
		-L../modules/ffmpeg/lib/ \
		-L../dependencies/SFML/lib/ \
		-L../common -L../core \
		-L../modules/image \
		-L../modules/ogl \
		-L../protocol/osc \
		-L../protocol/amcp \
		-L../dependencies/tbb/lib/intel64/gcc4.4/ \
		-L../dependencies/ffmpeg/lib/ \
		-L../dependencies/FreeImage/Dist/ \
		-L../dependencies/icu/source/lib/ \
		-L../dependencies/glew/lib/

LIBFLAGS = -lpthread -ltbb -ltbbmalloc -ltbbmalloc_proxy -lGL -lGLEW -lGLEWmx -lboost_system -lboost_thread -lboost_log_setup -lboost_log -lboost_filesystem -lboost_locale -lboost_regex -lboost_date_time -lboost_chrono -lboost_timer -lavcodec -lavdevice -lavutil -lavfilter -lavformat -lswscale -lswresample -lfreeimage -lsfml-graphics -lsfml-window -lsfml-system -lsfml-audio

all:	$(OBJS)
	$(CC) -o $(EXE) -Wl,--start-group $(OBJS) -Wl,--end-group $(LDFLAGS) $(INCLUDES) $(LIBFLAGS)

$(OBJS):
	list='$(DIRS)'; \
	for subdir in $$list; do \
		$(MAKE) -C $$subdir || exit 1; \
	done
	$(MAKE) -f Makefile.inc

clean:
	list='$(DIRS)'; \
	for subdir in $$list; do \
		$(MAKE) -C $$subdir clean || exit 1; \
	done
	$(MAKE) -f Makefile.inc $@

