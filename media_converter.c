#include "media_converter.h"

/* The output bit rate in bit/s */
#define OUTPUT_BIT_RATE 128000
/* The number of output channels */
#define OUTPUT_CHANNELS 2
/* The number of output Opus sample rate */
#define OUTPUT_OPUS_SAMPLE_RATE 48000

/**
 * Open an input file.
 * @param      filename             File to be opened
 * @param[out] input_format_context Format context of opened file
 * @return Error code (0 if successful)
 */
int open_input_file(const char *filename, AVFormatContext **input_format_context)
{
    int error;

    /* Open the input file to read from it. */
    if ((error = avformat_open_input(input_format_context, filename, NULL,
                                     NULL)) < 0) {
        fprintf(stderr, "Could not open input file '%s' (error '%s')\n",
                filename, av_err2str(error));
        *input_format_context = NULL;
        return error;
    }

    /* Get information on the input file (number of streams etc.). */
    if ((error = avformat_find_stream_info(*input_format_context, NULL)) < 0) {
        fprintf(stderr, "Could not open find stream info (error '%s')\n",
                av_err2str(error));
        avformat_close_input(input_format_context);
        return error;
    }
    av_dump_format(*input_format_context, 0, filename, 0);
    return 0;
}

/**
 * Prepare the required decoder by stream type.
 * @param      stream_type          Stream type of AVMediaType 
 * @param[out] stream_type_index    Stream type's index of input file
 * @param[out] input_format_context Format context of input file
 * @param[out] input_codec_context  Codec context of input file
 * @return Error code (0 if successful)
 */
int prepare_stream_decoder( enum AVMediaType stream_type, int *stream_type_index,
                            AVFormatContext **input_format_context,
                            AVCodecContext **input_codec_context)
{
    AVCodecContext *input_codec_ctx;
    AVCodec *input_codec;
    int stream_index;
    int error;

    stream_index = av_find_best_stream(*input_format_context, stream_type, -1, -1, NULL, 0);
    if (stream_index < 0) {
        printf("Could not find stream type(%d)\n", stream_type);
        *stream_type_index = -1;
        return -1; 
    } else {
        *stream_type_index = stream_index;
        printf("Find stream type(%d) at index: %d\n", stream_type, *stream_type_index);
    }

    /* Find a decoder for the video/audio stream. */
    if (!(input_codec = avcodec_find_decoder((*input_format_context)->streams[stream_index]->codecpar->codec_id))) {
        fprintf(stderr, "Could not find input codec\n");
        return AVERROR_EXIT;
    }

    /* Allocate a new decoding context. */
    input_codec_ctx = avcodec_alloc_context3(input_codec);
    if (!input_codec_ctx) {
        fprintf(stderr, "Could not allocate a decoding context\n");
        return AVERROR(ENOMEM);
    }

    /* Initialize the stream parameters with demuxer information. */
    error = avcodec_parameters_to_context(input_codec_ctx, (*input_format_context)->streams[stream_index]->codecpar);
    if (error < 0) {
        avcodec_free_context(&input_codec_ctx);
        return error;
    }

    /* Open the decoder for the audio stream to use it later. */
    if ((error = avcodec_open2(input_codec_ctx, input_codec, NULL)) < 0) {
        fprintf(stderr, "Could not open input codec (error '%s')\n",
                av_err2str(error));
        avcodec_free_context(&input_codec_ctx);
        return error;
    }

    /* Save the decoder context for easier access later. */
    *input_codec_context = input_codec_ctx;

    return 0;
}

/**
 * Open an output file and create output_format_context.
 * @param      filename              File to be opened
 * @param[out] output_format_context Format context of output file
 * @return Error code (0 if successful)
 */
