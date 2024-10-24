#include <libavformat/avformat.h>
#include <libavutil/log.h>
#include <libavutil/avutil.h>

int main (int argc, char *argv[]) {
    // 1. 处理参数
    char *src;
    char *dst;

    int *stream_map = NULL;
    int stream_idx = 0;

    int ret;
    int idx = -1;
    int i = 0;
    AVFormatContext *pFmtCtx = NULL;
    AVFormatContext *oFmtCtx = NULL;

    const AVOutputFormat *outFmt = NULL;
    
    AVPacket pkt;

    av_log_set_level(AV_LOG_DEBUG);
    if (argc < 3) {
        av_log(NULL, AV_LOG_ERROR, "argument must be more than 3\n");
        exit(-1);
    }

    src = argv[1];
    dst = argv[2];

    // 2. 打开多媒体文件
    ret = avformat_open_input(&pFmtCtx, src, NULL, NULL);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "%s\n", av_err2str(ret));
        exit(-1);
    }
    
    // 4. 打开目的文件的上下文
    avformat_alloc_output_context2(&oFmtCtx, NULL, NULL, dst);
    if (!oFmtCtx) {
        av_log(NULL, AV_LOG_ERROR, "NO MEMORY\n");
        goto _ERROR;
    }

    stream_map = av_calloc(pFmtCtx->nb_streams, sizeof(int));
    if (!stream_map) {
        av_log(NULL, AV_LOG_ERROR, "NO MEMORY\n");
        goto _ERROR;
    }

    for (i = 0; i < pFmtCtx->nb_streams; ++i) {
        AVStream *outStream = NULL;
        AVStream *inStream = pFmtCtx->streams[i];
        AVCodecParameters *inCodecPar = inStream->codecpar;
        if (inCodecPar->codec_type != AVMEDIA_TYPE_AUDIO &&
            inCodecPar->codec_type != AVMEDIA_TYPE_VIDEO &&
            inCodecPar->codec_type != AVMEDIA_TYPE_SUBTITLE) {
            stream_map[i] = -1;
            continue;
        }
        stream_map[i] = stream_idx++;

        // 5. 为目的文件创建新的音频流
        outStream = avformat_new_stream(oFmtCtx, NULL);
        if (!outStream) {
            av_log(oFmtCtx, AV_LOG_ERROR, "NO MEMORY\n");
            goto _ERROR;
        }

        avcodec_parameters_copy(outStream->codecpar, inStream->codecpar);
        // 设置为0，会自动去找对应的编码器
        outStream->codecpar->codec_tag = 0;

    }
    
    ret = avio_open2(&oFmtCtx->pb, dst, AVIO_FLAG_WRITE, NULL, NULL);
    if (ret < 0) {
        av_log(oFmtCtx, AV_LOG_ERROR, "%s\n", av_err2str(ret));
        goto _ERROR;
    }
    
    // 7. 写多媒体文件头到目的文件
    ret = avformat_write_header(oFmtCtx, NULL);
    if (ret < 0) {
        av_log(oFmtCtx, AV_LOG_ERROR, "%s\n", av_err2str(ret));
        goto _ERROR;
    }
    // 8. 从源多媒体文件中读音频数据到目的文件中
    while (av_read_frame(pFmtCtx, &pkt) >= 0) {
        AVStream *outStream = NULL;
        AVStream *inStream = NULL;
        
        if (stream_map[pkt.stream_index] < 0) {
            av_packet_unref(&pkt);
            continue;
        }
        inStream = pFmtCtx->streams[pkt.stream_index];

        pkt.stream_index = stream_map[pkt.stream_index];

        outStream = oFmtCtx->streams[pkt.stream_index];
        av_packet_rescale_ts(&pkt, inStream->time_base, outStream->time_base);
        // 设置成-1，自己会去计算
        pkt.pos = -1;
        av_interleaved_write_frame(oFmtCtx, &pkt);
        av_packet_unref(&pkt);
        
    }
    // 9. 写多媒体文件尾到目的文件
    av_write_trailer(oFmtCtx);
    // 10. 将申请的资源释放掉
_ERROR:
    if (pFmtCtx) {
        avformat_close_input(&pFmtCtx);
        pFmtCtx = NULL;
    }    
    if (oFmtCtx->pb) {
        avio_close(oFmtCtx->pb);
    }
    if (oFmtCtx) {
        avformat_free_context(oFmtCtx);
        oFmtCtx = NULL;
    }
    if (stream_map) {
        av_free(stream_map);
    }
    return 0;
}