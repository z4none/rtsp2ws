#pragma once

#include <stdio.h>
#include <tchar.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>  
#include <inttypes.h>  
#include <stdint.h>  
#include <signal.h>

#include <cstdlib>
#include <functional>
#include <iostream>
#include <string>
#include <thread>
#include <fstream>
#include <vector>

#ifdef __cplusplus  
extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavutil/common.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libavutil/time.h>
#include <libavutil/pixfmt.h>
#include <libavutil/pixdesc.h>
#include <libswscale/swscale.h>
#include <libavcodec/avcodec.h>

#include <SDL2/SDL.h>
}

#include <websocketpp/server.hpp>
#include <websocketpp/config/asio_no_tls.hpp>

#include "ws.h"

#endif  