int open_output_file(const char *filename, AVFormatContext **output_format_context)
{
    AVIOContext *output_io_context = NULL;
    int error;

    /* Open the output file to write to it. */
    if ((error = avio_open(&output_io_context, filename,
                           AVIO_FLAG_WRITE)) < 0) {
        fprintf(stderr, "Could not open output file '%s' (error '%s')\n",
                filename, av_err2str(error));
        return error;
    }

    /* Create a new format context for the output container format. */
    if (!(*output_format_context = avformat_alloc_context())) {
        fprintf(stderr, "Could not allocate output format context\n");
        return AVERROR(ENOMEM);
    }

    /* Associate the output file (pointer) with the container format context. */
    (*output_format_context)->pb = output_io_context;

    /* Guess the desired container format based on the file extension. */
    if (!((*output_format_context)->oformat = av_guess_format(NULL, filename,
                                                              NULL))) {
        fprintf(stderr, "Could not find output file format\n");
        goto cleanup;
    }

    if (!((*output_format_context)->url = av_strdup(filename))) {
        fprintf(stderr, "Could not allocate url.\n");
        error = AVERROR(ENOMEM);
        goto cleanup;
    }

    return 0;
cleanup:
    avio_closep(&(*output_format_context)->pb);
    avformat_free_context(*output_format_context);
    *output_format_context = NULL;
    return error < 0 ? error : AVERROR_EXIT;
}

/**
 * Prepare the output required encoder.
 * Also set some basic encoder parameters.
 * Some of these parameters are based on the input file's parameters.
 * @param      input_format_context  Codec context of input file
 * @param      input_codec_context   Codec context of input file
 * @param[out] output_format_context Format context of output file
 * @param[out] output_codec_context  Codec context of output file
 * @return Error code (0 if successful)
 */
int prepare_output_codec(AVFormatContext *input_format_context,
                         AVCodecContext *input_codec_context,
                         AVFormatContext **output_format_context,
                         AVCodecContext **output_codec_context)
{
    AVCodecContext *avctx          = NULL;
    AVStream *stream               = NULL;
    AVCodec *output_codec          = NULL;
    int error;
    int stream_index               = -1;
    for (int i = 0; i < input_format_context->nb_streams; i++) {
        AVStream *in_stream = input_format_context->streams[i];
        if (in_stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            /* video output encoder prepare */
            /* currently, only support the video stream bypass*/
            AVStream *out_stream = avformat_new_stream(*output_format_context, NULL);
        
            if (!out_stream) {
                fprintf(stderr, "Failed allocating output stream\n");
                error = AVERROR_UNKNOWN;
                goto cleanup;
            }
        
            AVCodecParameters *in_codecpar = in_stream->codecpar;
            AVCodecParameters *out_codecpar = out_stream->codecpar;
        
            if (avcodec_parameters_copy(out_codecpar, in_codecpar) < 0) {
                fprintf(stderr, "Failed to copy codec parameters\n");
                return -1;
            }
            // Copy the time_base while we're at it.
            out_stream->time_base = in_stream->time_base;

        } else if(in_stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            stream_index = i;

            /* audio output encoder prepare */
            /* Find the encoder to be used by its name. */
            if (!(output_codec = avcodec_find_encoder(AV_CODEC_ID_OPUS))) {
                fprintf(stderr, "Could not find an AAC encoder.\n");
                goto cleanup;
            }
        
            /* Create a new audio stream in the output file container. */
            if (!(stream = avformat_new_stream(*output_format_context, NULL))) {
                fprintf(stderr, "Could not create new stream\n");
                error = AVERROR(ENOMEM);
                goto cleanup;
            }
        
            avctx = avcodec_alloc_context3(output_codec);
            if (!avctx) {
                fprintf(stderr, "Could not allocate an encoding context\n");
                error = AVERROR(ENOMEM);
                goto cleanup;
            }
            
            /* Set the basic encoder parameters.
             * The input file's sample rate is used to avoid a sample rate conversion. */
            avctx->channels       = OUTPUT_CHANNELS;
            avctx->channel_layout = av_get_default_channel_layout(OUTPUT_CHANNELS);
            avctx->sample_rate    = OUTPUT_OPUS_SAMPLE_RATE;
            avctx->sample_fmt     = output_codec->sample_fmts[0];
            avctx->bit_rate       = OUTPUT_BIT_RATE;
            avctx->codec_tag      = 0;
            avctx->codec_id       = AV_CODEC_ID_OPUS;
            avctx->codec_type     = AVMEDIA_TYPE_AUDIO;
        
            /* Set the sample rate for the container. */
            stream->time_base.den = OUTPUT_OPUS_SAMPLE_RATE;
            stream->time_base.num = 1;
        
            /* Some container formats (like MP4) require global headers to be present.
             * Mark the encoder so that it behaves accordingly. */
            if ((*output_format_context)->oformat->flags & AVFMT_GLOBALHEADER)
                avctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
        
            /* Open the encoder for the audio stream to use it later. */
            if ((error = avcodec_open2(avctx, output_codec, NULL)) < 0) {
                fprintf(stderr, "Could not open output codec (error '%s')\n",
                        av_err2str(error));
                goto cleanup;
            }
        
            error = avcodec_parameters_from_context(stream->codecpar, avctx);
            if (error < 0) {
                fprintf(stderr, "Could not initialize stream parameters\n");
                goto cleanup;
            }
            //stream->codecpar->codec_tag = av_codec_get_tag2((*output_format_context)->oformat->codec_tag, avctx->codec_id);
            /* Save the encoder context for easier access later. */
            *output_codec_context = avctx;
        }
    }
    return 0;

cleanup:
    avcodec_free_context(&avctx);
    avio_closep(&(*output_format_context)->pb);
    avformat_free_context(*output_format_context);
    *output_format_context = NULL;
    return error < 0 ? error : AVERROR_EXIT;
}

