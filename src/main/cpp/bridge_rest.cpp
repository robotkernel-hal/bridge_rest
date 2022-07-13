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
using namespace httpserver;

// Checks whether `str' starts with `start' ignoring case
static bool starts_with(const std::string& str, const std::string& start) {
    if (&start == &str) 
        return true; // str and start are the same string

    if (start.length() > str.length()) 
        return false;

    for (size_t i = 0; i < start.length(); ++i) {
        if (start[i] != str[i]) 
            return false;
    }

    return true;
}

rest::rest(const char*& bridgename, YAML::Node& node) :
    bridge_base(bridgename, "bridge_rest", node),
    ws(create_webserver(8080).no_regex_checking()), res(this)
{
    pthread_mutex_init(&service_map_lock, NULL);
    start();
    
    try {
        ws.register_resource(string("/api/v2.0/list"), &res);
    } catch (std::exception& e) {
        log(error, "caught exception: %s\n", e.what());
    }
}

rest::~rest() {
    pthread_mutex_destroy(&service_map_lock);
    stop();
}

void rest::run() {
    log(info, "rest server running!\n");
    ws.start(true);
}
        
class hello_world_resource : public http_resource {
public:
    const std::shared_ptr<http_response> render(const http_request&) {
        return std::shared_ptr<http_response>(new string_response("Hello, World!"));
    }
};


void rest::add_service(const robotkernel::service_t &svc) {
    std::string name = format_string("/api/v2.0/%s/%s", svc.owner.c_str(), svc.name.c_str());

    pthread_mutex_lock(&service_map_lock);
    service_map[name] = svc;
    pthread_mutex_unlock(&service_map_lock);

    log(info, "adding %s\n", name.c_str());
    ws.register_resource(name, &res, true);
    log(info, "...done\n");

    int i = 1;
    YAML::Node message_definition = YAML::Load(svc.service_definition);
    if (message_definition["request"]) {
        const YAML::Node& request = message_definition["request"];

        for (YAML::const_iterator it = request.begin(); 
                it != request.end(); ++it) {
            for (const auto& kv : *it) {
                string key   = kv.first.as<string>();
                string value = kv.second.as<string>();
                                
                if (key == "string") {
                    name = format_string("%s/{arg%d}", name.c_str(), i++);
                    //name = format_string("%s/\{%s\}", name.c_str(), value.c_str());
                }

#define push_back_type(type) \
                if (key == #type) {                                 \
                    name = format_string("%s/{%s|[0-9]+}", name.c_str(), value.c_str()); \
                }

                push_back_type(uint64_t);
                push_back_type(int64_t);
                push_back_type(uint32_t);
                push_back_type(int32_t);
                push_back_type(uint16_t);
                push_back_type(int16_t);
                push_back_type(uint8_t);
                push_back_type(int8_t);
                push_back_type(float);
                push_back_type(double);
#undef push_back_type
            }
        }
    }
    
    //log(info, "adding more services %s\n", name.c_str());
    //ws.register_resource(name, &res);
    //log(info, "...done\n");
}

void rest::remove_service(const robotkernel::service_t &svc) {
    std::string name = format_string("/api/v2.0/%s/%s", svc.owner.c_str(), svc.name.c_str());

    pthread_mutex_lock(&service_map_lock);

    for (auto it = service_map.begin(); it != service_map.end(); ++it) {
        if ((it->first == name)) {
            service_map.erase(it);
            break;
        }
    }

    pthread_mutex_unlock(&service_map_lock);
}

const std::shared_ptr<http_response> rest::resource::render(const http_request& req) {
    return parent->render(req);
}
      
