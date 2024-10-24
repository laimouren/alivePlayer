#include <libavformat/avformat.h>
#include <libavutil/log.h>

int main (int argc, char *argv[]) {
    AVFormatContext *fmt_cxt = NULL;
    int ret;
    av_log_set_level(AV_LOG_INFO);

    ret = avformat_open_input(&fmt_cxt, "/root/resource/1.mp4", NULL, NULL);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Can't open file: %s", av_err2str(ret));
        return -1;
    }

    av_dump_format(fmt_cxt, 0, "/root/resource/1.mp4", 0);

    avformat_close_input(&fmt_cxt);
    
    return 0;
}