/**
 * Initialize one data packet for reading or writing.
 * @param packet Packet to be initialized
 */
void init_packet(AVPacket *packet)
{
    av_init_packet(packet);
    /* Set the packet data and size so that it is recognized as being empty. */
    packet->data = NULL;
    packet->size = 0;
}

/**
 * Initialize one audio frame for reading from the input file.
 * @param[out] frame Frame to be initialized
 * @return Error code (0 if successful)
 */
int init_input_frame(AVFrame **frame)
{
    if (!(*frame = av_frame_alloc())) {
        fprintf(stderr, "Could not allocate input frame\n");
        return AVERROR(ENOMEM);
    }
    return 0;
}

/**
 * Initialize the audio resampler based on the input and output codec settings.
 * If the input and output sample formats differ, a conversion is required
 * libswresample takes care of this, but requires initialization.
 * @param      input_codec_context  Codec context of the input file
 * @param      output_codec_context Codec context of the output file
 * @param[out] resample_context     Resample context for the required conversion
 * @return Error code (0 if successful)
 */
int init_resampler(AVCodecContext *input_codec_context,
                          AVCodecContext *output_codec_context,
                          SwrContext **resample_context)
{
        int error;

        /*
         * Create a resampler context for the conversion.
         * Set the conversion parameters.
         * Default channel layouts based on the number of channels
         * are assumed for simplicity (they are sometimes not detected
         * properly by the demuxer and/or decoder).
         */
        *resample_context = swr_alloc_set_opts(NULL,
                                              av_get_default_channel_layout(output_codec_context->channels),
                                              output_codec_context->sample_fmt,
                                              output_codec_context->sample_rate,
                                              av_get_default_channel_layout(input_codec_context->channels),
                                              input_codec_context->sample_fmt,
                                              input_codec_context->sample_rate,
                                              0, NULL);
        if (!*resample_context) {
            fprintf(stderr, "Could not allocate resample context\n");
            return AVERROR(ENOMEM);
        }
        /*
        int64_t compensation_distance = av_rescale_rnd(swr_get_delay(*resample_context, input_codec_context->sample_rate) + input_codec_context->frame_size,
                                               output_codec_context->sample_rate,
                                               input_codec_context->sample_rate,
                                               AV_ROUND_UP);
        swr_set_compensation(*resample_context, compensation_distance, compensation_distance);
        */
        /* Open the resampler with the specified parameters. */
        if ((error = swr_init(*resample_context)) < 0) {
            fprintf(stderr, "Could not open resample context\n");
            swr_free(resample_context);
            return error;
        }
    return 0;
}

