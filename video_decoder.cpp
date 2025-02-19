#include "video_decoder.h"
#include <iostream>
#include <string>
#include "ffmpeg.h"

VideoDecoder::VideoDecoder(const std::string& decoder_name, AVCodecParameters* codec_parameters) : next_pts_(AV_NOPTS_VALUE) {
  if (decoder_name.empty()) {
    codec_ = avcodec_find_decoder(codec_parameters->codec_id);
  } else {
    codec_ = avcodec_find_decoder_by_name(decoder_name.c_str());
  }

  if (codec_ == nullptr) {
    throw ffmpeg::Error{"Unsupported video codec"};
  }
  codec_context_ = avcodec_alloc_context3(codec_);
  if (codec_context_ == nullptr) {
    throw ffmpeg::Error{"Couldn't allocate video codec context"};
  }
  ffmpeg::check(avcodec_parameters_to_context(codec_context_, codec_parameters));
  ffmpeg::check(avcodec_open2(codec_context_, codec_, nullptr));
}

VideoDecoder::~VideoDecoder() {
  avcodec_free_context(&codec_context_);
}

bool VideoDecoder::send(AVPacket* packet) {
  auto ret = avcodec_send_packet(codec_context_, packet);
  if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
    return false;
  }
  ffmpeg::check(ret);
  return true;
}

bool VideoDecoder::receive(AVFrame* frame) {
  auto ret = avcodec_receive_frame(codec_context_, frame);
  if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
    return false;
  }
  ffmpeg::check(ret);

  // use an increasing timestamp via pkt_duration between keyframes; otherwise, fall back to the best effort timestamp when PTS is not available
  frame->pts = (next_pts_ == AV_NOPTS_VALUE || frame->key_frame) ? (frame->pts != AV_NOPTS_VALUE ? frame->pts : frame->best_effort_timestamp) : next_pts_;

  // save next PTS
  next_pts_ = frame->pts + frame->pkt_duration;

  return true;
}

void VideoDecoder::flush() {
  avcodec_flush_buffers(codec_context_);
}

unsigned VideoDecoder::width() const {
  return codec_context_->width;
}

unsigned VideoDecoder::height() const {
  return codec_context_->height;
}

AVPixelFormat VideoDecoder::pixel_format() const {
  return codec_context_->pix_fmt;
}

AVRational VideoDecoder::time_base() const {
  return codec_context_->time_base;
}

const AVCodec* VideoDecoder::codec() const {
  return codec_;
}

AVCodecContext* VideoDecoder::codec_context() const {
  return codec_context_;
}

int64_t VideoDecoder::next_pts() const {
  return next_pts_;
}
