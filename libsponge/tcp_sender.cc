/*
 * @Descripttion: 
 * @version: 
 * @Author: xp.Zhang
 * @Date: 2023-09-06 10:53:22
 * @LastEditors: xp.Zhang
 * @LastEditTime: 2023-09-06 22:06:46
 */
#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity) {}

uint64_t TCPSender::bytes_in_flight() const { return {_bytes_in_flight}; }

void TCPSender::fill_window() {}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
//! \returns `false` if the ackno appears invalid (acknowledges something the TCPSender hasn't sent yet)
bool TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    //相对序列号，初始序列号，检查点
    size_t abs_ackno = unwrap(ackno, _isn, _recv_ackno);
    //ackno非法
    if(abs_ackno >= _next_seqno){//sender还没发你ack都回来了
        return false;
    }
    if(abs_ackno < _recv_ackno){//已经收到过了
        return true;
    }
    uint64_t rwnd = window_size;//保存一下当前接收窗口的大小
    _recv_ackno = abs_ackno; //更新，在abs_ackno之前的所有字节都收到了
    //更新未传输完成的队列
    while(!_segments_outstanding.empty()){
        TCPSegment tempseg = _segments_outstanding.front();
        //segment 的序列号是相对序列号
        size_t abseqno_tempseg = unwrap(tempseg.header().seqno, _isn, _recv_ackno);
        if (abseqno_tempseg + tempseg.length_in_sequence_space() <= abs_ackno){
            _bytes_in_flight -= tempseg.length_in_sequence_space();
            _segments_outstanding.pop();
        }
        else
            break;
    }
    //参照当前rwnd继续send
    fill_window();
    _RTO = _initial_retransmission_timeout;
    _consecutive_retransmission = 0;
    //还有没有收到ack的段，那么就要启动重传计时器了
    if(!_segments_outstanding.empty()){
        _timer_running = true;
        _timer = 0;
    }
    return true;
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) { DUMMY_CODE(ms_since_last_tick); }

unsigned int TCPSender::consecutive_retransmissions() const { return {}; }

void TCPSender::send_empty_segment() {}