/**
 * Initialize a FIFO buffer for the audio samples to be encoded.
 * @param[out] fifo                 Sample buffer
 * @param      output_codec_context Codec context of the output file
 * @return Error code (0 if successful)
 */
int init_fifo(AVAudioFifo **fifo, AVCodecContext *output_codec_context)
{
    /* Create the FIFO buffer based on the specified output sample format. */
    if (!(*fifo = av_audio_fifo_alloc(output_codec_context->sample_fmt,
                                      output_codec_context->channels, output_codec_context->frame_size))) {
        fprintf(stderr, "Could not allocate FIFO\n");
        return AVERROR(ENOMEM);
    }
    return 0;
}

/**
 * Write the header of the output file container.
 * @param output_format_context Format context of the output file
 * @return Error code (0 if successful)
 */
int write_output_file_header(AVFormatContext *output_format_context)
{
    int error;
    if ((error = avformat_write_header(output_format_context, NULL)) < 0) {
        fprintf(stderr, "Could not write output file header (error '%s')\n",
                av_err2str(error));
        return error;
    }
    return 0;
}

/**
 * Decode one audio frame from the input file.
 * @param      frame                      Frame to be decoded
 * @param      input_format_context       Format context of the input file
 * @param      input_audio_codec_context  Audio codec context of the input file
 * @param      output_format_context      Format context of the output file
 * @param      input_video_codec_context  Video codec context of the input file
 * @param      video_stream_index         Video stream index
 * @param[out] data_present               Indicates whether data has been decoded
 * @param[out] finished                   Indicates whether the end of file has
 *                                        been reached and all data has been
 *                                        decoded. If this flag is false, there
 *                                        is more data to be decoded, i.e., this
 *                                        function has to be called again.
 * @return Error code (0 if successful)
 */
int decode_frame(AVFrame *frame,
                 AVFormatContext *input_format_context,
                 AVCodecContext *input_audio_codec_context,
                 AVFormatContext *output_format_context,
                 AVCodecContext *input_video_codec_context,
                 const int video_stream_index,
                 int *data_present, int *finished)
{
    /* Packet used for temporary storage. */
    AVPacket input_packet;
    AVCodecContext *input_codec_context = NULL;
    int error;
    init_packet(&input_packet);

    /* Read one audio frame from the input file into a temporary packet. */
    if ((error = av_read_frame(input_format_context, &input_packet)) < 0) {
        /* If we are at the end of the file, flush the decoder below. */
        if (error == AVERROR_EOF)
            *finished = 1;
        else {
            fprintf(stderr, "Could not read frame (error '%s')\n",
                    av_err2str(error));
            return error;
        }
    }

    if (input_packet.stream_index == video_stream_index) {
        input_codec_context = input_video_codec_context;
    } else {
        input_codec_context = input_audio_codec_context;
    }

    /* Send the frame stored in the temporary packet to the decoder.
     * The input stream decoder is used to do this. */
    if ((error = avcodec_send_packet(input_codec_context, &input_packet)) < 0) {
        fprintf(stderr, "Could not send packet for decoding (error '%s')\n",
                av_err2str(error));
        return error;
    }

    /* Receive one frame from the decoder. */
    error = avcodec_receive_frame(input_codec_context, frame);
    /* return indicating that no data is present. */
    if (error == AVERROR(EAGAIN)) {
        error = 0;
        goto cleanup;
    /* If the end of the input file is reached, stop decoding. */
    } else if (error == AVERROR_EOF) {
        *finished = 1;
        error = 0;
        goto cleanup;
    } else if (error < 0) {
        fprintf(stderr, "Could not decode frame (error '%s')\n",
                av_err2str(error));
        goto cleanup;
    } else {
        /* TODO： If want to transcode the video stream, need to modify */
        if (input_packet.stream_index == video_stream_index) {
            av_packet_rescale_ts(&input_packet, input_format_context->streams[video_stream_index]->time_base,
                                 output_format_context->streams[video_stream_index]->time_base);
        
            if (av_interleaved_write_frame(output_format_context, &input_packet) < 0) {
                //fprintf(stderr, "Failed to write video packet\n");
            }
            error = 0;
        } else { /*input_packet is audio stream, let other api to handle resample*/ 
            /* Default case: Return decoded data. */
            *data_present = 1;
        }

        goto cleanup;
    }

cleanup:
    av_packet_unref(&input_packet);
    return error;
}

