/*
 * @Descripttion: 
 * @version: 
 * @Author: xp.Zhang
 * @Date: 2023-07-21 16:22:49
 * @LastEditors: xp.Zhang
 * @LastEditTime: 2023-08-20 19:09:14
 */
#include "wrapping_integers.hh"
#include<iostream>

// Dummy implementation of a 32-bit wrapping integer

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! Transform an "absolute" 64-bit sequence number (zero-indexed) into a WrappingInt32
//! \param n The input absolute 64-bit sequence number
//! \param isn The initial sequence number
WrappingInt32 wrap(uint64_t n, WrappingInt32 isn) {
    // 先取余数在加上初始序列号
    // (（n % 2^32）+ isn) %(2^32);
    WrappingInt32 temp = WrappingInt32(static_cast<uint32_t>((static_cast<uint32_t>(n  & 0x00000000FFFFFFFF) + isn.raw_value())));
    return temp;
}

//! Transform a WrappingInt32 into an "absolute" 64-bit sequence number (zero-indexed)
//! \param n The relative sequence number
//! \param isn The initial sequence number
//! \param checkpoint A recent absolute 64-bit sequence number
//! \returns the 64-bit sequence number that wraps to `n` and is closest to `checkpoint`
//!
//! \note Each of the two streams of the TCP connection has its own ISN. One stream
//! runs from the local TCPSender to the remote TCPReceiver and has one ISN,
//! and the other stream runs from the remote TCPSender to the local TCPReceiver and
//! has a different ISN.
uint64_t unwrap(WrappingInt32 n, WrappingInt32 isn, uint64_t checkpoint) {
    //计算相对序列号n距离初始序列号isn的距离
    uint32_t distance = n.raw_value() - isn.raw_value();
/*     //找到距离checkpoint最近的
    需要提一下的地方有checkpoint表示最近一次转换求得的absolute seqno，而本次转换出的absolute seqno应该选择与上次值最为接近的那一个。\
    原理是虽然segment不一定按序到达，但几乎不可能出现相邻到达的两个segment序号差值超过INT32_MAX的情况，
    除非延迟以年为单位，或者产生了比特差错（后面的LAB可能涉及）。
    实际操作就是把算出来的 绝对序号分别加减1ul << 32做比较，选择与checkpoing差的绝对值最小的那个。 */

    //checkpoint 取得前32位, 其实就找到checkpoint所在的区间下界
    uint64_t t = (checkpoint & 0xFFFFFFFF00000000) + distance;
    //明显结果有三种可能，一种是temp = t 也就是result就在checkpoint所在区间
    //temp = temp + (1ul << 32) result 在checkpoint上面的区间
    //temp = temp - (1ul << 32) result 在checkpoint下面那个区间
    //选择的方式就是看 哪个的距离更近
    uint64_t temp = t;
    if(abs(int64_t(t - checkpoint)) > abs(int64_t(t + (1ul << 32) - checkpoint))){
        temp = t + (1ul << 32);
    }
    if((t >= (1ul<<32)) && (abs(int64_t(t - checkpoint)) > abs(int64_t(t - (1ul << 32) - checkpoint)))){
        temp = t - (1ul << 32);
    }
    return temp; 
}
