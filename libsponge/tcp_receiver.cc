/*
 * @Descripttion: 
 * @version: 
 * @Author: xp.Zhang
 * @Date: 2023-07-21 16:22:49
 * @LastEditors: xp.Zhang
 * @LastEditTime: 2023-08-22 17:16:35
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
    if(seg.header().syn){//如果是第一个segment，进行处理
        if(_syn_flag){
            return false; //重复收到两个syn ，refuse
        }
        _syn_flag = true;
        _isn = seg.header().seqno;
        //data index eof
        //查看segment的长度（length_in_sequence_space包含playload ，syn ，fin 的总长度）
        size_t PyloadLength = seg.length_in_sequence_space() - 1;

        if (Pyloadlength == 0) {  //! segment's content only have a SYN flag
            return true;
        }

        // //?为什么用了copy之后 才会变成string类型？
        // _reassembler.push_substring(seg.payload().copy(), 0, seg.header().fin);
        // if(seg.header().fin){
        //     _fin_flag = true;
        // }
        // //!注意重组器中是absolute seqno
        // //!更新ackno
        // _ackno = ackno();
        // return true;
    }
    else if(!_syn_flag) //在synFlag为真之前拒绝所有段
        return false;
    else{ //synFlag为真后到来的代码段
        uint64_t abs_seqno = unwrap(seg.header().seqno, _isn.value(), _reassembler.head_index());
        size_t length = seg.length_insequence_space();
    }
    
    if (seg.header().fin) {
        if (_fin_flag) {  // already get a FIN, refuse other FIN
            return false;
        }
        _fin_flag = true;
        ret = true;
    }
    uint64_t abs_seqno = unwrap(seg.header().seqno, _isn.value(), _reassembler.head_index());
    // if(WrappingInt32(seg.header().seqno- 1)+ WrappingInt32(seg.payload().size())< int32_t(_ackno.value().raw_value()))
    //     return false;
    // 由于syn占用了一个字节，在初始的segment中，由于syn的存在，接收端接收到的相对序列号其实加了1
    // 同理，当更新ackno的时候，重组器头部的绝对序列号要+1 才能去解包到相对序列号
    _reassembler.push_substring(seg.payload().copy(), abs_seqno - 1, seg.header().fin);
        if(seg.header().fin){
                _fin_flag = true;
            }
    WrappingInt32 tempAckno = _ackno.value();
    _ackno = ackno();
    if(seg.header().seqno - 1 <= _ackno.value() && tempAckno >= _ackno.value())
                return false;
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
