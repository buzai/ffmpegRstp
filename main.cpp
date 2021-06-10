#include <iostream>
extern "C"
{
    #include "libavcodec/avcodec.h"

    #include "libavcodec/avfft.h"

    #include "libavdevice/avdevice.h"

    #include "libavfilter/avfilter.h"

    #include "libavfilter/buffersink.h"
    #include "libavfilter/buffersrc.h"

    #include "libavformat/avformat.h"
    #include "libavformat/avio.h"

    // libav resample

    #include "libavutil/opt.h"
    #include "libavutil/common.h"
    #include "libavutil/channel_layout.h"
    #include "libavutil/imgutils.h"
    #include "libavutil/mathematics.h"
    #include "libavutil/samplefmt.h"
    #include "libavutil/time.h"
    #include "libavutil/opt.h"
    #include "libavutil/pixdesc.h"
    #include "libavutil/file.h"
    #include "libswscale/swscale.h"

    // sdl2
    #include <SDL2/SDL.h>
    #include <SDL2/SDL_types.h>
}

#include <iostream>
#include <string>


#ifdef __DEBUG
#define debugLog(format, ...) \
          	printf("debug: [%s:%d->%s] " format, __FILE__, __LINE__, __func__, ##__VA_ARGS__)
#else
#define debugLog(format, ...)
#endif

#define infoLog(format, ...) \
  			printf("info: [%s:%d->%s] " format, __FILE__, __LINE__, __func__, ##__VA_ARGS__)

#define errorLog(format, ...) \
  			printf("error: [%s:%d->%s] " format, __FILE__, __LINE__, __func__, ##__VA_ARGS__)

class SdlPlay {
public:
    SdlPlay(int width, int height);
    void updateWithFrame(AVFrame * frame) {
        SDL_UpdateTexture(sdlTexture, NULL, frame -> data[0], frame -> linesize[0]);
        SDL_RenderClear(sdlRenderer);
        SDL_RenderCopy(sdlRenderer, sdlTexture, NULL, NULL);
        SDL_RenderPresent(sdlRenderer);
        SDL_Delay(40);
    }
    bool isQuit() {
        if (SDL_PollEvent(&event)) {
            if (SDL_QUIT == event.type) {
                printf("SDL2 quit event\n");
                return true;
            }
        }
        return false;
    }
    void delay(int ms) {
        SDL_Delay(ms);
    }
private:
    int _screen_w;
    int _screen_h;
    SDL_Window * screen;
    SDL_Renderer * sdlRenderer;
    SDL_Texture * sdlTexture;
    SDL_Rect sdlRect;
    SDL_Thread * video_pthread;
    SDL_Event event;
};

SdlPlay::SdlPlay(int width, int height) : _screen_h(height), _screen_w(width) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)) {
        errorLog("Could not initialize SDL2 - %s\n", SDL_GetError());
        throw "sdl2 init error";
    }
    screen = SDL_CreateWindow(
            "ffmpeg player",
            SDL_WINDOWPOS_UNDEFINED,
            SDL_WINDOWPOS_UNDEFINED,
            _screen_w,
            _screen_h,
            SDL_WINDOW_OPENGL);

    if(!screen) {
        errorLog("sdl2 window init error");
        throw "sdl2 window init error";
    }

    sdlRenderer = SDL_CreateRenderer(screen, -1, 0);
    sdlTexture = SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING, _screen_w, _screen_h);

    sdlRect.x = 0;
    sdlRect.y = 0;
    sdlRect.w = _screen_w;
    sdlRect.h = _screen_h;
}


class RtspPlay {
public:
    /**
     * init rtsp input
     * @param rtspUrl rtsp url
     * @param protocol tcp or udp
     */
    RtspPlay(std::string rtspUrl, std::string protocol);
    void findStreamIndex();
    int width;
    int height;
private:
    const std::string _rtspUrl;
    AVDictionary *options = NULL;
    AVFormatContext * _avFormatContext;
    AVCodecContext * _avCodecContext;
    AVCodec * _avCodec;
    AVFrame * inFrame, * outFrame;
    unsigned char * out_buffer;

    int videoIndex = -1;
    int audioIndex = -1;
};

