/*
 * @Descripttion: 
 * @version: 
 * @Author: xp.Zhang
 * @Date: 2023-07-24 10:13:04
 * @LastEditors: xp.Zhang
 * @LastEditTime: 2023-07-24 14:14:22
 */
#include <cstdint>
#include <ostream>
#include "wrapping_integers.hh"
int main()
{
    unwrap(WrappingInt32(UINT32_MAX - 1), WrappingInt32(0), 3 * (1ul << 32));
    return 0;
}
