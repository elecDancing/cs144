/*
 * @Descripttion: 
 * @version: 
 * @Author: xp.Zhang
 * @Date: 2023-09-15 17:16:14
 * @LastEditors: xp.Zhang
 * @LastEditTime: 2023-09-27 16:14:39
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

//每次segment_reveived的时候_time_since_last_segment_received都会归零
size_t TCPConnection::time_since_last_segment_received() const { return _time_since_last_segment_received; }

void TCPConnection::segment_received(const TCPSegment &seg) { 
    if(!_active)
        return;
    _time_since_last_segment_received = 0;
    //在syn_sent状态下，收到不是回复syn的ack段，一律忽略
    if (in_syn_sent() && seg.header().ack && seg.payload().size() > 0){
        return;
    }
    // 如果段中包含rst标志
    if (seg.header().rst) {
        // TODO RST segments without ACKs should be ignored in SYN_SENT
        //  有ack的话是对syn的回复，不能忽略
        if (in_syn_sent() && !seg.header().ack) {
            return;
        }
        unclean_shutdown(false);
        return;
    }
    //设置一个发送空段标志位
    bool send_empty = false;
    //--keep alive机制
    //对面终端发送一个不合法的ackno，用来测试TCP连接是否alive，需要给这些segments回复
    //或者说只是单纯的收到了一个不合法的ackno
    //本地已经开始发送，并且当前段中含有ack
    if(_sender.next_seqno_absolute() > 0 && seg.header().ack){
        //使用本地发送端的ack_received函数，返回的是ack是否合法
        if(!_sender.ack_received(seg.header().ackno, seg.header().win)){
            send_empty = true; 
        }
    }
    //判断本地接收端是否正常接收这个段
    bool recv_flag = false;
    recv_flag = _receiver.segment_received(seg);
    if(!recv_flag){
        send_empty = true;
    }

    // 远端传来syn
    if(seg.header().syn && _sender.next_seqno_absolute() == 0){
        connect();
        return;
    }

    if(seg.payload().size() > 0){
        send_empty = true;
    }

    if(send_empty){
        //如果reveiver已经开始接收远端文件，但是sender的发送队列当前为空，就需要发送一个空段，
        //用于确认当前段收到（也就是当前没有可以加上负载的段用于确认，所以只能勉为其难的发一个空段用于确认接收端收到的远端数据）
        if(_receiver.ackno().has_value() && _sender.segments_out().empty()){
            _sender.send_empty_segment();
        }
    }

    push_segments_out();
}

bool TCPConnection::active() const { return {_active}; }

size_t TCPConnection::write(const string &data) { 
    //使用write函数写入data，函数内部考虑了缓冲区的大小
    //放入sender流
    size_t writeLen = _sender.stream_in().write(data);
    //放入connection流 
    push_segments_out();
    return writeLen;
 }

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) { 
    if(!_active)
        return;
    _time_since_last_segment_received += ms_since_last_tick;
    _sender.tick(ms_since_last_tick);
    if(_sender.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS){
        unclean_shutdown(true);
    }
    push_segments_out();
}

void TCPConnection::end_input_stream() { 
    _sender.stream_in().end_input();
    push_segments_out();
}

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
    clean_shutdown();//每次将segments放入connection的队列中的时候，都调用该函数判断一下是否要shutdown了
    return true;
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            unclean_shutdown(true);
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

bool TCPConnection::unclean_shutdown(bool sent_rst) { 
    //将出入数据流都设置在error_state
    _sender.stream_in().set_error();
    _receiver.stream_out().set_error();
    //将tcpconnection活跃状态标志关闭
    _active = false;
    //如果本地TCPConnection主动发出rst
    if(sent_rst){
        _need_send_rst = true;
        if(_sender.segments_out().empty()){
            _sender.send_empty_segment();
        }
        push_segments_out();
    }
}

bool TCPConnection::in_listen(){ 
    return (!_receiver.ackno().has_value() && _sender.next_seqno_absolute() == 0); 
}
bool TCPConnection::in_syn_sent(){
    //return (!_receiver.ackno().has_value() && _sender.next_seqno_absolute() > 0 &&
            //_sender.next_seqno_absolute() == bytes_in_flight());
    return (_sender.next_seqno_absolute() > 0 && _sender.next_seqno_absolute() == bytes_in_flight());
}
bool TCPConnection::in_syn_recv(){
    if(_receiver.ackno().has_value() && !_receiver.stream_out().input_ended())
        return true;
    else
        return false;
}