RtspPlay::RtspPlay(std::string rtspUrl, std::string protocol) : _rtspUrl(rtspUrl)
{
    avdevice_register_all();
    avformat_network_init();

    // set protocol
    av_dict_set(&options, "rtsp_transport", protocol.c_str(), 0);

    // 设置TCP连接最大延时时间
    av_dict_set(&options, "max_delay", "900000000", 0);

    // 设置“buffer_size”缓存容量
    av_dict_set(&options, "buffer_size", "102400000", 0);

    // 设置avformat_open_input超时时间为3秒
    av_dict_set(&options, "stimeout", "900000000", 0);

    _avFormatContext = avformat_alloc_context();

    int ret = avformat_open_input(&_avFormatContext, _rtspUrl.c_str(), NULL, &options);
    if (ret != 0)
    {
        errorLog("open input stream : ret=%d\n", ret);
        throw "open input stream error";
    }
    // find stream info
    ret = avformat_find_stream_info(_avFormatContext, NULL);
    if (ret < 0) {
        errorLog("find stream info : ret=%d\n", ret);
        throw "find stream info";
    }

    // find stream
    findStreamIndex();
    if (videoIndex == -1) {
        errorLog("cant find video stream");
        throw "cant find video stream";
    }

    if (audioIndex == -1) {
        infoLog("cant find audio stream");
//        throw "cant find audio stream";
    }

    // init video codec
    AVCodecParameters* para = avcodec_parameters_alloc();
    AVCodec* _avCodec = avcodec_find_decoder(_avFormatContext->streams[videoIndex]->codecpar->codec_id);
    AVCodecContext* _avCodecContext = avcodec_alloc_context3(_avCodec);
    avcodec_parameters_to_context(_avCodecContext, _avFormatContext->streams[videoIndex]->codecpar);
    _avCodecContext->pkt_timebase = _avFormatContext->streams[videoIndex]->time_base;
    _avCodecContext->codec_id = _avCodec->id;
    _avFormatContext->streams[videoIndex]->discard = AVDISCARD_DEFAULT;


//    _avCodecContext = _avFormatContext->streams[videoIndex]->codec;
//    _avCodec = avcodec_find_decoder(_avCodecContext->codec_id);

    if (!_avCodec) {
        errorLog("cant find video decoder");
        throw "cant find video decoder";
    }

    if (NULL == _avCodec) {
        errorLog("cant find video decoder");
        throw "cant find video decoder";
    }

    if(_avCodec->capabilities & AV_CODEC_CAP_TRUNCATED)
        _avCodecContext->flags |= AV_CODEC_FLAG_TRUNCATED; /* We may send incomplete frames */
    if(_avCodec->capabilities & AV_CODEC_FLAG2_CHUNKS)
        _avCodecContext->flags2 |= AV_CODEC_FLAG2_CHUNKS;

    int first = 1;

    if (avcodec_open2(_avCodecContext, _avCodec, NULL) < 0) {
        errorLog("cant open video decoder");
        throw "cant open video decoder";
    }
    width = _avCodecContext->width;
    height = _avCodecContext->height;

    inFrame = av_frame_alloc();
    outFrame = av_frame_alloc();

    AVPixelFormat fromat = AV_PIX_FMT_YUV420P;
    out_buffer = (unsigned char *) av_malloc(av_image_get_buffer_size(fromat, width, height, 1));
    av_image_fill_arrays(
            outFrame -> data,
            outFrame -> linesize,
            out_buffer,
            fromat,
            width,
            height,
            1);


    struct SwsContext * img_convert_ctx;
    img_convert_ctx = sws_getContext(width, height, _avCodecContext -> pix_fmt,
                                    width, height, fromat, SWS_BICUBIC, NULL, NULL, NULL);

    AVPacket * packet;
    packet = (AVPacket *) av_malloc(sizeof(AVPacket));
    int got_picture;

    SdlPlay * playView = new SdlPlay(width, height);

    for (;;) {
        if (av_read_frame(_avFormatContext, packet) < 0) {
            errorLog("av_read_frame error\n");
            continue;
        }

        int ret = avcodec_decode_video2(_avCodecContext, inFrame, &got_picture, packet);
        if (ret < 0) {
            continue;
        }

        int is_key_frame = (inFrame->key_frame == 1) || (inFrame->pict_type == AV_PICTURE_TYPE_I);
        if (!is_key_frame && first) {
            continue;
        }
        if (is_key_frame && first == 1) {
            first = 0;
        }

        if (!got_picture) {
            errorLog("got_picture Error. got_picture: %d\n", got_picture);
            continue;
        }

        if (got_picture) {
            infoLog("render picture\n");
            sws_scale(img_convert_ctx, (const unsigned char *
            const * ) inFrame -> data, inFrame -> linesize, 0, height, outFrame -> data, outFrame -> linesize);
            playView->updateWithFrame(outFrame);
            bool quit = playView->isQuit();
            if (quit) {
                break;
            }
        }
//        if(strcmp(_avFormatContext->iformat->name, "rtsp") == 0) {
//            playView->delay(10);
//        }
        av_free_packet(packet);
    }
}

void RtspPlay::findStreamIndex() {
    for (int i = 0; i < _avFormatContext->nb_streams; i++)
    {
        if (_avFormatContext->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            videoIndex = i;
            break;
        }
    }
    for (int i = 0; i < _avFormatContext->nb_streams; i++)
    {
        if (_avFormatContext->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO)
        {
            audioIndex = i;
            break;
        }
    }
}


int main() {
    std::cout << "Hello, World!" << std::endl;
    RtspPlay rtsp("rtsp://wowzaec2demo.streamlock.net/vod/mp4:BigBuckBunny_115k.mov", "tcp");
    return 0;
}
