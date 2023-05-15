/*
 * http://ffmpeg.org/doxygen/trunk/index.html
 *
 * Main components
 *
 * Format (Container) - a wrapper, providing sync, metadata and muxing for the streams.
 * Stream - a continuous stream (audio or video) of data over time.
 * Codec - defines how data are enCOded (from Frame to Packet)
 *        and DECoded (from Packet to Frame).
 * Packet - are the data (kind of slices of the stream data) to be decoded as raw frames.
 * Frame - a decoded raw frame (to be encoded or filtered).
 */

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

// print out the steps and errors
static void logging(const char *fmt, ...);
// decode packets into frames
static int decode_packet(AVPacket *pPacket, AVCodecContext *pCodecContext, AVFrame *pFrame);
// save a frame into a .pgm file
static void save_gray_frame(unsigned char *buf, int wrap, int xsize, int ysize, char *filename);

int main(int argc, const char *argv[])
{
  if (argc < 2) {
    printf("You need to specify a media file.\n");
    return -1;
  }
  
  logging("initializing all the containers, codecs and protocols.");

  // AVFormatContext holds the header information from the format (Container)
  // Allocating memory for this component
  // http://ffmpeg.org/doxygen/trunk/structAVFormatContext.html
  AVFormatContext *pFormatContext = avformat_alloc_context();
  if (!pFormatContext) {
    logging("ERROR could not allocate memory for Format Context");
    return -1;
  }

  logging("opening the input file (%s) and loading format (container) header", argv[1]);
  // Open the file and read its header. The codecs are not opened.
  // The function arguments are:
  // AVFormatContext (the component we allocated memory for),
  // url (filename),
  // AVInputFormat (if you pass NULL it'll do the auto detect)
  // and AVDictionary (which are options to the demuxer)
  // http://ffmpeg.org/doxygen/trunk/group__lavf__decoding.html#ga31d601155e9035d5b0e7efedc894ee49
  if (avformat_open_input(&pFormatContext, argv[1], NULL, NULL) != 0) 
  {
    logging("ERROR could not open the file");
    return -1;
  }

  // now we have access to some information about our file
  // since we read its header we can say what format (container) it's
  // and some other information related to the format itself.
  logging("format %s, duration %lld us, bit_rate %lld", pFormatContext->iformat->name, pFormatContext->duration, pFormatContext->bit_rate);

  logging("finding stream info from format");
  // read Packets from the Format to get stream information
  // this function populates pFormatContext->streams
  // (of size equals to pFormatContext->nb_streams)
  // the arguments are:
  // the AVFormatContext
  // and options contains options for codec corresponding to i-th stream.
  // On return each dictionary will be filled with options that were not found.
  // https://ffmpeg.org/doxygen/trunk/group__lavf__decoding.html#gad42172e27cddafb81096939783b157bb
  if (avformat_find_stream_info(pFormatContext,  NULL) < 0) {
    logging("ERROR could not get the stream info");
    return -1;
  }

  // the component that knows how to enCOde and DECode the stream
  // it's the codec (audio or video)
  // http://ffmpeg.org/doxygen/trunk/structAVCodec.html
  AVCodec *pCodec = NULL;
  // this component describes the properties of a codec used by the stream i
  // https://ffmpeg.org/doxygen/trunk/structAVCodecParameters.html
  AVCodecParameters *pCodecParameters =  NULL;
  int video_stream_index = -1;

  // loop though all the streams and print its main information
  for (int i = 0; i < pFormatContext->nb_streams; i++)
  {
    AVCodecParameters *pLocalCodecParameters =  NULL;
    pLocalCodecParameters = pFormatContext->streams[i]->codecpar;
    logging("AVStream->time_base before open coded %d/%d", pFormatContext->streams[i]->time_base.num, pFormatContext->streams[i]->time_base.den);
    logging("AVStream->r_frame_rate before open coded %d/%d", pFormatContext->streams[i]->r_frame_rate.num, pFormatContext->streams[i]->r_frame_rate.den);
    logging("AVStream->start_time %" PRId64, pFormatContext->streams[i]->start_time);
    logging("AVStream->duration %" PRId64, pFormatContext->streams[i]->duration);

    logging("finding the proper decoder (CODEC)");

    AVCodec *pLocalCodec = NULL;

    // finds the registered decoder for a codec ID
    // https://ffmpeg.org/doxygen/trunk/group__lavc__decoding.html#ga19a0ca553277f019dd5b0fec6e1f9dca
    //寻找解码器
    pLocalCodec = avcodec_find_decoder(pLocalCodecParameters->codec_id);

    if (pLocalCodec==NULL) {
      logging("ERROR unsupported codec!");
      // In this example if the codec is not found we just skip it
      continue;
    }

    // when the stream is a video we store its index, codec parameters and codec
    if (pLocalCodecParameters->codec_type == AVMEDIA_TYPE_VIDEO)//视频 
    {
      if (video_stream_index == -1) 
      {
        video_stream_index = i;//视频流索引
        pCodec = pLocalCodec;//解码器
        pCodecParameters = pLocalCodecParameters;//编解码器参数
      }

      logging("Video Codec: resolution %d x %d", pLocalCodecParameters->width, pLocalCodecParameters->height);
    } 
    else if (pLocalCodecParameters->codec_type == AVMEDIA_TYPE_AUDIO) 
    {
      logging("Audio Codec: %d channels, sample rate %d", pLocalCodecParameters->channels, pLocalCodecParameters->sample_rate);
    }

    // print its name, id and bitrate
    logging("\tCodec %s ID %d bit_rate %lld", pLocalCodec->name, pLocalCodec->id, pLocalCodecParameters->bit_rate);
  }

  if (video_stream_index == -1) 
  {
    logging("File %s does not contain a video stream!", argv[1]);
    return -1;
  }

  // https://ffmpeg.org/doxygen/trunk/structAVCodecContext.html
  AVCodecContext *pCodecContext = avcodec_alloc_context3(pCodec);
  if (!pCodecContext)
  {
    logging("failed to allocated memory for AVCodecContext");
    return -1;
  }

  // Fill the codec context based on the values from the supplied codec parameters
  // https://ffmpeg.org/doxygen/trunk/group__lavc__core.html#gac7b282f51540ca7a99416a3ba6ee0d16
  //用编解码器参数填充编解码上下文
  if (avcodec_parameters_to_context(pCodecContext, pCodecParameters) < 0)
  {
    logging("failed to copy codec params to codec context");
    return -1;
  }

  // Initialize the AVCodecContext to use the given AVCodec.
  // https://ffmpeg.org/doxygen/trunk/group__lavc__core.html#ga11f785a188d7d9df71621001465b0f1d
  //打开解码器
  if (avcodec_open2(pCodecContext, pCodec, NULL) < 0)
  {
    logging("failed to open codec through avcodec_open2");
    return -1;
  }

  // https://ffmpeg.org/doxygen/trunk/structAVFrame.html
  AVFrame *pFrame = av_frame_alloc();//解码后的帧
  if (!pFrame)
  {
    logging("failed to allocate memory for AVFrame");
    return -1;
  }
  // https://ffmpeg.org/doxygen/trunk/structAVPacket.html
  AVPacket *pPacket = av_packet_alloc();//压缩帧
  if (!pPacket)
  {
    logging("failed to allocate memory for AVPacket");
    return -1;
  }

  int response = 0;
  int how_many_packets_to_process = 8;

  // fill the Packet with data from the Stream
  // https://ffmpeg.org/doxygen/trunk/group__lavf__decoding.html#ga4fdb3084415a82e3810de6ee60e46a61
  while (av_read_frame(pFormatContext, pPacket) >= 0)//读取一包数据
  {
    // if it's the video stream
    if (pPacket->stream_index == video_stream_index)//视频索引 
    {
    logging("AVPacket->pts %" PRId64, pPacket->pts);
      response = decode_packet(pPacket, pCodecContext, pFrame);//解码保存pgm图
      if (response < 0)
        break;
      // stop it, otherwise we'll be saving hundreds of frames
      if (--how_many_packets_to_process <= 0) break;
    }
    // https://ffmpeg.org/doxygen/trunk/group__lavc__packet.html#ga63d5a489b419bd5d45cfd09091cbcbc2
    av_packet_unref(pPacket);
  }

  logging("releasing all the resources");

  avformat_close_input(&pFormatContext);
  av_packet_free(&pPacket);
  av_frame_free(&pFrame);
  avcodec_free_context(&pCodecContext);
  return 0;
}