const std::shared_ptr<http_response> rest::render(const http_request& req) {
    //uint8_t svc[1024];
    //req.set_data(&svc[0], signature.c_str());
    //uint64_t adr = (uint64_t)&svc[0];
    std::string method = req.get_method();

    log(verbose, "got method \"%s\"\n", method.c_str());

    std::string content = req.get_content();
    log(verbose, "got content \"%s\"\n", content.c_str());
    auto content_node = YAML::Load(content);

    std::string name = req.get_path();
    
    if (name == "/api/v2.0/list") {
        return list_services(req);
    }

    auto& _svc = service_map[name];
    
    log(verbose, "rendering \"%s\"\n", name.c_str());

    if (method == "GET") {
        YAML::Node answer = YAML::Load(_svc.service_definition);

        YAML::Emitter emitter;
        emitter << YAML::DoubleQuoted << YAML::Flow << YAML::BeginSeq << answer;
        std::string json(emitter.c_str() + 1);  // Remove beginning [ character

        return std::shared_ptr<http_response>(new string_response(json));
    }

    // request arguments
    robotkernel::service_arglist_t service_request;

    YAML::Node message_definition = YAML::Load(_svc.service_definition);
    if (message_definition["request"]) {
        const YAML::Node& request = message_definition["request"];

        for (YAML::const_iterator it = request.begin(); 
                it != request.end(); ++it) {
            for (const auto& kv : *it) {
                string key   = kv.first.as<string>();
                string value = kv.second.as<string>();

                log(verbose, "request field: %s %s\n", key.c_str(), value.c_str());

                if (content_node[value]) {
                    log(verbose, "  field found in json message\n");
                } else {
                    log(warning, "  field NOT found in json message\n");
                }
                
#define push_back_type(type) \
                if (key == #type) {                                 \
                    if (content_node[value]) {                      \
                        log(verbose, "  pushing to service_request, %d\n", (type)content_node[value].as<type>()); \
                        service_request.push_back((type)content_node[value].as<type>());         \
                    } else {                                        \
                        service_request.push_back((type)0);         \
                    }                                               \
                }

                push_back_type(uint64_t);
                push_back_type(int64_t);
                push_back_type(uint32_t);
                push_back_type(int32_t);
                push_back_type(uint16_t);
                push_back_type(int16_t);
                push_back_type(uint8_t);
                push_back_type(int8_t);
                push_back_type(float);
                push_back_type(double);
                push_back_type(string);
#undef push_back_type


            }
        }
    }

    // call robotkernel service
    robotkernel::service_arglist_t service_response;
    _svc.callback(service_request, service_response);

    std::list<uint8_t *> to_delete;

    YAML::Node answer = YAML::Node(YAML::NodeType::Map);

    if (message_definition["response"]) {
        const YAML::Node& response = message_definition["response"];
        int i = 0;

        for (YAML::const_iterator it = response.begin(); 
                it != response.end(); ++it) {
            for (const auto& kv : *it) {
                string key   = kv.first.as<string>();
                string value = kv.second.as<string>();

                if (starts_with(key, "vector")) {
                    const size_t equals_idx = key.find_first_of('/');
                    if (std::string::npos != equals_idx) {
                        string vector = key.substr(0, equals_idx);
                        string real_key = key.substr(equals_idx + 1);

                        const std::vector<any> elem = any_cast<std::vector<any> >(service_response[i++]);
#define push_back_type(type)                            \
                        if (real_key == #type) {                            \
                            for (unsigned i = 0; i < elem.size(); ++i) {    \
                                answer[value].push_back(any_cast<type>(elem[i]));     \
                        } }
                        
                        push_back_type(uint64_t);
                        push_back_type(int64_t);
                        push_back_type(uint32_t);
                        push_back_type(int32_t);
                        push_back_type(uint16_t);
                        push_back_type(int16_t);
                        push_back_type(uint8_t);
                        push_back_type(int8_t);
                        push_back_type(float);
                        push_back_type(double);
                        push_back_type(string);
#undef push_back_type

                    }
                } else {
#define push_back_type(type)                            \
                    if (key == #type) {                                 \
                        const type& v = any_cast<type>(service_response[i++]);          \
                        answer[value] = v;                              \
                    }

                    push_back_type(uint64_t);
                    push_back_type(int64_t);
                    push_back_type(uint32_t);
                    push_back_type(int32_t);
                    push_back_type(uint16_t);
                    push_back_type(int16_t);
                    push_back_type(uint8_t);
                    push_back_type(int8_t);
                    push_back_type(float);
                    push_back_type(double);
                    push_back_type(string);
#undef push_back_type
                }
            }
        }
    }

    for (std::list<uint8_t *>::iterator it = to_delete.begin();
            it != to_delete.end(); ++it) {
        delete[] (*it);
    }

    YAML::Emitter emitter;
    emitter << YAML::DoubleQuoted << YAML::Flow << YAML::BeginSeq << answer;
    std::string json(emitter.c_str() + 1);  // Remove beginning [ character

    return std::shared_ptr<http_response>(new string_response(json));
}

const std::shared_ptr<http_response> rest::list_services(const http_request& req) {
    YAML::Node answer = YAML::Node(YAML::NodeType::Map);

    pthread_mutex_lock(&service_map_lock);

    for (auto it = service_map.begin(); it != service_map.end(); ++it) {
        answer["services"].push_back(it->first);
    }

    pthread_mutex_unlock(&service_map_lock);

    YAML::Emitter emitter;
    emitter << YAML::DoubleQuoted << YAML::Flow << YAML::BeginSeq << answer;
    std::string json(emitter.c_str() + 1);  // Remove beginning [ character

    return std::shared_ptr<http_response>(new string_response(json));
}

