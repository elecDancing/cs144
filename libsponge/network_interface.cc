/*
 * @Descripttion: 
 * @version: 
 * @Author: xp.Zhang
 * @Date: 2023-11-06 18:00:19
 * @LastEditors: xp.Zhang
 * @LastEditTime: 2023-11-07 04:36:55
 */
#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"

#include <iostream>

// Dummy implementation of a network interface
// Translates from {IP datagram, next hop address} to link-layer frame, and from link-layer frame to IP datagram

// For Lab 5, please replace with a real implementation that passes the
// automated checks run by `make check_lab5`.

// You will need to add private members to the class declaration in `network_interface.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface(const EthernetAddress &ethernet_address, const Address &ip_address)
    : _ethernet_address(ethernet_address), _ip_address(ip_address) {
    cerr << "DEBUG: Network interface has Ethernet address " << to_string(_ethernet_address) << " and IP address "
         << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but may also be another host if directly connected to the same network as the destination)
//! (Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) with the Address::ipv4_numeric() method.)
void NetworkInterface::send_datagram(const InternetDatagram &dgram, const Address &next_hop) {
    // convert IP address of next hop to raw 32-bit representation (used in ARP header)
    const uint32_t next_hop_ip = next_hop.ipv4_numeric();

    //create EthernetFrame
    EthernetFrame frame;
    frame.header().type = EthernetHeader::TYPE_IPv4;
    //local MAC
    frame.header().src = _ethernet_address;
    frame.payload() = move(dgram.serialize());
    //if ip-mac table contain dst ip
    if(_table.count(next_hop_ip) && _timer <= _table[next_hop_ip].ttl){
        frame.header().dst = _table[next_hop_ip].mac;
        _frames_out.push(frame);//A EthernetFrame frame is ready
    }
    else{//use queue record next_hop_ip, boardcoast arp frame, record waiting frame
        _pending_arg.push(next_hop_ip);
        _retransmission_arp_frame();
        _frames_waiting.push({frame, next_hop_ip});
    }
}

//! \param[in] frame the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame) {
    //determine whether frame needs to be receives locally
    bool isArpFrame = _ethernet_address_equal(frame.header().dst, ETHERNET_BROADCAST);
    bool isnormalFrame = _ethernet_address_equal(frame.header().dst, _ethernet_address);
    if(!isArpFrame && !isnormalFrame)
        return nullopt;
    else if (frame.header().type == EthernetHeader::TYPE_IPv4) // return InternetDatagram
    {
        InternetDatagram dgram;
        //judge if paload is ok
        if(dgram.parse(frame.payload()) == ParseResult::NoError){
            return dgram;
        }
        else{
            return nullopt;
        }
    }
    //ARP process
    else if(frame.header().type == EthernetHeader::TYPE_ARP){
        ARPMessage msg;
        if(msg.parse(frame.payload()) != ParseResult::NoError)
            return nullopt;
        //record the MAC in ip-mac table
        uint32_t ip = msg.sender_ip_address;
        _table[ip].mac = msg.sender_ethernet_address;
        _table[ip].ttl = _timer + 30 * 1000; //active mapping live 30 seconds

        //if local ip is the boardcast needed ip
        if (msg.opcode == ARPMessage::OPCODE_REQUEST && msg.target_ip_address == _ip_address.ipv4_numeric()){
            ARPMessage reply;
            reply.opcode = ARPMessage::OPCODE_REPLY;
            reply.sender_ethernet_address = _ethernet_address;
            reply.sender_ip_address = _ip_address.ipv4_numeric();
            reply.target_ethernet_address = msg.sender_ethernet_address;
            reply.target_ip_address = msg.sender_ip_address;

            EthernetFrame arp_frame;
            arp_frame.header().type = EthernetHeader::TYPE_ARP;
            arp_frame.header().dst = msg.sender_ethernet_address;
            arp_frame.header().src = _ethernet_address;
            arp_frame.payload() = move(reply.serialize());
            _frames_out.push(arp_frame);
        }
        //_pending IP process
        while(!_pending_arg.empty()){
            uint32_t t_ip = _pending_arg.front();
            if(_table.count(t_ip) && _table[t_ip].ttl >= _timer){
                _pending_arg.pop();
                _pending_flag = false;
            }
            else
                break;
        }
        //_pending frame process
        while(!_frames_waiting.empty()){
            waiting_frame node = _frames_waiting.front();
            if(_table.count(node.ip) && _timer <= _table[node.ip].ttl){
                node.frame.header().dst = _table[node.ip].mac;
                _frames_waiting.pop();
                _frames_out.push(move(node.frame));
            }
            else{
                break;
                
            }
        }
    }
    else return nullopt;
    return nullopt;
}

void NetworkInterface::_retransmission_arp_frame(){
    if(_pending_arg.empty()) //no need to find dstMac
        return;
    // first time retrans or more than 5 seconds since the last retrans
    if(!_pending_flag || (_pending_flag && _timer - _pending_timer >= 5000)){
        uint32_t ip = _pending_arg.front();
        ARPMessage msg;
        //flags request arpmsg
        msg.opcode = ARPMessage::OPCODE_REQUEST;
        msg.sender_ethernet_address = _ethernet_address;
        msg.sender_ip_address = _ip_address.ipv4_numeric();
        msg.target_ip_address = ip;
        // init target_ethernet_address
        msg.target_ethernet_address = {0, 0, 0, 0, 0, 0};

        EthernetFrame arp_frame;
        arp_frame.header().type = EthernetHeader::TYPE_ARP;
        arp_frame.header().src = _ethernet_address;
        arp_frame.header().dst = ETHERNET_BROADCAST;
        arp_frame.payload() = move(msg.serialize());

        _frames_out.push(arp_frame);
        _pending_flag = true;
        _pending_timer = _timer; 
    }
}
bool NetworkInterface::_ethernet_address_equal(const EthernetAddress &a, const EthernetAddress &b) {
    for (int i = 0; i < 6; ++i){
        if(a[i] != b[i])
            return false;
    }
    return true;
}
//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick) { 
    _timer += ms_since_last_tick;
    _retransmission_arp_frame();
}
