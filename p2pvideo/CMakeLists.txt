cmake_minimum_required(VERSION 2.6)
project(p2pvideo)

include_directories(./../)
include_directories(./include)
file(GLOB p2pvideo_src ./src/*.cc)

add_library(p2pvideo STATIC ${p2pvideo_src})

set(PLATFORM_SPECIFIC_DEFINITIONS 
	-DWEBRTC_POSIX
	-DWEBRTC_LINUX
)

add_definitions(
	${PLATFORM_SPECIFIC_DEFINITIONS}
	-std=gnu++11
	-ldl
	-lss3
	-lrt
	-DEXPAT_RELATIVE_PATH
	#-DSSL_USE_NSS
	-DSSL_USE_SCHANNEL
	-DWEBRTC_LOGGING=1
	)
set(EXT_LIB pthread ssl crypto)
add_executable(p2pvideo_example webrtc_streamer.cc)
target_link_libraries(p2pvideo_example p2pvideo base talk_app talk_media_devices talk_media_base json_app p2p ${EXT_LIB})


