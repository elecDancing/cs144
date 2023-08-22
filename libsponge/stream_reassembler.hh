/*
 * @Descripttion:
 * @version:
 * @Author: xp.Zhang
 * @Date: 2023-07-13 11:01:15
 * @LastEditors: xp.Zhang
 * @LastEditTime: 2023-08-20 21:39:11
 */
#ifndef SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH
#define SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH

#include "byte_stream.hh"

#include <cstdint>
#include <set>
#include <string>
#include <vector>

//! \brief A class that assembles a series of excerpts from a byte stream (possibly out of order,
//! possibly overlapping) into an in-order byte stream.
class StreamReassembler {
  private:
    // Your code here -- add private members as necessary.
    struct segment {
        size_t beginIndex = 0;
        size_t dataSize = 0;
        std::string data = "";
        //重载了小于号运算符 目的是后面std::set排序的时候使用的是报文段的index
        bool operator<(const segment t) const { return beginIndex < t.beginIndex; }
    };

    std::set<segment> _segments = {};
    std::vector<char> _buffer = {};

    size_t _unassembled_byte = 0;
    size_t _head_index = 0;
    bool _eof_flag = false;

    ByteStream _output;  //!< The reassembled in-order byte stream
    size_t _capacity;    //!< The maximum number of bytes

  public:
  // 返回重组器的头部序号
    size_t head_index() const { return _head_index; }
    //字节流不再输入重组器件的标志
    bool input_ended() const { return _output.input_ended(); }
    //! \brief Construct a `StreamReassembler` that will store up to `capacity` bytes.
    //! \note This capacity limits both the bytes that have been reassembled,
    //! and those that have not yet been reassembled.
    StreamReassembler(const size_t capacity);

    //! \brief Receives a substring and writes any newly contiguous bytes into the stream.
    //!
    //! If accepting all the data would overflow the `capacity` of this
    //! `StreamReassembler`, then only the part of the data that fits will be
    //! accepted. If the substring is only partially accepted, then the `eof`
    //! will be disregarded.
    //!
    //! \param data the string being added
    //! \param index the index of the first byte in `data`
    //! \param eof whether or not this segment ends with the end of the stream
    void push_substring(const std::string &data, const uint64_t index, const bool eof);

    //这段代码用于合并有重叠的两个报文段
    long mergeTwoSegment(segment &str1, const segment &str2);

    //! \name Access the reassembled byte stream
    //!@{
    const ByteStream &stream_out() const { return _output; }
    ByteStream &stream_out() { return _output; }
    //!@}

    //! The number of bytes in the substrings stored but not yet reassembled
    //!
    //! \note If the byte at a particular index has been submitted twice, it
    //! should only be counted once for the purpose of this function.
    size_t unassembled_bytes() const;

    //! \brief Is the internal state empty (other than the output stream)?
    //! \returns `true` if no substrings are waiting to be assembled
    bool empty() const;
};

#endif  // SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH
