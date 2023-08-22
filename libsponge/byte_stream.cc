/*
 * @Descripttion:
 * @version:
 * @Author: xp.Zhang
 * @Date: 2023-07-12 15:22:53
 * @LastEditors: xp.Zhang
 * @LastEditTime: 2023-08-22 21:28:57
 */
#include "byte_stream.hh"

#include <algorithm>
#include <iterator>
#include <stdexcept>

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;
//构造函数使用了成员初始化变量_capacity，避免了在函数体中对成员变量进行赋值的额外开销，同时更加清晰的表达了成员变量的初始化过程
ByteStream::ByteStream(const size_t capacity) : _capacity(capacity) {}

size_t ByteStream::write(const string &data) {
    size_t len = data.size();
    if (len > _capacity - _buffer.size()) {  //写入数据长度大于缓冲区剩余容量
        len = _capacity - _buffer.size();
    }
    _writeCount += len;
    for (size_t i = 0; i < len; ++i) {
        _buffer.push_back(data[i]);
    }
    return len;
}

//! \param[in] len bytes will be copied from the output side of the buffer
// peek_output函数使用了const关键字修饰，因此它是一个常量成员函数。这意味着在调用该函数时，它不能修改ByteStream对象的状态，即不能修改任何成员变量的值
//查看队列前端len个元素
string ByteStream::peek_output(const size_t len) const {
    size_t length = len;
    if (length > _buffer.size())
        length = _buffer.size();
    string tempstr;
    for (auto it = _buffer.begin(); it < _buffer.begin() + len; ++it) {
        tempstr += *it;
    }
    return tempstr;
}

//! \param[in] len bytes will be removed from the output side of the buffer
//
void ByteStream::pop_output(const size_t len) {
    if (!_buffer.empty()) {
        size_t length = len;
        if (length > _buffer.size()) {
            length = _buffer.size();
        }
        _readCount += length;
        for (size_t i = 0; i < len; ++i) {
            _buffer.pop_front();
        }
        return;
    }
    return;
}

void ByteStream::end_input() { _endInput_flag = true; }

bool ByteStream::input_ended() const { return {_endInput_flag}; }

size_t ByteStream::buffer_size() const { return {_buffer.size()}; }

bool ByteStream::buffer_empty() const { return {_buffer.empty()}; }

bool ByteStream::eof() const {
    if (buffer_empty() && input_ended())
        return true;
    else
        return false;
}

size_t ByteStream::bytes_written() const { return {_writeCount}; }

size_t ByteStream::bytes_read() const { return {_readCount}; }

size_t ByteStream::remaining_capacity() const { return {_capacity - buffer_size()}; }
