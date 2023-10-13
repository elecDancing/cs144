/*
 * @Descripttion: 
 * @version: 
 * @Author: xp.Zhang
 * @Date: 2023-07-17 17:23:08
 * @LastEditors: xp.Zhang
 * @LastEditTime: 2023-10-13 10:41:12
 */
#include "tcp_config.hh"
#include "tcp_expectation.hh"
#include "tcp_fsm_test_harness.hh"
#include "tcp_header.hh"
#include "tcp_segment.hh"
#include "util.hh"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

using namespace std;
using State = TCPTestHarness::State;

static constexpr unsigned NREPS = 64;

int main() {
    try {
        TCPConfig cfg{};
        cfg.recv_capacity = 65000;//缓冲区容量65000
        auto rd = get_random_generator();

        // loop segments back into the same FSM
        //循环将段送到同一个FSM
        for (unsigned rep_no = 0; rep_no < NREPS; ++rep_no) {
            //生成随机的接收偏移量
            const WrappingInt32 rx_offset(rd());
            //创建测试对象，进行三次握手建立TCP连接
            TCPTestHarness test_1 = TCPTestHarness::in_established(cfg, rx_offset - 1, rx_offset - 1);
            //发送一个ack
            test_1.send_ack(rx_offset, rx_offset, 65000);
            //初始化一个字符串
            string d(cfg.recv_capacity, 0);
            //随机填充字符串中的内容
            generate(d.begin(), d.end(), [&] { return rd(); });

            //初始化发送偏移量为0
            size_t sendoff = 0;
            //发送偏移量小于字符串大小的时候循环执行
            while (sendoff < d.size()) {
                //待发送的数据长度
                const size_t len = min(d.size() - sendoff, static_cast<size_t>(rd()) % 8192);
                if (len == 0) {
                    continue;
                }
                //从字符串中截取一段写入
                test_1.execute(Write{d.substr(sendoff, len)});
                test_1.execute(Tick(1));
                test_1.execute(ExpectBytesInFlight{len});

                test_1.execute(ExpectSegmentAvailable{}, "test 1 failed: cannot read after write()");
                //计算截取的这段数据要分成多少个段
                size_t n_segments = ceil(double(len) / TCPConfig::MAX_PAYLOAD_SIZE);
                //初始化剩余字节数目
                size_t bytes_remaining = len;

                // Transfer the data segments
                for (size_t i = 0; i < n_segments; ++i) {
                    //计算当前数据段的预期大小
                    size_t expected_size = min(bytes_remaining, TCPConfig::MAX_PAYLOAD_SIZE);
                    auto seg = test_1.expect_seg(ExpectSegment{}.with_payload_size(expected_size));
                    //发送完一个段之后需要更新剩余字节数目
                    bytes_remaining -= expected_size;
                    test_1.execute(SendSegment{move(seg)});
                    test_1.execute(Tick(1));
                }

                // Transfer the (bare) ack segments
                for (size_t i = 0; i < n_segments; ++i) {
                    auto seg = test_1.expect_seg(ExpectSegment{}.with_ack(true).with_payload_size(0));
                    test_1.execute(SendSegment{move(seg)});
                    test_1.execute(Tick(1));
                }
                test_1.execute(ExpectNoSegment{});

                test_1.execute(ExpectBytesInFlight{0});

                sendoff += len;//更新发送偏移量
            }

            test_1.execute(ExpectData{}.with_data(d), "test 1 falied: got back the wrong data");
        }
    } catch (const exception &e) {
        cerr << e.what() << endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