static void logging(const char *fmt, ...)
{
    va_list args;
    fprintf( stderr, "LOG: " );
    va_start( args, fmt );
    vfprintf( stderr, fmt, args );
    va_end( args );
    fprintf( stderr, "\n" );
}

static int decode_packet(AVPacket *pPacket, AVCodecContext *pCodecContext, AVFrame *pFrame)
{
  // Supply raw packet data as input to a decoder
  // https://ffmpeg.org/doxygen/trunk/group__lavc__decoding.html#ga58bc4bf1e0ac59e27362597e467efff3
  //*********************解码开始****************************
  
   /*
  0：正常返回，意味着送入的 packet 被解码器正常接收。
  AVERROR (EAGAIN)：由于解码器内部缓存已满，送入的 packet 未被接收，需要 avcodec_receive_frame () 读取掉一些已经解码的音视频帧后，才能继续送入。
  AVERROR (EOF)：当 send_packet 送入为 NULL 时才会触发该状态，通知解码器输入 packet 已结束，后续不再送入 packet。
  AVERROR (EINVAL)：解码器没有打开，或者这是一个编码器，或者要求刷新。
  AVERROR (ENOMEM)：通常是内存不足。
*/
  int response = avcodec_send_packet(pCodecContext, pPacket);

  if (response < 0) {
    logging("Error while sending a packet to the decoder: %s", av_err2str(response));
    return response;
  }

  while (response >= 0)
  {
    // Return decoded output data (into a frame) from a decoder
    // https://ffmpeg.org/doxygen/trunk/group__lavc__decoding.html#ga11e6542c4e66d3028668788a1a74217c
    //解码数据保存在pFrame
    /*
    0：正常返回，意味着输出一帧解码后的音频/视频帧，frame 中有解码后的帧数据填充。
    AVERROR (EAGAIN)：由于解码器内部已空，没有音视频帧解码输出，frame 中没有填充，需要 avcodec_send_packet () 继续送入数据包以便后续继续解码。
    AVERROR (EOF)：当 send_packet 送入为 NULL 时才会触发该状态，通知解码器输入 packet 已结束，后续不再送入 packet。此时需要调用 avcodec_flush_buffers () 来复位重置解码器。
    AVERROR (EINVAL)：解码器没有打开。
    */
    response = avcodec_receive_frame(pCodecContext, pFrame);
    if (response == AVERROR(EAGAIN) || response == AVERROR_EOF) 
    {
      break;
    } else if (response < 0) 
    {
      logging("Error while receiving a frame from the decoder: %s", av_err2str(response));
      return response;
    }

    if (response >= 0) 
    {
      logging(
          "Frame %d (type=%c, size=%d bytes, format=%d) pts %d key_frame %d [DTS %d]",
          pCodecContext->frame_number,
          av_get_picture_type_char(pFrame->pict_type),//I,P,B帧
          pFrame->pkt_size,
          pFrame->format,//视频帧或音频帧的格式，对应 AVPixelFormat 或 AVSampleFormat 枚举值
          pFrame->pts,//表示这一帧的显示时间戳（presentation timestamp），单位由时间基决定。
          pFrame->key_frame,
          pFrame->coded_picture_number
      );

      char frame_filename[1024];//输出文件的名字
      snprintf(frame_filename, sizeof(frame_filename), "%s-%d.pgm", "frame", pCodecContext->frame_number);
      // Check if the frame is a planar YUV 4:2:0, 12bpp
      // That is the format of the provided .mp4 file
      // RGB formats will definitely not give a gray image
      // Other YUV image may do so, but untested, so give a warning
      if (pFrame->format != AV_PIX_FMT_YUV420P)
      {
        logging("Warning: the generated file may not be a grayscale image, but could e.g. be just the R component if the video format is RGB");
      }
      // save a grayscale frame into a .pgm file
      save_gray_frame(pFrame->data[0], pFrame->linesize[0], pFrame->width, pFrame->height, frame_filename);
    }
  }
  return 0;
}

