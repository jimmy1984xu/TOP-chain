// Copyright (c) 2017-2019 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "xgossip/gossip_interface.h"

#include "xgossip/include/gossip_utils.h"
#include "xpbase/base/kad_key/kadmlia_key.h"
#include "xpbase/base/top_log.h"
#include "xpbase/base/top_utils.h"
#include "xpbase/base/uint64_bloomfilter.h"

#include <cinttypes>
#include <unordered_set>

namespace top {

namespace gossip {

void GossipInterface::CheckDiffNetwork(transport::protobuf::RoutingMessage & message) {
    if (message.gossip().diff_net()) {
        auto gossip_param = message.mutable_gossip();
        gossip_param->set_diff_net(false);
        // message.clear_bloomfilter();
        message.set_hop_num(message.hop_num() - 1);
        TOP_DEBUG("message from diff network and arrive the des network at the first time. %s", message.debug().c_str());
    }
}

uint64_t GossipInterface::GetDistance(const std::string & src, const std::string & des) {
    assert(src.size() >= sizeof(uint64_t));
    assert(des.size() >= sizeof(uint64_t));
    assert(src.size() == des.size());
    uint64_t dis = 0;
    uint32_t index = src.size() - 1;
    uint32_t rollleft_num = 56;
    for (uint32_t i = 0; i < 8; ++i) {
        dis += (static_cast<uint64_t>(static_cast<uint8_t>(src[index]) ^ static_cast<uint8_t>(des[index])) << rollleft_num);
        --index;
        rollleft_num -= 8;
    }
    return dis;
}

void GossipInterface::Send(transport::protobuf::RoutingMessage & message, const std::vector<kadmlia::NodeInfoPtr> & nodes) {
    _xip2_header xip2_header;
    memset(&xip2_header, 0, sizeof(xip2_header));
    // add serialize protocol
    xip2_header.ver_protocol = kSerializeProtocolProtobuf;

    std::string body;
    body.reserve(sizeof(xip2_header) + 2048);
    body.append((const char *)&xip2_header, sizeof(xip2_header));

    if (!message.AppendToString(&body)) {
        TOP_WARN2("wrouter message SerializeToString failed");
        return;
    }
    base::xpacket_t packet(base::xcontext_t::instance(), (uint8_t *)body.c_str(), body.size(), 0, body.size(), true);

    auto each_call = [this, &message, &packet](kadmlia::NodeInfoPtr node_info_ptr) {
        if (!node_info_ptr) {
            TOP_WARN2("kadmlia::NodeInfoPtr null");
            return false;
        }

        packet.reset_process_flags(0);
        packet.set_to_ip_addr(node_info_ptr->public_ip);
        packet.set_to_ip_port(node_info_ptr->public_port);

        if (kadmlia::kKadSuccess != transport_ptr_->SendDataWithProp(packet, node_info_ptr->udp_property)) {
            TOP_WARN2("SendData to  endpoint(%s:%d) failed", node_info_ptr->public_ip.c_str(), node_info_ptr->public_port);
            return false;
        }

        return true;
    };

    std::for_each(nodes.begin(), nodes.end(), each_call);
}

// usually for root-broadcast
void GossipInterface::MutableSend(transport::protobuf::RoutingMessage & message, const std::vector<kadmlia::NodeInfoPtr> & nodes) {
    _xip2_header xip2_header;
    memset(&xip2_header, 0, sizeof(xip2_header));
    xip2_header.ver_protocol = kSerializeProtocolProtobuf;

    std::string xdata;
    xdata.reserve(sizeof(xip2_header) + 2048);

    uint8_t local_buf[kUdpPacketBufferSize];
    base::xpacket_t packet(base::xcontext_t::instance(), local_buf, sizeof(local_buf), 0, false);

    auto each_call = [this, &message, &packet, &xdata, &xip2_header](kadmlia::NodeInfoPtr node_info_ptr) {
        if (!node_info_ptr) {
            TOP_WARN2("kadmlia::NodeInfoPtr null");
            return false;
        }

        message.set_des_node_id(node_info_ptr->node_id);
        std::string body;
        if (!message.SerializeToString(&body)) {
            TOP_WARN2("wrouter message SerializeToString failed");
            return false;
        }
        xdata.clear();
        xdata.append((const char *)&xip2_header, sizeof(xip2_header));
        xdata.append(body);

        packet.reset_process_flags(0);
        packet.reset();
        packet.get_body().push_back((uint8_t *)xdata.data(), xdata.size());
        packet.set_to_ip_addr(node_info_ptr->public_ip);
        packet.set_to_ip_port(node_info_ptr->public_port);

        if (kadmlia::kKadSuccess != transport_ptr_->SendDataWithProp(packet, node_info_ptr->udp_property)) {
            TOP_WARN2("SendData to  endpoint(%s:%d) failed", node_info_ptr->public_ip.c_str(), node_info_ptr->public_port);
            return false;
        }

        return true;
    };

    std::for_each(nodes.begin(), nodes.end(), each_call);
}

// usually for root-broadcast hash
void GossipInterface::MutableSendHash(transport::protobuf::RoutingMessage & message, const std::vector<kadmlia::NodeInfoPtr> & nodes) {
    _xip2_header xip2_header;
    memset(&xip2_header, 0, sizeof(xip2_header));
    xip2_header.ver_protocol = kSerializeProtocolProtobuf;

    std::string xdata;
    xdata.reserve(sizeof(xip2_header) + 2048);

    uint8_t local_buf[kUdpPacketBufferSize];
    base::xpacket_t packet(base::xcontext_t::instance(), local_buf, sizeof(local_buf), 0, false);

    auto each_call = [this, &message, &packet, &xdata, &xip2_header](kadmlia::NodeInfoPtr node_info_ptr) {
        if (!node_info_ptr) {
            TOP_WARN2("kadmlia::NodeInfoPtr null");
            return false;
        }

        std::string body;
        if (!message.SerializeToString(&body)) {
            TOP_WARN2("wrouter message SerializeToString failed");
            return false;
        }
        xdata.clear();
        xdata.append((const char *)&xip2_header, sizeof(xip2_header));
        xdata.append(body);

        packet.reset_process_flags(0);
        packet.reset();
        packet.get_body().push_back((uint8_t *)xdata.data(), xdata.size());
        packet.set_to_ip_addr(node_info_ptr->public_ip);
        packet.set_to_ip_port(node_info_ptr->public_port);

        if (kadmlia::kKadSuccess != transport_ptr_->SendDataWithProp(packet, node_info_ptr->udp_property)) {
            TOP_WARN2("SendData to  endpoint(%s:%d) failed", node_info_ptr->public_ip.c_str(), node_info_ptr->public_port);
            return false;
        }

        return true;
    };

    std::for_each(nodes.begin(), nodes.end(), each_call);
}

// todo next version delete GetNeighborCount.(only used in old broadcast)
uint32_t GossipInterface::GetNeighborCount(transport::protobuf::RoutingMessage & message) {
    if (message.gossip().neighber_count() > 0) {
        return message.gossip().neighber_count();
    }
    return 3;
}

std::vector<kadmlia::NodeInfoPtr> GossipInterface::GetRandomNodes(std::vector<kadmlia::NodeInfoPtr> & neighbors, uint32_t number_to_get) const {
    if (neighbors.size() <= number_to_get) {
        return neighbors;
    }
    std::random_shuffle(neighbors.begin(), neighbors.end());
    return std::vector<kadmlia::NodeInfoPtr>{neighbors.begin(), neighbors.begin() + number_to_get};
}
#if 0
void GossipInterface::SendLayered(transport::protobuf::RoutingMessage & message, const std::vector<kadmlia::NodeInfoPtr> & nodes) {
    uint64_t min_dis = message.gossip().min_dis();
    uint64_t max_dis = message.gossip().max_dis();
    if (max_dis <= 0) {
        max_dis = std::numeric_limits<uint64_t>::max();
    }

    uint8_t local_buf[kUdpPacketBufferSize];
    base::xpacket_t packet(base::xcontext_t::instance(), local_buf, sizeof(local_buf), 0, false);
    _xip2_header xip2_header;
    memset(&xip2_header, 0, sizeof(xip2_header));
    xip2_header.ver_protocol = kSerializeProtocolProtobuf;
    std::string header((const char *)&xip2_header, sizeof(xip2_header));
    std::string xdata;

    for (uint32_t i = 0; i < nodes.size(); ++i) {
        auto gossip = message.mutable_gossip();
        if (i == 0) {
            gossip->set_min_dis(min_dis);
            gossip->set_left_min(min_dis);

            if (nodes.size() == 1) {
                gossip->set_max_dis(max_dis);
                gossip->set_right_max(max_dis);
            } else {
                gossip->set_max_dis(nodes[0]->hash64);
                gossip->set_right_max(nodes[1]->hash64);
            }
        }

        if (i > 0 && i < (nodes.size() - 1)) {
            gossip->set_min_dis(nodes[i - 1]->hash64);
            gossip->set_max_dis(nodes[i]->hash64);

            if (i == 1) {
                gossip->set_left_min(min_dis);
            } else {
                gossip->set_left_min(nodes[i - 2]->hash64);
            }
            gossip->set_right_max(nodes[i + 1]->hash64);
        }

        if (i > 0 && i == (nodes.size() - 1)) {
            gossip->set_min_dis(nodes[i - 1]->hash64);
            gossip->set_max_dis(max_dis);

            if (i == 1) {
                gossip->set_left_min(min_dis);
            } else {
                gossip->set_left_min(nodes[i - 2]->hash64);
            }
            gossip->set_right_max(max_dis);
        }

        std::string body;
        if (!message.SerializeToString(&body)) {
            TOP_WARN2("wrouter message SerializeToString failed");
            return;
        }
        xdata = header + body;
        packet.reset();
        packet.get_body().push_back((uint8_t *)xdata.data(), xdata.size());
        packet.set_to_ip_addr(nodes[i]->public_ip);
        packet.set_to_ip_port(nodes[i]->public_port);

        if (kadmlia::kKadSuccess != transport_ptr_->SendDataWithProp(packet, nodes[i]->udp_property)) {
            TOP_WARN2("SendData to  endpoint(%s:%d) failed", nodes[i]->public_ip.c_str(), nodes[i]->public_port);
            continue;
        }
    };
}
#endif
void GossipInterface::SendDispatch(transport::protobuf::RoutingMessage & message, const std::vector<gossip::DispatchInfos> & dispatch_nodes) {
    // uint64_t min_dis = message.gossip().min_dis();
    // uint64_t max_dis = message.gossip().max_dis();
    // if (max_dis <= 0) {
    //     max_dis = std::numeric_limits<uint64_t>::max();
    // }

    uint8_t local_buf[kUdpPacketBufferSize];
    base::xpacket_t packet(base::xcontext_t::instance(), local_buf, sizeof(local_buf), 0, false);
    _xip2_header xip2_header;
    memset(&xip2_header, 0, sizeof(xip2_header));
    xip2_header.ver_protocol = kSerializeProtocolProtobuf;
    std::string header((const char *)&xip2_header, sizeof(xip2_header));
    std::string xdata;

    for (uint32_t i = 0; i < dispatch_nodes.size(); ++i) {
        auto nodes = dispatch_nodes[i].nodes;
        auto gossip = message.mutable_gossip();

        gossip->set_sit1(dispatch_nodes[i].sit1);
        gossip->set_sit2(dispatch_nodes[i].sit2);
        xdbg("[debug] send to %s:%d % " PRIu64 " % " PRIu64, nodes->public_ip.c_str(), nodes->public_port, dispatch_nodes[i].sit1, dispatch_nodes[i].sit2);

        std::string body;
        if (!message.SerializeToString(&body)) {
            xwarn("wrouter message SerializeToString failed");
            return;
        }

        xdata = header + body;
        packet.reset();
        packet.get_body().push_back((uint8_t *)xdata.data(), xdata.size());
        packet.set_to_ip_addr(nodes->public_ip);
        packet.set_to_ip_port(nodes->public_port);

        if (kadmlia::kKadSuccess != transport_ptr_->SendDataWithProp(packet, nodes->udp_property)) {
            xinfo("SendDispatch send to (%s:%d) failed % " PRIu64 " % " PRIu64, nodes->public_ip.c_str(), nodes->public_port, dispatch_nodes[i].sit1, dispatch_nodes[i].sit2);
            continue;
        }
    };
}

}  // namespace gossip

}  // namespace top
