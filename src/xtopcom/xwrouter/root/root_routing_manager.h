// Copyright (c) 2017-2019 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <map>
#include <set>
#include <memory>
#include <mutex>

#include "xpbase/base/top_utils.h"
#include "xpbase/base/top_config.h"
#include "xpbase/base/kad_key/kadmlia_key.h"
#include "xkad/routing_table/node_info.h"
#include "xkad/routing_table/bootstrap_cache.h"

namespace top {

namespace kadmlia {
    class RoutingTable;
}

namespace transport {
    class Transport;
}

namespace wrouter {
class RootRoutingManager;
using RootRoutingManagerPtr = std::shared_ptr<RootRoutingManager>;

class RootRoutingManager {
public:
    static RootRoutingManagerPtr Instance();
    RootRoutingManager();
    ~RootRoutingManager();
    int AddRoutingTable(
            std::shared_ptr<transport::Transport> transport,
            const base::Config& config,
            base::KadmliaKeyPtr kad_key_ptr,
            on_bootstrap_cache_get_callback_t get_cache_callback,
            on_bootstrap_cache_set_callback_t set_cache_callback,
            bool wait_for_joined = true);
    void RemoveRoutingTable(uint64_t service_type);
    void RemoveAllRoutingTable();
    std::shared_ptr<kadmlia::RoutingTable> GetRoutingTable(uint64_t service_type);
    std::shared_ptr<kadmlia::RoutingTable> GetRoutingTable(const std::string& routing_id);
    int GetRootNodes(uint32_t network_id, std::vector<kadmlia::NodeInfoPtr>& root_nodes);
    int GetRootNodes(
            const std::string& des_id,
            std::vector<kadmlia::NodeInfoPtr>& root_nodes);
    int GetRootNodesV2(
            const std::string& des_id,
            uint64_t service_type,
            std::vector<kadmlia::NodeInfoPtr>& root_nodes);
    int GetRootBootstrapCache(
            std::set<std::pair<std::string, uint16_t>>& boot_endpoints);
    int GetBootstrapRootNetwork(
            uint64_t service_type,
            std::set<std::pair<std::string, uint16_t>>& boot_endpoints);

    bool GetServiceBootstrapRootNetwork(
            uint64_t service_type,
            std::set<std::pair<std::string, uint16_t>>& boot_endpoints);
    bool SetCacheServiceType(uint64_t service_type);

    using GetRootNodesV2AsyncCallback = std::function<void(uint64_t, const std::vector<kadmlia::NodeInfoPtr>&)>;
    int GetRootNodesV2Async(
            const std::string& des_kroot_id,
            uint64_t des_service_type,
            GetRootNodesV2AsyncCallback cb);

private:
    int CreateRoutingTable(
            std::shared_ptr<transport::Transport> transport,
            const base::Config& config,
            base::KadmliaKeyPtr kad_key_ptr,
            on_bootstrap_cache_get_callback_t get_cache_callback,
            on_bootstrap_cache_set_callback_t set_cache_callback,
            bool wait_for_joined);

    void OnGetRootNodesV2Async(
            GetRootNodesV2AsyncCallback cb,
            uint64_t service_type,
            const std::vector<kadmlia::NodeInfoPtr>& nodes);

private:
    std::map<uint64_t, std::shared_ptr<kadmlia::RoutingTable>> root_routing_map_;
    std::mutex root_routing_map_mutex_;

    DISALLOW_COPY_AND_ASSIGN(RootRoutingManager);
};

}  // namespace wrouter

}  // namespace top