/**
 * Initialize a temporary storage for the specified number of audio samples.
 * The conversion requires temporary storage due to the different format.
 * The number of audio samples to be allocated is specified in frame_size.
 * @param[out] converted_input_samples Array of converted samples. The
 *                                     dimensions are reference, channel
 *                                     (for multi-channel audio), sample.
 * @param      output_codec_context    Codec context of the output file
 * @param      frame_size              Number of samples to be converted in
 *                                     each round
 * @return Error code (0 if successful)
 */
int init_converted_samples(uint8_t ***converted_input_samples,
                           AVCodecContext *output_codec_context,
                           int frame_size)
{
    int error;

    /* Allocate as many pointers as there are audio channels.
     * Each pointer will later point to the audio samples of the corresponding
     * channels (although it may be NULL for interleaved formats).
     */
    if (!(*converted_input_samples = calloc(output_codec_context->channels,
                                            sizeof(**converted_input_samples)))) {
        fprintf(stderr, "Could not allocate converted input sample pointers\n");
        return AVERROR(ENOMEM);
    }

    /* Allocate memory for the samples of all channels in one consecutive
     * block for convenience. */
    if ((error = av_samples_alloc(*converted_input_samples, NULL,
                                  output_codec_context->channels,
                                  frame_size,
                                  output_codec_context->sample_fmt, 0)) < 0) {
        fprintf(stderr,
                "Could not allocate converted input samples (error '%s')\n",
                av_err2str(error));
        av_freep(&(*converted_input_samples)[0]);
        free(*converted_input_samples);
        return error;
    }
    return 0;
}

/**
 * Convert the input audio samples into the output sample format.
 * The conversion happens on a per-frame basis, the size of which is
 * specified by frame_size.
 * @param      input_data       Samples to be decoded. The dimensions are
 *                              channel (for multi-channel audio), sample.
 * @param[out] converted_data   Converted samples. The dimensions are channel
 *                              (for multi-channel audio), sample.
 * @param      input_samples    Number of samples of input
 * @param      output_samples   Number of samples to be converted
 * @param      resample_context Resample context for the conversion
 * @return Error code (0 if successful)
 */
int convert_samples(const uint8_t **input_data, uint8_t **converted_data,
                    const int input_samples, const int out_samples, 
                    SwrContext *resample_context)
{
    int ret = 0;
    ret = swr_convert(resample_context, 
                      converted_data, out_samples, 
                      input_data, input_samples);
    if (ret < 0) {
        fprintf(stderr, "Error resampling audio\n");
        return ret;
    }

    return 0;
}

/**
 * Add converted input audio samples to the FIFO buffer for later processing.
 * @param fifo                    Buffer to add the samples to
 * @param converted_input_samples Samples to be added. The dimensions are channel
 *                                (for multi-channel audio), sample.
 * @param frame_size              Number of samples to be converted
 * @return Error code (0 if successful)
 */
