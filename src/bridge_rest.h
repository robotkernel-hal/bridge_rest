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

namespace bridge {
#ifdef EMACS
}
#endif

class rest : 
    public robotkernel::bridge_base,
    public robotkernel::runnable
{
    public:
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
        typedef std::map<std::string, robotkernel::service_t> service_map_t;
        service_map_t service_map;
        pthread_mutex_t service_map_lock;

        httpserver::webserver ws;
        resource res;
};

#ifdef EMACS
{
#endif
}

#endif

