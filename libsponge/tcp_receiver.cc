/*
 * @Descripttion: 
 * @version: 
 * @Author: xp.Zhang
 * @Date: 2023-07-21 16:22:49
 * @LastEditors: xp.Zhang
 * @LastEditTime: 2023-09-28 12:05:27
 */
#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;
//实现一个基于滑动窗口的TCP接收端
bool TCPReceiver::segment_received(const TCPSegment &seg) {
    //reassembler 中使用的都是绝对序列号
    //ackno 为相对序列号，但是为了判断在resssmenbler中的边界条件，需要定义abs_ackno
    uint64_t abs_seqno = 0;
    uint64_t abs_ackno = 0;
    size_t length = 0;
    // 第一个segment
    if (seg.header().syn) {  // 如果是第一个segment，进行处理
        if(_syn_flag){
            return false; //重复收到两个syn ，refuse
        }
        _syn_flag = true;
        _isn = seg.header().seqno;
        //data index eof
        //查看segment的长度（length_in_sequence_space包含playload ，syn ，fin 的总长度）
        size_t PyloadLength = seg.length_in_sequence_space() - 1;
        abs_seqno = 1;
        _ackno = ackno();
        if (PyloadLength == 0) {  //! segment's content only have a SYN flag
            return true;
        }
    }
    else if (!_syn_flag)  // 在synFlag为真之前拒绝所有段
        return false;
    //后续segment
    else{ //synFlag为真后到来的代码段
        abs_seqno = unwrap(seg.header().seqno, _isn.value(), _reassembler.head_index());
        abs_ackno = unwrap(_ackno.value(), _isn.value(), _reassembler.head_index());
        length = seg.length_in_sequence_space();
    }
    
    /* 检查当前段的绝对序列号是否在接收窗口之外。如果绝对序列号大于等于基准序列号加上窗口大小，
    或者绝对序列号加上段的长度小于等于基准序列号，那么当前段与接收窗口没有重叠，
    应被丢弃。如果此时ret标志为false，说明之前没有成功接收到过段，因此函数返回false表示接收失败。 */
    //!超出接收窗口检查，重复报文检查
    if(abs_seqno >= abs_ackno + window_size()){
        return false;
    }
    if (abs_seqno + length <= abs_ackno){
        //如果收到的段不携带负载的话absseqno 和 abs_ackno相等没什么问题
        if(length != 0)
            return false;
    }
        //!报文截止检测
        if (seg.header().fin) {
            if (_fin_flag) {  // already get a FIN, refuse other FIN
                return false;
            }
            _fin_flag = true;
        }
    //!空报文段检查
    if(seg.length_in_sequence_space() == 0 && abs_seqno == abs_ackno){
        return true;
    }
    // 由于syn占用了一个字节，在初始的segment中，由于syn的存在，接收端接收到的相对序列号其实加了1
    // 同理，当更新ackno的时候，重组器头部的绝对序列号要+1 才能去解包到相对序列号
    _reassembler.push_substring(seg.payload().copy(), abs_seqno - 1, seg.header().fin);
    _ackno = ackno();
    return true;
}

optional<WrappingInt32> TCPReceiver::ackno() const { 
    if(_syn_flag){
        optional<WrappingInt32> temp = wrap(_reassembler.head_index() + 1, _isn.value());
        if(_fin_flag)
            temp = wrap(_reassembler.head_index() + 2, _isn.value());
        return temp;
    }
    else
        return std::nullopt;
}
//容量 - 重组器输出到字节流中的有序缓冲字节数目
size_t TCPReceiver::window_size() const { return {_capacity - _reassembler.stream_out().buffer_size()}; }
