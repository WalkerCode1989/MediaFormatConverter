#include "media_converter.h"

typedef struct {
    AVCodecContext *input_codec_context;
    AVCodecContext *output_codec_context;
    SwrContext *resample_context;
    int stream_index;
} MediaStreamContext;

static void init_media_stream_context(MediaStreamContext *stream_context) {
    stream_context->input_codec_context = NULL;
    stream_context->output_codec_context = NULL;
    stream_context->resample_context = NULL;
    stream_context->stream_index = -1;
}
static void destroy_media_stream_context(MediaStreamContext *stream_context) {
    if (stream_context->input_codec_context)
        avcodec_free_context(&(stream_context->input_codec_context));
    if (stream_context->output_codec_context)
        avcodec_free_context(&(stream_context->output_codec_context));
    if (stream_context->resample_context)
        swr_free(&(stream_context->resample_context));
}

int main(int argc, char **argv)
{
    AVFormatContext *input_format_context = NULL, *output_format_context = NULL;
    AVAudioFifo *fifo = NULL;
    int ret = AVERROR_EXIT;

    MediaStreamContext video_stream_context, audio_stream_context;
    init_media_stream_context(&audio_stream_context);
    destroy_media_stream_context(&video_stream_context);

    if (argc != 3) {
        fprintf(stderr, "Usage: %s <input file> <output file>\n", argv[0]);
        exit(1);
    }

    /* Open the input file for reading. */
    if (open_input_file(argv[1], &input_format_context))
        goto cleanup;
    
    /* Prepare the input codec with audio stream type*/
    if (prepare_stream_decoder(AVMEDIA_TYPE_AUDIO, &(audio_stream_context.stream_index),
                               &input_format_context, &(audio_stream_context.input_codec_context)))
        goto cleanup;

    /* Prepare the input codec with video stream type*/
    if (prepare_stream_decoder(AVMEDIA_TYPE_VIDEO, &(video_stream_context.stream_index),
                               &input_format_context, &(video_stream_context.input_codec_context)))
        video_stream_context.stream_index = -1;

    /* Open the output file for writing. */
    if (open_output_file(argv[2], &output_format_context))
        goto cleanup;

    if (prepare_output_codec(input_format_context, audio_stream_context.input_codec_context,
                         &output_format_context, &(audio_stream_context.output_codec_context)))
        goto cleanup;

    /* Initialize the resampler to be able to convert audio sample formats. */
    if (init_resampler(audio_stream_context.input_codec_context, audio_stream_context.output_codec_context,
                       &(audio_stream_context.resample_context)))
        goto cleanup;
    /* Initialize the FIFO buffer to store audio samples to be encoded. */
    if (init_fifo(&fifo, audio_stream_context.output_codec_context))
        goto cleanup;
    /* Write the header of the output file container. */
    if (write_output_file_header(output_format_context))
        goto cleanup;

    /* Loop as long as we have input samples to read or output samples
     * to write; abort as soon as we have neither. */
    while (1) {
        /* Use the encoder's desired frame size for processing. */
        const int output_frame_size = audio_stream_context.output_codec_context->frame_size;
        int finished                = 0;
        
        /* Since the decoder's and the encoder's frame size may differ, we
         * need to FIFO buffer to store as many frames worth of input samples
         * that they make up at least one frame worth of output samples. */
        while (av_audio_fifo_size(fifo) < output_frame_size) {
            /* Decode one frame worth of audio samples, convert it to the
             * output sample format and put it into the FIFO buffer. */
            if (read_decode_convert_and_store(fifo, input_format_context, output_format_context,
                                              audio_stream_context.input_codec_context,
                                              audio_stream_context.output_codec_context,
                                              audio_stream_context.resample_context,
                                              video_stream_context.input_codec_context, 
                                              video_stream_context.stream_index,
                                              &finished))
                goto cleanup;

            /* If we are at the end of the input file, we continue
             * encoding the remaining audio samples to the output file. */
            if (finished)
                break;
        }

        /* If we have enough samples for the encoder, we encode them.
         * At the end of the file, we pass the remaining samples to
         * the encoder. */
        while (av_audio_fifo_size(fifo) >= output_frame_size ||
               (finished && av_audio_fifo_size(fifo) > 0))
            /* Take one frame worth of audio samples from the FIFO buffer,
             * encode it and write it to the output file. */
            if (load_encode_and_write(fifo, output_format_context,
                                      audio_stream_context.output_codec_context, input_format_context))
                goto cleanup;

        /* If we are at the end of the input file and have encoded
         * all remaining samples, we can exit this loop and finish. */
        if (finished) {
            int data_written;
            /* Flush the encoder as it may have delayed frames. */
            do {
                data_written = 0;
                if (encode_audio_frame(NULL, output_format_context,
                                       audio_stream_context.output_codec_context, &data_written, input_format_context))
                    goto cleanup;
            } while (data_written);
            break;
        }
    }

    /* Write the trailer of the output file container. */
    if (write_output_file_trailer(output_format_context))
        goto cleanup;
    ret = 0;
    printf("finish the convert task from %s to %s\n", argv[1], argv[2]);
cleanup:
    if (fifo)
        av_audio_fifo_free(fifo);

    if (output_format_context) {
        avio_closep(&output_format_context->pb);
        avformat_free_context(output_format_context);
    }
    destroy_media_stream_context(&video_stream_context);
    destroy_media_stream_context(&audio_stream_context);
    if (input_format_context)
        avformat_close_input(&input_format_context);

    return ret;
}