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
using namespace httpserver;

rest::rest(const char*& bridgename, YAML::Node& node) :
    bridge_base(bridgename, "bridge_rest", node),
    ws(create_webserver(8080).no_regex_checking()), res(this)
{
    pthread_mutex_init(&service_map_lock, NULL);
    start();
    
    ws.register_resource(string("/api/v2.0/list"), &res);
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
    ws.register_resource(name, &res);
    log(info, "...done\n");
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

    std::string name = req.get_path();
    
    if (name == "/api/v2.0/list") {
        return list_services(req);
    }

    auto& _svc = service_map[name];

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

//                string ln_dt = service_datatype_to_ln(key);
//
//                if (ln_dt == "char*") {
//                    uint32_t tmp_len = ((uint32_t *)adr)[0];
//                    adr += 4;
//                    char *tmp_adr = ((char **)adr)[0];
//                    adr += sizeof(char*);
//
//                    service_request.push_back(string(tmp_adr, tmp_len));
//                } else if (starts_with(key, "vector")) {
//                    const size_t equals_idx = key.find_first_of('/');
//                    if (std::string::npos != equals_idx)
//                    {
//                        //signature "uint32_t 4 1,[uint32_t 4 1,char* 1 1]* 8 1|uint32_t 4 1,[uint32_t 4 1,char* 1 1]* 8 1"
//
//                        string vector = key.substr(0, equals_idx);
//                        string real_key = key.substr(equals_idx + 1);
//                        string ln_dt = service_datatype_to_ln(real_key);
//
//#define add_vector_type(type) \
//                        if (ln_dt == #type) {                                                                       \
//                            uint32_t len = ((uint32_t *)adr)[0];                                                    \
//                            adr += 4;                                                                               \
//                            \
//                            std::vector<rk_type> entries(len);                                                      \
//                            type *tmp_adr = ((type **)adr)[0];                                                      \
//                            adr += sizeof(type *);                                                                  \
//                            \
//                            for (unsigned i = 0; i < len; ++i) {                                                    \
//                                entries[i] = tmp_adr[i];                                                            \
//                            }                                                                                       \
//                            service_request.push_back(entries);                                                     \
//                        }
//                        add_vector_type(uint64_t);
//                        add_vector_type(int64_t);
//                        add_vector_type(uint32_t);
//                        add_vector_type(int32_t);
//                        add_vector_type(uint16_t);
//                        add_vector_type(int16_t);
//                        add_vector_type(uint8_t);
//                        add_vector_type(int8_t);
//                        add_vector_type(float);
//                        add_vector_type(double);
//#undef add_vector_type
//
//#define add_vector_type_char(type) \
//                        if (ln_dt == #type) {                                                                       \
//                            uint32_t len = ((uint32_t *)adr)[0];                                                    \
//                            adr += 4;                                                                               \
//                            \
//                            std::vector<rk_type> entries(len);                                                      \
//                            ln_vector_t* lnentries = *(ln_vector_t **)adr;                                            \
//                            \
//                            for (unsigned i = 0; i < len; ++i) {                                                    \
//                                entries[i] = string((char *)lnentries[i].val, lnentries[i].len);                    \
//                            }                                                                                       \
//                            service_request.push_back(entries);                                                     \
//                        }
//                        add_vector_type_char(char*);
//#undef add_vector_type_char
//                    }
//
//                } else if (ends_with(ln_dt, string("*"))) {               
//                    service_request.push_back(((uint32_t *)adr)[0]);    //<! array length
//                    adr += 4;                
//#define push_back_type(type) \
//                    if (ln_dt == #type) {                               \
//                        service_request.push_back(((type*)adr)[0]);     \
//                        adr += sizeof(type);                            \
//                    }
//
//                    push_back_type(uint64_t*);
//                    push_back_type(int64_t*);
//                    push_back_type(uint32_t*);
//                    push_back_type(int32_t*);
//                    push_back_type(uint16_t*);
//                    push_back_type(int16_t*);
//                    push_back_type(uint8_t*);
//                    push_back_type(int8_t*);
//                    push_back_type(float*);
//                    push_back_type(double*);
//                } else {
//                    push_back_type(uint64_t);
//                    push_back_type(int64_t);
//                    push_back_type(uint32_t);
//                    push_back_type(int32_t);
//                    push_back_type(uint16_t);
//                    push_back_type(int16_t);
//                    push_back_type(uint8_t);
//                    push_back_type(int8_t);
//                    push_back_type(float);
//                    push_back_type(double);
//#undef push_back_type
//                }
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

                #define push_back_type(type)                            \
                    if (key == #type) {                                 \
                        const type& v = service_response[i++];          \
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

                //<< YAML::BeginMap;
                //answer << YAML::Key << value << YAML::Value << "";
                //answer << YAML::EndMap;
                
//                string ln_dt = service_datatype_to_ln(key);
//                //            int ln_dt_size = ln_datatype_size(ln_dt);
//
//                if (ln_dt == "char*") {
//                    const string& tmp_string = service_response[i++];
//                    ((uint32_t *)adr)[0] = (uint32_t)tmp_string.size();
//                    adr += 4;
//                    if (tmp_string.size()) {
//                        ((const char **)adr)[0] = (const char *)strdup(tmp_string.c_str());
//                        to_delete.push_back((uint8_t *)(((const char **)adr)[0]));
//                    } else 
//                        ((const char **)adr)[0] = NULL;
//                    adr += sizeof(char *);
//                } else if (starts_with(key, "vector")) {
//                    const size_t equals_idx = key.find_first_of('/');
//                    if (std::string::npos != equals_idx)
//                    {
//                        //signature "uint32_t 4 1,[uint32_t 4 1,char* 1 1]* 8 1|uint32_t 4 1,[uint32_t 4 1,char* 1 1]* 8 1"
//
//                        string vector = key.substr(0, equals_idx);
//                        string real_key = key.substr(equals_idx + 1);
//                        string ln_dt = service_datatype_to_ln(real_key);
//
//                        const std::vector<robotkernel::rk_type> elem = service_response[i++];
//
//#define add_vector_type(type) \
//                        if (ln_dt == #type) {                                                                       \
//                            ((uint32_t *)adr)[0] = (uint32_t)elem.size();                                           \
//                            adr += 4;                                                                               \
//                            \
//                            type* entries = new type[elem.size()];                                                  \
//                            to_delete.push_back((uint8_t *)entries);                                                \
//                            \
//                            for (unsigned i = 0; i < elem.size(); ++i) {                                            \
//                                entries[i] = (type)elem[i];                                                         \
//                            }                                                                                       \
//                            ((type **)adr)[0] = entries;                                                            \
//                            adr += sizeof(type *);                                                                  \
//                        }
//
//#define add_vector_type_char(type) \
//                        if (ln_dt == #type) {                                                                       \
//                            ((uint32_t *)adr)[0] = (uint32_t)elem.size();                                           \
//                            adr += 4;                                                                               \
//                            \
//                            ln_vector_t* entries = new ln_vector_t[elem.size()];                                    \
//                            to_delete.push_back((uint8_t *)entries);                                                \
//                            \
//                            for (unsigned i = 0; i < elem.size(); ++i) {                                            \
//                                string entry = elem[i];                                                             \
//                                entries[i].len = entry.length();                                                    \
//                                entries[i].val = (const uint8_t *)(strdup(entry.c_str()));                          \
//                                to_delete.push_back((uint8_t *)entries[i].val);                                     \
//                            }                                                                                       \
//                            ((ln_vector_t **)adr)[0] = entries;                                                     \
//                            adr += sizeof(void*);                                                                   \
//                        }
//
//                        add_vector_type(uint64_t);
//                        add_vector_type(int64_t);
//                        add_vector_type(uint32_t);
//                        add_vector_type(int32_t);
//                        add_vector_type(uint16_t);
//                        add_vector_type(int16_t);
//                        add_vector_type(uint8_t);
//                        add_vector_type(int8_t);
//                        add_vector_type(float);
//                        add_vector_type(double);
//                        add_vector_type_char(char*);
//                    }
//                } else if (ends_with(ln_dt, string("*"))) {
//                    ((uint32_t *)adr)[0] = service_response[i++];
//                    adr += 4;
//
//#define push_back_type(type) \
//                    if (ln_dt == #type) {                                               \
//                        ((type*)adr)[0] = service_response[i++]; \
//                        adr += sizeof(type);                                            \
//                    }
//
//                    push_back_type(uint64_t*);
//                    push_back_type(int64_t*);
//                    push_back_type(uint32_t*);
//                    push_back_type(int32_t*);
//                    push_back_type(uint16_t*);
//                    push_back_type(int16_t*);
//                    push_back_type(uint8_t*);
//                    push_back_type(int8_t*);
//                    push_back_type(float*);
//                    push_back_type(double*);
//                } else {
//                    push_back_type(uint64_t);
//                    push_back_type(int64_t);
//                    push_back_type(uint32_t);
//                    push_back_type(int32_t);
//                    push_back_type(uint16_t);
//                    push_back_type(int16_t);
//                    push_back_type(uint8_t);
//                    push_back_type(int8_t);
//                    push_back_type(float);
//                    push_back_type(double);
//                }
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