int add_samples_to_fifo(AVAudioFifo *fifo,
                        uint8_t **converted_input_samples,
                        const int frame_size)
{
    int error;

    /* Make the FIFO as large as it needs to be to hold both,
     * the old and the new samples. */
    if ((error = av_audio_fifo_realloc(fifo, av_audio_fifo_size(fifo) + frame_size)) < 0) {
        fprintf(stderr, "Could not reallocate FIFO\n");
        return error;
    }

    /* Store the new samples in the FIFO buffer. */
    if (av_audio_fifo_write(fifo, (void **)converted_input_samples,
                            frame_size) < frame_size) {
        fprintf(stderr, "Could not write data to FIFO\n");
        return AVERROR_EXIT;
    }
    return 0;
}

/**
 * Read one audio frame from the input file, decode, convert and store
 * it in the FIFO buffer.
 * @param      fifo                       Buffer used for temporary storage
 * @param      input_format_context       Format context of the input file
 * @param      output_format_context      Format context of the output file
 * @param      input_audio_codec_context  Audio codec context of the input file
 * @param      output_audio_codec_context Audio codec context of the output file
 * @param      audio_resampler_context    Audio resample context for the conversion
 * @param      input_video_codec_context  Video codec context of the input file
 * @param      video_stream_index         Video of stream array index
 * @param[out] finished                   Indicates whether the end of file has
 *                                        been reached and all data has been
 *                                        decoded. If this flag is false,
 *                                        there is more data to be decoded,
 *                                        i.e., this function has to be called
 *                                        again.
 * @return Error code (0 if successful)
 */
int read_decode_convert_and_store(AVAudioFifo *fifo,
                                  AVFormatContext *input_format_context,
                                  AVFormatContext *output_formt_context,
                                  AVCodecContext *input_audio_codec_context,
                                  AVCodecContext *output_audio_codec_context,
                                  SwrContext *audio_resampler_context,
                                  AVCodecContext *input_video_codec_context,
                                  int video_stream_index,
                                  int *finished)
{
    /* Temporary storage of the input samples of the frame read from the file. */
    AVFrame *input_frame = NULL;
    /* Temporary storage for the converted input samples. */
    uint8_t **converted_input_samples = NULL;
    int data_present = 0;
    int ret = AVERROR_EXIT;

    /* Initialize temporary storage for one input frame. */
    if (init_input_frame(&input_frame))
        goto cleanup;
    /* Decode one frame worth of audio samples. */
    if (decode_frame(input_frame, input_format_context, input_audio_codec_context,
                     output_formt_context, input_video_codec_context,
                     video_stream_index, &data_present, finished))
        goto cleanup;
    /* If we are at the end of the file and there are no more samples
     * in the decoder which are delayed, we are actually finished.
     * This must not be treated as an error. */
    if (*finished) {
        ret = 0;
        goto cleanup;
    }
    /* If there is decoded data, convert and store it. */
    if (data_present) {
        /* Initialize the temporary storage for the converted input samples. */
        if (init_converted_samples(&converted_input_samples, output_audio_codec_context,
                                   input_frame->nb_samples))
            goto cleanup;
        
        /* Convert the samples using the resampler. */
        static int max_out_samples = 0, out_samples = 0, out_linesize = 0;
        max_out_samples = out_samples = av_rescale_rnd(input_frame->nb_samples, output_audio_codec_context->sample_rate, input_frame->sample_rate,
                                                       AV_ROUND_UP);
        out_samples = av_rescale_rnd(swr_get_delay(audio_resampler_context, input_frame->sample_rate) +
                                                       input_frame->nb_samples, output_audio_codec_context->sample_rate, input_frame->sample_rate,
                                                       AV_ROUND_UP);
        if (!(*converted_input_samples = calloc(output_audio_codec_context->channels, sizeof(**converted_input_samples)))) {
            fprintf(stderr, "Could not allocate converted input sample pointers\n");
        }

        max_out_samples = out_samples; 
        ret = av_samples_alloc_array_and_samples(&converted_input_samples, &out_linesize,
                                                      output_audio_codec_context->channels, out_samples, output_audio_codec_context->sample_fmt, 0);
        if (ret < 0) {
            fprintf(stderr, "Error allocating resampled buffer\n");
            return ret;
        }

        if (convert_samples((const uint8_t**)input_frame->extended_data, 
                            converted_input_samples, input_frame->nb_samples,
                            out_samples, audio_resampler_context))
            goto cleanup;

        /* Add the converted input samples to the FIFO buffer for later processing. */
        if (add_samples_to_fifo(fifo, converted_input_samples,
                                out_samples)) {
            goto cleanup;
        }
        ret = 0;
    }
    ret = 0;

cleanup:
    if (converted_input_samples) {
        av_freep(&converted_input_samples[0]);
        free(converted_input_samples);
    }

    av_frame_free(&input_frame);

    return ret;
}

