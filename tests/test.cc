/*
 * @Descripttion: 
 * @version: 
 * @Author: xp.Zhang
 * @Date: 2023-07-24 10:13:04
 * @LastEditors: xp.Zhang
 * @LastEditTime: 2023-11-08 01:05:45
 */
#include <cstdint>
#include <iostream>
#include <ostream>
// #include "wrapping_integers.hh"
int main()
{
    //unwrap(WrappingInt32(UINT32_MAX - 1), WrappingInt32(0), 3 * (1ul << 32));
    uint32_t a = 0xffffffff << 32;
    std::cout << a << std::endl;
    return 0;
}
