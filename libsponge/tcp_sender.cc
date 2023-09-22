/*
 * @Descripttion: 
 * @version: 
 * @Author: xp.Zhang
 * @Date: 2023-09-06 10:53:22
 * @LastEditors: xp.Zhang
 * @LastEditTime: 2023-09-21 10:33:13
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
    , _stream(capacity)
    //注意这里_RTO需要初始化
    , _RTO(retx_timeout) {}

// TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
//     : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
//     , _initial_retransmission_timeout{retx_timeout}
//     , _stream(capacity)
//     , _RTO(retx_timeout) {}

uint64_t TCPSender::bytes_in_flight() const { return {_bytes_in_flight}; }

void TCPSender::fill_window(bool send_syn) {
    if(!_syn_flag){
        if(send_syn){
            TCPSegment seg;
            seg.header().syn = true;
            _syn_flag = true;
            send_segment(seg);
            return;
        }
    }
    //接收窗口的大小不能为0(接收窗口包含空闲区域和发出确没有收到确认的段占用的区域)
    size_t window_size = _window_size == 0 ? 1 : _window_size;
    size_t free_space = window_size - _bytes_in_flight;
    while (free_space > 0 && !_fin_flag){
        size_t payloadSize = min(free_space, TCPConfig::MAX_PAYLOAD_SIZE);
        string tempstr = _stream.read(payloadSize);
        //判断一下是否读到了结束
        TCPSegment seg;
        //注意TCPsegment中的payload是一个buffer类型的
        seg.payload() = Buffer(std::move(tempstr));
        //第一个判断条件是为了我现在要在加入一个fin字节，避免溢出
        size_t showsegsize = seg.length_in_sequence_space();
        bool a = _stream.eof();
        if (showsegsize < free_space && a) {
            seg.header().fin = true;
            _fin_flag = true;
        }
        // //
        if(seg.length_in_sequence_space() == 0)
            return;
        
        send_segment(seg);
        free_space = window_size - _bytes_in_flight;
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
//! \returns `false` if the ackno appears invalid (acknowledges something the TCPSender hasn't sent yet)
bool TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    //相对序列号，初始序列号，检查点
    size_t abs_ackno = unwrap(ackno, _isn, _recv_ackno);
    //ackno非法
    if(abs_ackno > _next_seqno){//sender还没发你ack都回来了
        return false;
    }
    _window_size = window_size;//更新一下当前接收窗口的大小
    
    if(abs_ackno <= _recv_ackno){//已经收到过了
        return true;
    }

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
void TCPSender::tick(const size_t ms_since_last_tick) { 
    _timer += ms_since_last_tick; 
    if(_timer >= _RTO && !_segments_outstanding.empty()){
        _segments_out.push(_segments_outstanding.front());
        _RTO = 2 * _RTO;
        _consecutive_retransmission++;
        _timer_running = true;
        _timer = 0;
    }
    if(_segments_outstanding.empty()){
        _timer_running = false;
        _timer = 0;
    }
}

unsigned int TCPSender::consecutive_retransmissions() const {
    return  _consecutive_retransmission;
}

void TCPSender::send_empty_segment() {
     // empty segment doesn't need store to outstanding queue
        TCPSegment tempseg;
        tempseg.header().seqno = wrap(_next_seqno, _isn);
        _segments_out.push(tempseg);
}


void TCPSender::send_segment(TCPSegment &seg) { 
    //给段分配的序列号是相对序列号
    seg.header().seqno = wrap(_next_seqno, _isn);
    _next_seqno += seg.length_in_sequence_space();
    _bytes_in_flight += seg.length_in_sequence_space();
    _segments_out.push(seg);
    _segments_outstanding.push(seg);
    //要是已经启动了呢？
    //if(_timer_running == false){
        _timer_running = true;
        _timer = 0;
    //}
}