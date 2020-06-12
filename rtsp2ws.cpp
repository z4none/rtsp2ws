//
#include "pch.h"
#include "fmp4.h"


static volatile bool quit = false;

SDL_Window * window = nullptr;
SDL_Renderer * renderer = nullptr;
SDL_Texture * texture = nullptr;

//
void sig_handler(int dummy)
{
	quit = true;
}

//
void print_bin(uint8_t * p, int s)
{
	for (int i = 0; i < s; i++)
	{
		printf("%02X ", p[i]);
	}
	printf("\n");
}

	
//
// -1 invalid start code
// -2 invalid size
int parse_nal(uint8_t *& pkt, int & pkt_size, uint8_t *& nal, int & nal_size)
{
	if (pkt_size > 3)
	{
		if (pkt[0] == 0x00 && pkt[1] == 0x00)
		{
			if (pkt[2] == 0x01)
			{
				pkt = nal = pkt + 3;
				pkt_size -= 3;
			}
			else if (pkt_size > 4 && pkt[2] == 0x00 && pkt[3] == 0x01)
			{
				pkt = nal = pkt + 4;
				pkt_size -= 4;
			}
			else
			{
				return -1;
			}
		}
		else
		{
			return -1;
		}
	}
	else
	{
		return -2;
	}

	nal_size = 0;
	while (pkt_size)
	{
		if (pkt[0] == 0x00 && pkt[1] == 0x00)
		{
			if (pkt[2] == 0x01)
			{
				return 0;
			}
			else if (pkt_size > 4 && pkt[2] == 0x00 && pkt[3] == 0x01)
			{
				return 0;
			}
		}

		pkt += 1;
		pkt_size -= 1;
		nal_size += 1;
	}

	return 0;
}



