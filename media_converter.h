#include <stdio.h>

#include "libavformat/avformat.h"
#include "libavformat/avio.h"

#include "libavcodec/avcodec.h"

#include "libavutil/audio_fifo.h"
#include "libavutil/avassert.h"
#include "libavutil/avstring.h"
#include "libavutil/frame.h"
#include "libavutil/opt.h"

#include "libswresample/swresample.h"


/**
 * Open an input file and the required decoder.
 * @param      filename             File to be opened
 * @param[out] input_format_context Format context of opened file
 * @return Error code (0 if successful)
 */
int open_input_file(const char *filename, AVFormatContext **input_format_context);

/**
 * Prepare the required decoder by stream type.
 * @param      stream_type   Stream type of AVMediaType 
 * @param[out] input_format_context Format context of input file
 * @param[out] input_codec_context  Codec context of input file
 * @return Error code (0 if successful)
 */
int prepare_stream_decoder( enum AVMediaType stream_type, int *stream_index,
                            AVFormatContext **input_format_context,
                            AVCodecContext **input_codec_context);

/**
 * Open an output file and the required encoder.
 * Also set some basic encoder parameters.
 * Some of these parameters are based on the input file's parameters.
 * @param      filename              File to be opened
 * @param      input_codec_context   Codec context of input file
 * @param[out] output_format_context Format context of output file
 * @param[out] output_codec_context  Codec context of output file
 * @return Error code (0 if successful)
 */
int open_output_file(const char *filename, AVFormatContext **output_format_context);

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
                         AVCodecContext **output_codec_context);

/**
 * Initialize one data packet for reading or writing.
 * @param packet Packet to be initialized
 */
void init_packet(AVPacket *packet); 

/**
 * Initialize one audio frame for reading from the input file.
 * @param[out] frame Frame to be initialized
 * @return Error code (0 if successful)
 */
int init_input_frame(AVFrame **frame);

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
                          SwrContext **resample_context);

/**
 * Initialize a FIFO buffer for the audio samples to be encoded.
 * @param[out] fifo                 Sample buffer
 * @param      output_codec_context Codec context of the output file
 * @return Error code (0 if successful)
 */
int init_fifo(AVAudioFifo **fifo, AVCodecContext *output_codec_context);

/**
 * Write the header of the output file container.
 * @param output_format_context Format context of the output file
 * @return Error code (0 if successful)
 */
int write_output_file_header(AVFormatContext *output_format_context);

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
                 int *data_present, int *finished);

/**
 * Decode one audio frame from the input file.
 * @param      frame                Audio frame to be decoded
 * @param      input_format_context Format context of the input file
 * @param      input_codec_context  Codec context of the input file
 * @param[out] data_present         Indicates whether data has been decoded
 * @param[out] finished             Indicates whether the end of file has
 *                                  been reached and all data has been
 *                                  decoded. If this flag is false, there
 *                                  is more data to be decoded, i.e., this
 *                                  function has to be called again.
 * @return Error code (0 if successful)
 */
int decode_audio_frame(AVFrame *frame,
                              AVFormatContext *input_format_context,
                              AVCodecContext *input_audio_codec_context,
                              int *data_present, int *finished, const int video_stream_index,
                              AVFormatContext *output_format_context,
                              AVCodecContext *input_video_codec_context);

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
                                  int frame_size);

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
                           SwrContext *resample_context);

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
                               const int frame_size);

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
                                  int *finished);

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
                             int frame_size);

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
                              AVFormatContext *input_format_context);

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
                                 AVFormatContext *input_format_context);

/**
 * Write the trailer of the output file container.
 * @param output_format_context Format context of the output file
 * @return Error code (0 if successful)
 */
int write_output_file_trailer(AVFormatContext *output_format_context);