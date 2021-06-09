//! command line interface robotkernel bridge
/*!
 * author: Robert Burger <robert.burger@dlr.de>
 */

/*
 * This file is part of bridge_rest.
 *
 * bridge_rest is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * bridge_rest is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with robotkernel.	If not, see <http://www.gnu.org/licenses/>.
 */

#include "robotkernel/rk_type.h"
#include "robotkernel/helpers.h"
#include "robotkernel/service.h"

#include "bridge_rest.h"

#include "string_util/string_util.h"

using namespace std;
using namespace std::placeholders;
using namespace robotkernel;
using namespace string_util;

BRIDGE_DEF(bridge_rest, bridge::rest);

using namespace bridge;

rest::rest(const char*& bridgename, YAML::Node& node) :
    bridge_base(bridgename, "bridge_rest", node)
{
    pthread_mutex_init(&service_map_lock, NULL);
}

rest::~rest() {
    pthread_mutex_destroy(&service_map_lock);
}

void rest::add_service(const robotkernel::service_t &svc) {
    pthread_mutex_lock(&service_map_lock);
    service_map[std::make_pair(svc.owner, svc.name)] = svc;
    pthread_mutex_unlock(&service_map_lock);
}

void rest::remove_service(const robotkernel::service_t &svc) {
    pthread_mutex_lock(&service_map_lock);

    for (auto it = service_map.begin(); it != service_map.end(); ++it) {
        if ((it->first.first == svc.owner) && (it->first.second == svc.name)) {
            service_map.erase(it);
            break;
        }
    }

    pthread_mutex_unlock(&service_map_lock);
}