int play(const char * url, WServer & wserver)
{
	printf("打开视频流 ...\n");
	printf("%s\n", url);

	FMp4Muxer mux;

	AVFormatContext* In_FormatContext = NULL;

	AVDictionary * options = NULL;
	av_dict_set(&options, "rtsp_transport", "tcp", 0);
	av_dict_set(&options, "stimeout", "5000000", 0);
	av_dict_set(&options, "fflags", "nobuffer", 0);
	av_dict_set(&options, "flags", "low_delay", 0);
	av_dict_set(&options, "framedrop", "strict", 0);

	if (avformat_open_input(&In_FormatContext, url, NULL, &options) < 0)
	{
		printf("打开视频流失败\n");

		if (In_FormatContext)
		{
			avformat_close_input(&In_FormatContext);
		}

		av_dict_free(&options);
		return -1;
	}
	av_dict_free(&options);

	DWORD begin = GetTickCount();
	printf("检测视频流信息\n");
	if (avformat_find_stream_info(In_FormatContext, NULL) < 0)
	{
		printf("检测视频流信息失败\n");

		avformat_close_input(&In_FormatContext);
		return -1;
	}

	printf("检测视频流信息成功，耗时 %d ms\n", (GetTickCount() - begin));

	av_dump_format(In_FormatContext, 0, In_FormatContext->url, 0);

	AVStream* in_video_stream = NULL;
	AVStream* in_audio_stream = NULL;
	AVCodecParameters* in_VideoCodec = NULL;
	AVCodecParameters* in_AudioCodec = NULL;
	int Audio_InStream_Index = -1;
	int Video_InStream_Index = -1;

	// enumerate streams
	for (unsigned int i = 0; i < In_FormatContext->nb_streams; i++)
	{
		if ((In_FormatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) ||
			(In_FormatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO))
		{
			AVStream* in_stream = In_FormatContext->streams[i];

			if (In_FormatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
			{
				in_audio_stream = in_stream;
				Audio_InStream_Index = in_audio_stream->index;
				in_AudioCodec = in_stream->codecpar;
			}
			if (In_FormatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
			{
				in_video_stream = in_stream;
				Video_InStream_Index = in_video_stream->index;
				in_VideoCodec = in_stream->codecpar;
			}
		}
	}

	AVCodecContext *video_codec_ctx = NULL;
	if (in_video_stream != NULL)
	{
		AVCodec* codec = avcodec_find_decoder(in_video_stream->codecpar->codec_id);
		if (codec == NULL)
		{
			avformat_close_input(&In_FormatContext);
			return -1;
		}
		video_codec_ctx = avcodec_alloc_context3(codec);
		avcodec_parameters_to_context(video_codec_ctx, in_video_stream->codecpar);
		if (avcodec_open2(video_codec_ctx, codec, NULL) < 0)
		{
			avformat_close_input(&In_FormatContext);
			return -1;
		}
	}

	AVPacket pkt;
	AVFrame * decodeframe = av_frame_alloc();
	AVFrame * RGBFrame = av_frame_alloc();

	int frameWidth = video_codec_ctx->width;
	int frameHeight = video_codec_ctx->height;

	printf("视频画面大小：%d x %d\n", frameWidth, frameHeight);

	texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_YV12, SDL_TEXTUREACCESS_STREAMING, frameWidth, frameHeight);

	av_init_packet(&pkt);

	std::vector<char> sei_buffer;
	std::vector<uint8_t> sps;
	std::vector<uint8_t> pps;

	int  played = 0;
	while (!quit)
	{
		// process SDL events
		SDL_Event event;
		while (SDL_PollEvent(&event) != 0)
		{
			if (event.type == SDL_QUIT)
			{
				quit = true;
			}
		}

		// read a packet
		int ret = av_read_frame(In_FormatContext, &pkt);

		if (ret < 0)
		{
			printf("读取视频流失败, %d\n", ret);
			break;
		}

		// packet stream type check
		if ((pkt.stream_index != in_video_stream->index) && (pkt.stream_index != in_audio_stream->index))
		{
			av_packet_unref(&pkt);
			continue;
		}

		// process video packet
		if (pkt.stream_index == in_video_stream->index)
		{
			uint8_t * p = pkt.data;
			int pkt_size = pkt.size;
			uint8_t * nal = 0;
			int nal_size = 0;

			while (pkt_size) 
			{
				if (parse_nal(p, pkt_size, nal, nal_size) == 0)
				{					
					switch (nal[0] & 0x1F)
					{
					case 1:
					{
						// printf("p frame\n");

						uint32_t size = nal_size;
						uint8_t * data = nal;
						size = mux.generate_moof_mdat(data, size);
						wserver.send_data(data, size);
						free(data);
						break;
					}
					case 5:
					{
						// printf("i frame\n");

						if (sps.size() && pps.size())
						{
							FMp4Info fi;
							fi.sps = sps.data();
							fi.sps_size = sps.size();
							fi.pps = pps.data();
							fi.pps_size = pps.size();
							
							uint32_t size;
							uint8_t * data;
							size = mux.generate_ftyp_moov(data, fi);
							wserver.send_head(data, size);
							free(data);
						}

						uint32_t size = nal_size;
						uint8_t * data = nal;
						size = mux.generate_moof_mdat(data, size);
						wserver.send_data(data, size);
						free(data);

						break;
					}
					case 6:
						// printf("sei: ");
						// print_bin(nal, nal_size);
						break;
					case 7:
						// printf("sps: ");
						// print_bin(nal, nal_size);
						sps = std::vector<uint8_t>(nal, nal + nal_size);
						break;
					case 8:
						// printf("pps: ");
						// print_bin(nal, nal_size);
						pps = std::vector<uint8_t>(nal, nal + nal_size);
						break;
					default:
						printf("unknow nal %02X, size %d\n", nal[0] & 0x1F, nal_size);
					}
				}
			}
			
			// send to codec
			ret = avcodec_send_packet(video_codec_ctx, &pkt);
			if (ret == 0)
			{
				// trying to get a frame
				ret = avcodec_receive_frame(video_codec_ctx, decodeframe);
				if (ret == 0)
				{
					SDL_UpdateYUVTexture(texture, NULL,
						decodeframe->data[0], decodeframe->linesize[0],
						decodeframe->data[1], decodeframe->linesize[1],
						decodeframe->data[2], decodeframe->linesize[2]);

					SDL_RenderClear(renderer);
					SDL_RenderCopy(renderer, texture, NULL, NULL);
					SDL_RenderPresent(renderer);
					SDL_Delay(0);
				}
				else
				{
					printf("解码失败 %d\n", ret);
					continue;
				}
			}
		}

		av_packet_unref(&pkt);

	} // while 1

	if (decodeframe)
	{
		av_frame_free(&decodeframe);
		decodeframe = NULL;
	}

	if (video_codec_ctx) {
		avcodec_free_context(&video_codec_ctx);
	}

	if (In_FormatContext) {
		avformat_close_input(&In_FormatContext);
	}
	
	if (texture) {
		SDL_DestroyTexture(texture);
	}

	return 0;
}

int read_url(std::string & url)
{
	char path[MAX_PATH] = { 0 };
	GetModuleFileNameA(NULL, path, MAX_PATH);
	strrchr(path, '\\')[1] = 0;
	strcat_s(path, "url.txt");

	std::ifstream file(path);
	if (file.is_open())
	{
		std::string line;
		while (std::getline(file, line))
		{
			if (line.length() && line[0] != '#')
			{
				url = line;
				return 0;
			}
		}
	}
	return -1;
}



//
int main()
{
	signal(SIGINT, sig_handler);

	if (SDL_Init(SDL_INIT_EVERYTHING) == -1)
	{
		printf("SDL 初始化失败\n");
		return -1;
	}

	window = SDL_CreateWindow("Hello", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640, 480, SDL_WINDOW_SHOWN);
	
	if (window == NULL) 
	{
		printf("SDL创建窗口失败, %s\n", SDL_GetError());
		return -1;
	}

	renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
	if (renderer == nullptr) 
	{
		printf("SDL 创建渲染器失败, %s\n", SDL_GetError());
		return -1;
	}

	
	std::string url;
	if (read_url(url) < 0)
	{
		printf("未指定 url\n");
		return -1;
	}

	WServer wserver;
	wserver.start();

	play(url.c_str(), wserver);

	// std::cin.get();

	wserver.stop();
	SDL_Quit();

	return 0;
}

