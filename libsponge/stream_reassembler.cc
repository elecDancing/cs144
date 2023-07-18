/*
 * @Descripttion:
 * @version:
 * @Author: xp.Zhang
 * @Date: 2023-07-13 11:01:15
 * @LastEditors: xp.Zhang
 * @LastEditTime: 2023-07-17 21:50:53
 */
#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) : _output(capacity), _capacity(capacity) {
    _buffer.resize(capacity);
}

//用于合并两个重叠的报文段
long StreamReassembler::mergeTwoSegment(segment &str1, const segment &str2) {
    segment x, y;
    if (str1.beginIndex > str2.beginIndex) {
        x = str2;
        y = str1;
    } else {
        x = str1;
        y = str2;
    }
    if (x.beginIndex + x.dataSize < y.beginIndex) {  //说明没有重叠
        return -1;
    } else if (x.beginIndex + x.dataSize >=
               y.beginIndex + y.dataSize) {  //说明x不但前面大于y，后面也是大有y的，y为x的子集，x直接将y吃掉
        str1 = x;
        return y.dataSize;  //返回的是由于合并操作减少的长度
    } else {
        str1.beginIndex = x.beginIndex;
        // substr只有一个参数代表选择从这个参数开始向后的所有str
        str1.data = x.data + y.data.substr(x.beginIndex + x.dataSize - y.beginIndex);
        str1.dataSize = str1.data.size();
        int out =  x.beginIndex + x.dataSize - y.beginIndex;
        return out;
    }
}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    // headIndex 是没有写入bytestream中的报文段的头序号
    // case1: capacity over (discard directly) 当前重组器没有能力处理容量之外到达的报文段
    if (index >= _capacity + _head_index) {
        return;
    }
    // case2:
    // case2.1 处理到来的子段放到segment数据结构中
    // 将数据放到一个segment中 包括data ，beginIndex, dataSize
    segment elm;
    // case2.1.1 到来的整个报文段都有序字节流中都已经存在
    if (index + data.size() <= _head_index) {  //表明新来的报文段已经写到了bytestream中了
        //判断新来的报文段是否携带了eof信息（携带eof信息的都发走了，说明eof及之前的段都排列好发走了，说明不会在接受到有用的报文段了）
        goto JUDGE_EOF;
        // if (eof) {
        //     _eof_flag = true;
        // }
        // if (_eof_flag && empty()) {  // 重组器中是空的 _unreassembler == 0
        //     _output.end_input();     //关闭字节流输入，不会再有字节从重组器输入字节流了
        // }
        // return;

    }
    // case2.1.2 到来的整个报文段部分在有序字节流中都已经存在
    else if (index < _head_index) {  // 说明新来的报文段还有一部分需要放到重组器中，另一部分已经发送走了
        // 将迭代器从data.begin() + 偏移量 到结尾的输入字符串传入elm.data 成员变量
        elm.data.assign(_head_index - index + data.begin(), data.end());
        elm.beginIndex = _head_index;
        elm.dataSize = elm.data.size();
        // case2.1.3 到来的整个报文段都不曾传入到有序字节流中
    } else {  // 说明新来的报文确实很新，没有发送到bytestream中
        elm.data = data;
        elm.beginIndex = index;
        elm.dataSize = data.size();
    }
    _unassembled_byte += elm.dataSize;  //更新还没有重组的比特字节数目(char类型是一个字节)

    // case2.2 将处理好的报文段放到我的重组器中(unorded_set segments)
    //解决 交叉 和 重叠 的问题
    do {
        long merged_bytes = 0;  // 设置一个变量用于保存mergeTwoSegment函数返回的重叠的字节数
        // case 2.2.1 查询set中  elm.beginIndex后方交叉和重叠的segments
        auto iter = _segments.lower_bound(elm);  //返回set中第一个beginIndex于elm.beginIndex的元素指针
        while (iter != _segments.end() && (merged_bytes = mergeTwoSegment(elm, *iter)) >= 0) {  //?
            _unassembled_byte -= merged_bytes;
            _segments.erase(iter);
            iter = _segments.lower_bound(elm);
        }
        // case2.2.2 解决beginIndex前方与set中的段的交叉问题；
        if (iter == _segments.begin()) {  //?这里迭代器会指向集合头部么？
            break;
        }
        iter--;
        while ((merged_bytes = mergeTwoSegment(elm, *iter)) >= 0) {  //?為什麼要加=呢，經過debug之后发现merge两个不同的段，这两个段可能没有重叠，返回的merged_bytes当然是0但是肯定有合并操作
            _unassembled_byte -= merged_bytes;
            //!!!!!!!!!!!!!!!
            // auto temp = iter;
            // iter--;
            // _segments.erase(temp);
            // if (iter == _segments.begin()) {
            //     break;
            // }
            _segments.erase(iter);
            iter = _segments.lower_bound(elm);
            if (iter == _segments.begin()) {
                break;
            }
            iter--;
        }

    } while (false);
    _segments.insert(elm);

    //如果含有_headIndex的块合并好了，或者到了，将其传入有序字节流中
    if (!_segments.empty() && _segments.begin()->beginIndex == _head_index) {
        const segment head_segment = *_segments.begin();  //取出头部的segments
        size_t write_bytes = _output.write(head_segment.data);  // output是bytestream类的对象，调用其write成员函数将数据写入,返回的是写入的长度
        _head_index += write_bytes;
        _unassembled_byte -= write_bytes;
        _segments.erase(_segments.begin());
    }

JUDGE_EOF:
    if (eof) {
        _eof_flag = true;
    }
    if (_eof_flag && empty()) {  //重组器中是空的 _unreassembler == 0
        _output.end_input();     //关闭字节流输入，不会再有字节从重组器输入字节流了
    }
    return;
}

size_t StreamReassembler::unassembled_bytes() const { return _unassembled_byte; }

bool StreamReassembler::empty() const { return _unassembled_byte == 0; }