/**
 * Initialize one input frame for writing to the output file.
 * The frame will be exactly frame_size samples large.
 * @param[out] frame                Frame to be initialized
 * @param      output_codec_context Codec context of the output file
 * @param      frame_size           Size of the frame
 * @return Error code (0 if successful)
 */
int init_output_frame(AVFrame **frame,
                             AVCodecContext *output_codec_context,
                             int frame_size)
{
    int error;

    /* Create a new frame to store the audio samples. */
    if (!(*frame = av_frame_alloc())) {
        fprintf(stderr, "Could not allocate output frame\n");
        return AVERROR_EXIT;
    }

    /* Set the frame's parameters, especially its size and format.
     * av_frame_get_buffer needs this to allocate memory for the
     * audio samples of the frame.
     * Default channel layouts based on the number of channels
     * are assumed for simplicity. */
    (*frame)->nb_samples     = frame_size;
    (*frame)->channel_layout = output_codec_context->channel_layout;
    (*frame)->format         = output_codec_context->sample_fmt;
    (*frame)->sample_rate    = output_codec_context->sample_rate;

    /* Allocate the samples of the created frame. This call will make
     * sure that the audio frame can hold as many samples as specified. */
    if ((error = av_frame_get_buffer(*frame, 0)) < 0) {
        fprintf(stderr, "Could not allocate output frame samples (error '%s')\n",
                av_err2str(error));
        av_frame_free(frame);
        return error;
    }

    return 0;
}

/* Global timestamp for the audio frames. */
static double pts = 0;
static double duration = 0;

/**
 * Encode one frame worth of audio to the output file.
 * @param      frame                 Samples to be encoded
 * @param      output_format_context Format context of the output file
 * @param      output_codec_context  Codec context of the output file
 * @param[out] data_present          Indicates whether data has been
 *                                   encoded
 * @return Error code (0 if successful)
 */
