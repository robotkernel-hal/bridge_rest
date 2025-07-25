//! command line interface robotkernel bridge
/*!
 * author: Jan Cremer <jan.cremer@dlr.de>, Robert Burger <robert.burger@dlr.de>
 */

/*
 * This file is part of module_ethercat.
 *
 * module_ethercat is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 * 
 * module_ethercat is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with module_ethercat; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef ROBOTKERNEL_CLI_BRIDGE_H
#define ROBOTKERNEL_CLI_BRIDGE_H

#include "robotkernel/service.h"

#include "robotkernel/bridge_base.h"
#include "robotkernel/runnable.h"

#include <httpserver.hpp>

#ifdef REST_SVC_MAP_DEBUG
#define rest_svc_map_debug(...) printf(__VA_ARGS__)
#else 
#define rest_svc_map_debug(...)
#endif

namespace bridge {

class rest : 
    public robotkernel::bridge_base,
    public robotkernel::runnable
{
    public:
        class rest_service {
            public:
                std::string name;
                robotkernel::service_t *svc;

            public:
                rest_service() : name(""), svc(nullptr) {};
                rest_service(std::string name, std::string rest, robotkernel::service_t svc) : name(name), svc(nullptr) { 
                    if (rest == "") {
                        // the real endpoint
                        this->svc = new robotkernel::service_t(); 
                        *this->svc = svc; 
                    } else {
                        insert(rest, svc); 
                    }
                };
                
                ~rest_service() { 
                    rest_svc_map_debug("%s: destructing rest_service\n", name.c_str());

                    if (svc != nullptr) { 
                        delete svc; 
                    } 
                };

                void insert(std::string rest, robotkernel::service_t svc) {
                    size_t pos_dot = rest.find_first_of(".");
                    rest_svc_map_debug("%s: pos_dot is %d for %s\n", name.c_str(), (int)pos_dot, rest.c_str());

                    std::string key, value("");
                    if (pos_dot == std::string::npos) {  // the real endpoint
                        key = rest;
                    } else {
                        key = rest.substr(0, pos_dot);
                        value = rest.substr(pos_dot + 1);
                    }

                    if (children.find(key) == children.end()) {
                        rest_svc_map_debug("%s: inserting new key %s and value %s \n", name.c_str(), key.c_str(), value.c_str());
                        children.emplace(key, new rest_service(key, value, svc));
                    } else {
                        rest_svc_map_debug("%s: inserting exit key %s and value %s \n", name.c_str(), key.c_str(), value.c_str());
                        children[key]->insert(value, svc);
                    }
                }

                bool remove(std::string rest) {
                    size_t pos_dot = rest.find_first_of(".");
                    rest_svc_map_debug(" %s: pos_dot is %d for %s\n", name.c_str(), (int)pos_dot, rest.c_str());

                    std::string key, value("");
                    if (pos_dot == std::string::npos) {  // the real endpoint
                        key = rest;
                    } else {
                        key = rest.substr(0, pos_dot);
                        value = rest.substr(pos_dot + 1);
                    }

                    if (children.find(key) == children.end()) {
                        rest_svc_map_debug("rem %s: nothing key %s and value %s \n", name.c_str(), key.c_str(), value.c_str());
                    } else {
                        rest_svc_map_debug("%s: inserting exit key %s and value %s \n", name.c_str(), key.c_str(), value.c_str());
                        if (children[key]->remove(value)) {
                            // empty
                            auto it = children.find(key);
                            delete children[key];
                            children.erase(it);
                        }
                    }
                            
                    return children.empty();                
                }

                void printme(std::string indent) {
                    printf("%sthis is %s : %p\n", indent.c_str(), name.c_str(), svc);

                    for (const auto& c : children) {
                        c.second->printme(indent + " ");
                    }
                }

                rest_service * get_rest_service(std::string name) {
                    size_t pos_dot = name.find_first_of(".");
                    rest_svc_map_debug("get_rest_service %s, my name %s, svc anchor %p, pos_dot %d\n", name.c_str(), this->name.c_str(), this->svc, (int)pos_dot);

                    std::string key, value("");
                    if (pos_dot == std::string::npos) {
                        key = name;
                    } else {
                        key = name.substr(0, pos_dot);
                        value = name.substr(pos_dot + 1);
                    }

                    if (children.find(key) != children.end()) {
                        if (value == "") {
                            return children[key];
                        }

                        return children[key]->get_rest_service(value);
                    }

                    return nullptr; 
                }

                std::map<std::string, rest_service *> children;
        };

        class resource : public httpserver::http_resource {
            private:
                rest *parent;

            public:
                resource(rest *r) : parent(r) {};
                const std::shared_ptr<httpserver::http_response> render(const httpserver::http_request&);
        };

    public:
        //! construct bridge rest
        rest(const char*& bridgename, YAML::Node& node);

        //! destruct bridge rest 
        ~rest();

        void add_service(const robotkernel::service_t &svc);
        void remove_service(const robotkernel::service_t &svc);

        //! web server thread
        void run();
                
        const std::shared_ptr<httpserver::http_response> render(const httpserver::http_request&);
        
        const std::shared_ptr<httpserver::http_response> list_services(const httpserver::http_request&);

    private:
        //! services map
        rest_service services;
        pthread_mutex_t service_map_lock;

        httpserver::webserver ws;
        resource res;
};

}; // namespace bridge

#endif

