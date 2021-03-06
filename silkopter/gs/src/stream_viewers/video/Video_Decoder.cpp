#include "Video_Decoder.h"
#include "Comms.h"

#if !defined RASPBERRY_PI
extern "C"
{
#include <libavutil/opt.h>
#include <libavcodec/avcodec.h>
#include <libavutil/common.h>
#include <libavutil/imgutils.h>
#include <libavutil/mathematics.h>
#include <libavutil/samplefmt.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}
#endif

bool Video_Decoder::s_codecs_registered = false;

Video_Decoder::Video_Decoder()
{
#if !defined RASPBERRY_PI
    if (!s_codecs_registered)
    {
        av_register_all();
        avcodec_register_all();
        avformat_network_init();
    }

    m_ffmpeg.codec = avcodec_find_decoder(AV_CODEC_ID_H264);
    if (!m_ffmpeg.codec)
    {
        QLOGE("Codec not found");
        exit(1);
    }

    m_ffmpeg.context = avcodec_alloc_context3(m_ffmpeg.codec);
    if (!m_ffmpeg.context)
    {
        QLOGE("Could not allocate video codec context");
        exit(1);
    }

    avcodec_get_context_defaults3(m_ffmpeg.context, m_ffmpeg.codec);

    m_ffmpeg.context->flags |= CODEC_FLAG_LOW_DELAY;
    m_ffmpeg.context->flags2 |= CODEC_FLAG2_CHUNKS;

    //m_ffmpeg.context->thread_count = 4;
    //m_ffmpeg.context->thread_type = FF_THREAD_SLICE;
    //m_ffmpeg.context->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;

    // 	m_ffmpeg.context->profile = FF_PROFILE_H264_BASELINE;

    if (avcodec_open2(m_ffmpeg.context, m_ffmpeg.codec, nullptr) < 0)
    {
        QLOGE("Could not open codec");
        exit(1);
    }

    m_ffmpeg.frame_yuv = av_frame_alloc();
    if (!m_ffmpeg.frame_yuv)
    {
        QLOGE("Could not allocate video frame");
        exit(1);
    }

    m_ffmpeg.frame_rgb = av_frame_alloc();
    if (!m_ffmpeg.frame_rgb)
    {
        QLOGE("Could not allocate video frame");
        exit(1);
    }

    m_ffmpeg.rgb.reset(new AVPicture);
    m_ffmpeg.rgb->linesize[0] = 0;
    m_ffmpeg.rgb->data[0] = nullptr;
#endif
}

Video_Decoder::~Video_Decoder()
{
#if !defined RASPBERRY_PI
    m_ffmpeg.rgb.reset();
    sws_freeContext(m_ffmpeg.sws_context);
    m_ffmpeg.sws_context = nullptr;

    av_frame_free(&m_ffmpeg.frame_rgb);
    av_frame_free(&m_ffmpeg.frame_yuv);
    avcodec_close(m_ffmpeg.context);
    avcodec_free_context(&m_ffmpeg.context);
#endif
}


bool Video_Decoder::decode_frame(silk::stream::IVideo::Value const& frame, math::vec2u32 const& size, std::vector<uint8_t>& data, Format format)
{
#if !defined RASPBERRY_PI
    AVPacket packet;
    av_init_packet(&packet);

    packet.pts = AV_NOPTS_VALUE;
    packet.dts = AV_NOPTS_VALUE;
    packet.data = const_cast<uint8_t*>(frame.data.data());
    packet.size = frame.data.size();

    int got_frame = 0;
    int len = avcodec_decode_video2(m_ffmpeg.context, m_ffmpeg.frame_yuv, &got_frame, &packet);
    if (len < 0)
    {
        QLOGW("Error while decoding frame");
        return false;
    }
    if (got_frame)
    {
        int frame_w = m_ffmpeg.frame_yuv->width;
        int frame_h = m_ffmpeg.frame_yuv->height;

        m_ffmpeg.sws_context = sws_getCachedContext(m_ffmpeg.sws_context,
                                                    frame_w, frame_h,
                                                    m_ffmpeg.context->pix_fmt,
                                                    size.x, size.y,
                                                    format == Format::BGRA ? AV_PIX_FMT_RGB32 : AV_PIX_FMT_BGR32, //inverted for some reason
                                                    SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);

        if (m_ffmpeg.sws_context)
        {
            int line_size = static_cast<int>(size.x * 4);
            if (line_size != m_ffmpeg.rgb->linesize[0])
            {
                m_ffmpeg.rgb->linesize[0] = line_size;
                data.resize(line_size * size.y);
//                delete[] m_ffmpeg.rgb->data[0];
//                m_ffmpeg.rgb->data[0] = new uint8_t[(m_ffmpeg.rgb->linesize[0] + 1) * (img_h + 1)];
                m_ffmpeg.rgb->data[0] = data.data();
            }

            sws_scale(m_ffmpeg.sws_context,
                      m_ffmpeg.frame_yuv->data, m_ffmpeg.frame_yuv->linesize,
                      0, frame_h,
                      m_ffmpeg.rgb->data, m_ffmpeg.rgb->linesize);

            return true;
        }
    }
    return false;
#else
    return false;
#endif
}