int encode_audio_frame(AVFrame *frame,
                              AVFormatContext *output_format_context,
                              AVCodecContext *output_codec_context,
                              int *data_present,
                              AVFormatContext *input_format_context)
{
    /* Packet used for temporary storage. */
    AVPacket output_packet;
    int error;
    init_packet(&output_packet);

    /* Set a timestamp based on the sample rate for the container. */

    if (frame) {
#if 1
        frame->pts = (int64_t)pts;
        pts += frame->nb_samples;
#else
        frame->pts = (int64_t)pts;
        duration = (double)frame->nb_samples / (double)output_codec_context->sample_rate;
        pts += duration;
#endif
    } else {
        error = 0;
        goto cleanup;
    }
 
    /* Send the audio frame stored in the temporary packet to the encoder.
     * The output audio stream encoder is used to do this. */
    error = avcodec_send_frame(output_codec_context, frame);
    /* The encoder signals that it has nothing more to encode. */
    if (error == AVERROR_EOF) {
        error = 0;
        goto cleanup;
    } else if (error < 0) {
        fprintf(stderr, "Could not send packet for encoding (error '%s')\n",
                av_err2str(error));
        return error;
    }

    /* Receive one encoded frame from the encoder. */
    error = avcodec_receive_packet(output_codec_context, &output_packet);
    /* If the encoder asks for more data to be able to provide an
     * encoded frame, return indicating that no data is present. */
    if (error == AVERROR(EAGAIN)) {
        error = 0;
        goto cleanup;
    /* If the last frame has been encoded, stop encoding. */
    } else if (error == AVERROR_EOF) {
        error = 0;
        goto cleanup;
    } else if (error < 0) {
        fprintf(stderr, "Could not encode frame (error '%s')\n",
                av_err2str(error));
        goto cleanup;
    /* Default case: Return encoded data. */
    } else {
        *data_present = 1;
    }

    output_packet.stream_index = output_format_context->nb_streams -1;
    /*
    output_packet.pts = pts;
    output_packet.duration = av_rescale_q(frame->nb_samples, (AVRational){1, output_codec_context->sample_rate}, output_codec_context->time_base);

    duration = (double)frame->nb_samples / (double)output_codec_context->sample_rate;
    pts += duration;
*/
    /* Write one audio frame from the temporary packet to the output file. */
    if (*data_present &&
        (error = av_write_frame(output_format_context, &output_packet)) < 0) {
        fprintf(stderr, "Could not write frame (error '%s')\n",
                av_err2str(error));
        goto cleanup;
    } 

cleanup:
    av_packet_unref(&output_packet);
    return error;
}

/**
 * Load one audio frame from the FIFO buffer, encode and write it to the
 * output file.
 * @param fifo                  Buffer used for temporary storage
 * @param output_format_context Format context of the output file
 * @param output_codec_context  Codec context of the output file
 * @return Error code (0 if successful)
 */
int load_encode_and_write(AVAudioFifo *fifo,
                                 AVFormatContext *output_format_context,
                                 AVCodecContext *output_codec_context,
                                 AVFormatContext *input_format_context)
{
    /* Temporary storage of the output samples of the frame written to the file. */
    AVFrame *output_frame;
    /* Use the maximum number of possible samples per frame.
     * If there is less than the maximum possible frame size in the FIFO
     * buffer use this number. Otherwise, use the maximum possible frame size. */
    const int frame_size = FFMIN(av_audio_fifo_size(fifo),
                                 output_codec_context->frame_size);
    int data_written;

    /* Initialize temporary storage for one output frame. */
    if (init_output_frame(&output_frame, output_codec_context, frame_size))
        return AVERROR_EXIT;

    /* Read as many samples from the FIFO buffer as required to fill the frame.
     * The samples are stored in the frame temporarily. */
    if (av_audio_fifo_read(fifo, (void **)output_frame->data, frame_size) < frame_size) {
        fprintf(stderr, "Could not read data from FIFO\n");
        av_frame_free(&output_frame);
        return AVERROR_EXIT;
    }

    /* Encode one frame worth of audio samples. */
    if (encode_audio_frame(output_frame, output_format_context,
                           output_codec_context, &data_written, input_format_context)) {
        av_frame_free(&output_frame);
        return AVERROR_EXIT;
    }
    av_frame_free(&output_frame);
    return 0;
}

/**
 * Write the trailer of the output file container.
 * @param output_format_context Format context of the output file
 * @return Error code (0 if successful)
 */
int write_output_file_trailer(AVFormatContext *output_format_context)
{
    int error;
    if ((error = av_write_trailer(output_format_context)) < 0) {
        fprintf(stderr, "Could not write output file trailer (error '%s')\n",
                av_err2str(error));
        return error;
    }
    return 0;
}