// Copyright (c) 2026 CORDEL contributors. MIT.
// Small JSON values; bounded strict decoding lives in json.cpp.
#pragma once
#include <cmath>
#include <iomanip>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

namespace cordel {
class Json {
public:
    using Object=std::map<std::string,Json>;
    using Array=std::vector<Json>;
private:
    std::variant<std::nullptr_t,bool,double,std::string,Array,Object> data_{nullptr};
    static std::string quote(const std::string& s) {
        std::ostringstream out; out<<'"';
        for(unsigned char c:s) {
            if(c=='"'||c=='\\') out<<'\\'<<c;
            else if(c<32) out<<"\\u"<<std::hex<<std::setw(4)<<std::setfill('0')<<int(c)<<std::dec;
            else out<<c;
        }
        out<<'"'; return out.str();
    }
public:
    Json()=default;
    Json(bool v):data_(v) {}
    Json(double v):data_(v) { if(!std::isfinite(v)) throw std::invalid_argument("Non-finite JSON number"); }
    Json(int v):Json(double(v)) {}
    Json(unsigned v):Json(double(v)) {}
    Json(std::size_t v):Json(double(v)) {}
    Json(const char* v):data_(std::string(v)) {}
    Json(std::string v):data_(std::move(v)) {}
    Json(Array v):data_(std::move(v)) {}
    Json(Object v):data_(std::move(v)) {}
    static Json parse(const std::string& text);
    bool is(const std::string& kind) const {
        if(kind=="object") return std::holds_alternative<Object>(data_);
        if(kind=="array") return std::holds_alternative<Array>(data_);
        if(kind=="string") return std::holds_alternative<std::string>(data_);
        if(kind=="boolean") return std::holds_alternative<bool>(data_);
        if(kind=="number"||kind=="integer") {
            auto p=std::get_if<double>(&data_);
            return p&&(kind=="number"||(*p==std::floor(*p)&&std::abs(*p)<=9007199254740991.));
        }
        return false;
    }
    const Object& object() const { return std::get<Object>(data_); }
    const Array& array() const { return std::get<Array>(data_); }
    const std::string& string() const { return std::get<std::string>(data_); }
    double number() const { return std::get<double>(data_); }
    bool boolean() const { return std::get<bool>(data_); }
    bool contains(const std::string& key) const { return is("object")&&object().contains(key); }
    const Json& at(const std::string& key) const { return object().at(key); }
    std::string dump() const {
        return std::visit([](const auto& v)->std::string {
            using T=std::decay_t<decltype(v)>;
            if constexpr(std::is_same_v<T,std::nullptr_t>) return "null";
            else if constexpr(std::is_same_v<T,bool>) return v?"true":"false";
            else if constexpr(std::is_same_v<T,double>) { std::ostringstream o; o<<std::setprecision(17)<<v; return o.str(); }
            else if constexpr(std::is_same_v<T,std::string>) return quote(v);
            else {
                std::string s=std::is_same_v<T,Array>?"[":"{";
                bool first=true;
                for(const auto& item:v) {
                    if(!first) s+=",";
                    first=false;
                    if constexpr(std::is_same_v<T,Array>) s+=item.dump();
                    else s+=quote(item.first)+":"+item.second.dump();
                }
                return s+(std::is_same_v<T,Array>?"]":"}");
            }
        },data_);
    }
};
}
