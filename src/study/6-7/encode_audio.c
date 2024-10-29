#include <libavutil/log.h>
#include <libavutil/opt.h>
#include <libavcodec/avcodec.h>
#include <libavutil/samplefmt.h>


static int select_best_sample_rate(const AVCodec *codec) {
    const int *p;
    int best_sample_rate = 0;
    if (!codec->supported_samplerates) {
        return 44100;
    }
    p = codec->supported_samplerates;
    // 找到一个离44100最近的采样率
    while (*p) {
        if (!best_sample_rate || abs(44100 - *p) < abs(44100 - best_sample_rate)) {
            best_sample_rate = *p;
        }
        p++;
    }
    return best_sample_rate;
}

// ffmpeg源码自带
static int check_sample_fmt(const AVCodec *codec, enum AVSampleFormat sample_fmt)
{
    const enum AVSampleFormat *p = codec->sample_fmts;

    while (*p != AV_SAMPLE_FMT_NONE) {
        if (*p == sample_fmt)
            return 1;
        p++;
    }
    return 0;
}

static int encode(AVCodecContext *ctx, AVFrame *frame, AVPacket *pkt, FILE *out) {
    int ret = -1;
    ret = avcodec_send_frame(ctx, frame);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Failed to send frame to encoder\n");
        goto _END;
    }
    while (ret >= 0) {
        ret = avcodec_receive_packet(ctx, pkt);
        // AVERROR(EAGAIN)表示出错了
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            return 0;
        } else if(ret < 0) {
            return -1;
        }
        fwrite(pkt->data, 1, pkt->size, out);
        av_packet_unref(pkt);
    }
_END:
    return 0;
}

int main(int argc, char *argv[]) {

    char *dst = NULL;
    char *codecName = NULL;

    const AVCodec *codec = NULL;
    AVCodecContext *ctx = NULL;
    int ret = -1;
    FILE *f = NULL;
    AVFrame *frame = NULL;
    AVPacket *pkt = NULL;
    uint16_t *samples = NULL;

    av_log_set_level(AV_LOG_DEBUG);
    
    // 1. 输入参数
    if (argc < 2) {
        av_log(NULL, AV_LOG_ERROR, "arguments must more than 2\n");
        goto _ERROR;
    }

    dst = argv[1];
    // codecName = argv[2];
    

    // 2. 查找编码器：两种方式，by_name可以使用外部的库，通过CODEC_ID的则使用FFMpeg自带的库
    // codec = avcodec_find_encoder_by_name("libx264");
    codec = avcodec_find_encoder_by_name("libfdk_aac");
    // codec = avcodec_find_encoder(AV_CODEC_ID_AAC); // 用这个会涉及下面注释的一系列修改
    if (!codec) {
        av_log(NULL, AV_LOG_ERROR, "don't find codec name %s\n", codecName);
        goto _ERROR;
    }
    // 3. 创建编码器上下文
    ctx = avcodec_alloc_context3(codec);
    if (!ctx) {
        av_log(NULL, AV_LOG_ERROR, "No memory\n");
        goto _ERROR;
    }
    // 4. 设置编码器参数
    ctx->bit_rate = 64000;
    ctx->sample_fmt = AV_SAMPLE_FMT_S16;// AV_SAMPLE_FMT_FLTP，对于ffmpeg内部的编码器 用这个格式
    ret = check_sample_fmt(codec, AV_SAMPLE_FMT_S16); 
    if (ret == 0) {
        av_log(NULL, AV_LOG_ERROR, "encoder do not support sample format\n");
        goto _ERROR;
    }

    // 自定义函数找到最接近44100的采样率
    ctx->sample_rate = select_best_sample_rate(codec);
    // 设置声道布局为立体声stereo
    av_channel_layout_copy(&ctx->ch_layout, &(AVChannelLayout)AV_CHANNEL_LAYOUT_STEREO); // AV_CHANNEL_LAYOUT_MONO ffmpeg内部编码器修改成这个

    // 5. 编码器与编码器上下文绑定
    ret = avcodec_open2(ctx, codec, NULL);
    if (ret < 0) {
        av_log(ctx, AV_LOG_ERROR, "Don't open codec: %s", av_err2str(ret));
        goto _ERROR;
    }
    // 6. 创建输出文件
    f = fopen(dst, "wb");
    if (!f) {
        av_log(NULL, AV_LOG_ERROR, "Don't open file %s\n", dst);
        goto _ERROR;
    }

    // 7. 创建AVFrame
    frame = av_frame_alloc();
    if (!frame) {
        av_log(NULL, AV_LOG_ERROR, "No memory\n");
        goto _ERROR;
    }
    // 必须赋值才能获取到buffer
    frame->nb_samples = ctx->frame_size;
    frame->format = ctx->sample_fmt;
    av_channel_layout_copy(&frame->ch_layout, &ctx->ch_layout);

    // frame中真正存储数据的位置不会被av_frame_alloc生成，还需要调用av_frame_get_buffer获取
    ret = av_frame_get_buffer(frame, 0);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Could't not allocate the video frame\n");
        goto _ERROR;
    }
    // 8. 创建AVPacket
    pkt = av_packet_alloc();
    if (!pkt) {
        av_log(NULL, AV_LOG_ERROR, "No memory\n");
        goto _ERROR;
    }
    // 9. 生成音频内容
    float t = 0.0;
    // 有空了解一下这个值
    float tincr = 2 * M_PI * 440 / ctx->sample_rate;
    for (int i = 0; i < 200; ++i) {
        ret = av_frame_make_writable(frame);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "do not allocate space\n");
            goto _ERROR;
        }
        samples = (uint16_t*)frame->data[0]; // FLTP是32位的 所以改成uint32, 下面所有2都得改成4
        // 2字节一个数据
        for (int j = 0; j < ctx->frame_size; ++j) {
            samples[2 * j] = (sin(t) * 10000);
            // 多声道的处理
            for (int k = 1; k < ctx->ch_layout.nb_channels; ++k) {
                samples[2 * j + k] = samples[2 * j];
            }
            t += tincr;
        }
        encode(ctx, frame, pkt, f);
    }
    // 输出一个空包，用于刷新剩余数据
    encode(ctx, NULL, pkt, f);
_ERROR:
    if (ctx) {
        avcodec_free_context(&ctx);
    }
    
    if (frame) {
        av_frame_free(&frame);
    }
    
    if (pkt) {
        av_packet_free(&pkt);
    }

    if (f) {
        fclose(f);
    }
    return 0;
}
