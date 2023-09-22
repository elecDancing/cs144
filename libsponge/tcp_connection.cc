/*
 * @Descripttion: 
 * @version: 
 * @Author: xp.Zhang
 * @Date: 2023-09-15 17:16:14
 * @LastEditors: xp.Zhang
 * @LastEditTime: 2023-09-21 16:28:37
 */
#include "tcp_connection.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;
//其实就是查看sender的bytestream还有多少容量
size_t TCPConnection::remaining_outbound_capacity() const { return _sender.stream_in().remaining_capacity(); }

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return {}; }

void TCPConnection::segment_received(const TCPSegment &seg) { DUMMY_CODE(seg); }

bool TCPConnection::active() const { return {}; }

size_t TCPConnection::write(const string &data) {
    DUMMY_CODE(data);
    return {};
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) { DUMMY_CODE(ms_since_last_tick); }

void TCPConnection::end_input_stream() {}

//发送SYN段初始化连接
void TCPConnection::connect() { 
    push_segments_out(true); 
    }

bool TCPConnection::push_segments_out(bool send_syn) { 
    //在收到peer 发送的SYN之前 本地不发送syn
    _sender.fill_window(send_syn || in_syn_recv()); 
    TCPSegment seg;
    //将sender队列中的seg放入connection队列
    while(!_sender.segments_out().empty()){
        seg = _sender.segments_out().front();
        _sender.segments_out().pop();
        //查看远端是否正在向本地发送数据
        if(_receiver.ackno().has_value()){
            seg.header().ack = true;
            seg.header().ackno = _receiver.ackno().value();
            seg.header().win = _receiver.window_size();
        }
        //判断是否要重置
        if(_need_send_rst){
            _need_send_rst = false;
            seg.header().rst = true;
        }
        //放入connection的发送队列
        _segments_out.push(seg);
    }
    clean_shutdown();
    return true;
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            // Your code here: need to send a RST segment to the peer
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}

bool TCPConnection::clean_shutdown(){
    //_linger_after_streams_finish为真的时候本地需要设置time_wait
    //有两种可能，一是本地先接收完远端数据，但是本地还在向远端发送
    //另一种是，本地先结束向远端的发送，但是远端还在向本地发送数据，这时候本地会有一个TIME_Wait状态
    //第一种情况，本地接收端已经接收完了，但是本地发送端还在发送 那么本地将不会进入time_wait状态，远端会有time_wait状态
    if(_receiver.stream_out().input_ended() && !_sender.stream_in().input_ended()){
        _linger_after_streams_finish = false; //那么结束的时候本地不需要进入time_wait状态
    }
    //另一种情况就不用判断了，_linger_after_streams_finish默认是设置为真的
    //这里处理的是远端和本地都结束的时候
    if(_sender.stream_in().input_ended() && _sender.bytes_in_flight() == 0 && _receiver.stream_out().input_ended()){
        //前提，本地发送端和接收端都停止工作
        //不需要time_wait 那么active设置为false
        //或者需要time_wait, 但是当前距离上次收到segment已经过去了规定时间，active 设置为false；
        if(_linger_after_streams_finish == false || time_since_last_segment_received() >= 10 * _cfg.rt_timeout){
            _active = false;
        }
    }
    return !_active;
}

bool TCPConnection::in_syn_recv(){
    if(_receiver.ackno().has_value() && !_receiver.stream_out().input_ended())
        return true;
    else
        return false;
}