static void save_gray_frame(unsigned char *buf, int wrap, int xsize, int ysize, char *filename)
{
    FILE *f;
    int i;
    f = fopen(filename,"w");
    // writing the minimal required header for a pgm file format
    // portable graymap format -> https://en.wikipedia.org/wiki/Netpbm_format#PGM_example
  /*
  PGM图像是一种灰度图像格式，每个像素表示一个灰度级别，从黑色到白色。
  PGM图像有两种类型：P2和P5，分别对应ASCII和二进制格式。
  PGM图像由文件头和数据部分组成，文件头包含了图像的类型、宽度、高度和色彩范围等信息，数据部分包含了图像的像素值。
  P2类型的PGM图像，每个像素使用字符串来表示，每个像素数据之间需要用一个空格符分开存储，在图像的每一行数据结束时需要换行。当数据超过70个字节的时候，会自动换行。
  P5类型的PGM图像，每个像素用二进制表示，每个像素数据之间没有间隔的连续存储。在记事本中打开后看到的将会是乱码。
*/
    fprintf(f, "P5\n%d %d\n%d\n", xsize, ysize, 255);//p2和p5分别表示ascall和二进制格式

    // writing line by line
    for (i = 0; i < ysize; i++)
        fwrite(buf + i * wrap, 1, xsize, f);
    fclose(f);
}
