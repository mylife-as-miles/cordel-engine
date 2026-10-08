// Copyright (c) 2026 CORDEL contributors. MIT. Linux/POSIX spike transport.
#include "cordel/narrative.hpp"
#include <algorithm>
#include <utility>
#include <chrono>
#include <csignal>
#include <fcntl.h>
#include <poll.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
extern char** environ;

namespace cordel::narrative {
namespace {
double now() {return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();}
void close_fd(int& fd) {if(fd>=0) {::close(fd);fd=-1;}}
}
Client::Client(const std::filesystem::path& python,const std::filesystem::path& bootstrap,
               const std::filesystem::path& schema,const std::filesystem::path& output):protocol_(schema) {
    std::filesystem::create_directories(output);
    int input[2]{-1,-1},result[2]{-1,-1};
    int log=-1;
    posix_spawn_file_actions_t actions;
    bool initialized=false;
    try {
        if(pipe2(input,O_CLOEXEC)||pipe2(result,O_CLOEXEC)) throw std::runtime_error("Narrative pipe creation failed");
        log=open((output/"worker-stderr.txt").c_str(),O_WRONLY|O_CREAT|O_TRUNC|O_CLOEXEC,0600);
        if(log<0) throw std::runtime_error("Narrative stderr file creation failed");
        if(posix_spawn_file_actions_init(&actions)) throw std::runtime_error("Spawn actions failed");
        initialized=true;
        posix_spawn_file_actions_adddup2(&actions,input[0],STDIN_FILENO);
        posix_spawn_file_actions_adddup2(&actions,result[1],STDOUT_FILENO);
        posix_spawn_file_actions_adddup2(&actions,log,STDERR_FILENO);
        std::string executable=std::filesystem::absolute(python).string(),script=std::filesystem::absolute(bootstrap).string();
        std::vector<std::string> env;
        for(char** e=environ;*e;++e) if(std::string(*e).rfind("CORDEL_WORKER_TRACE=",0)!=0) env.emplace_back(*e);
        env.push_back("CORDEL_WORKER_TRACE="+std::filesystem::absolute(output/"worker-trace.jsonl").string());
        std::vector<char*> envp;for(auto& e:env) envp.push_back(e.data());envp.push_back(nullptr);
        char* argv[]{executable.data(),script.data(),nullptr};
        int rc=posix_spawn(&pid_,executable.c_str(),&actions,nullptr,argv,envp.data());
        if(rc) {pid_=-1;throw std::runtime_error("Narrative posix_spawn failed: "+std::to_string(rc));}
        posix_spawn_file_actions_destroy(&actions);initialized=false;
        close_fd(input[0]);close_fd(result[1]);close_fd(log);
        write_=std::exchange(input[1],-1);read_=std::exchange(result[0],-1);
        if(fcntl(read_,F_SETFL,O_NONBLOCK)||fcntl(write_,F_SETFL,O_NONBLOCK)) throw std::runtime_error("Nonblocking transport setup failed");
        // A dead worker must produce EPIPE, never terminate the authoritative host.
        struct sigaction ignore{};ignore.sa_handler=SIG_IGN;sigemptyset(&ignore.sa_mask);sigaction(SIGPIPE,&ignore,nullptr);
        io_=std::thread(&Client::loop,this);
    } catch(...) {
        if(initialized) posix_spawn_file_actions_destroy(&actions);
        for(auto& fd:input) close_fd(fd);
        for(auto& fd:result) close_fd(fd);
        close_fd(log);
        close();throw;
    }
}
Client::~Client() {close();}
void Client::fail(std::string error) {
    std::lock_guard lock(mutex_);if(error_.empty()) error_=std::move(error);stopping_=true;
}
std::string Client::error() const {std::lock_guard lock(mutex_);return error_;}
bool Client::enqueue(const Json& m) {protocol_.validate(m);return send_raw(m.dump()+"\n");}
bool Client::send_raw(std::string line) {
    std::lock_guard lock(mutex_);
    if(line.size()>max_line+1||outgoing_.size()>=queue_limit) {
        if(error_.empty()) error_="outgoing queue/framing limit exceeded";
        stopping_=true;return false;
    }
    if(stopping_||dead_) return false;
    outgoing_.push_back(std::move(line));out_high_water_=std::max(out_high_water_,outgoing_.size());return true;
}
Json Client::send(std::string type,Json::Object payload,std::string session,std::string correlation) {
    auto seq=++sequence_;
    Json::Object fields{{"protocol_version",version},{"session_id",std::move(session)},
        {"message_id","n-"+std::to_string(seq)},{"sequence",seq},{"type",std::move(type)},{"payload",std::move(payload)}};
    if(!correlation.empty()) fields["correlation_id"]=std::move(correlation);
    Json m(fields);enqueue(m);return m;
}
std::vector<Json> Client::drain(std::size_t limit) {
    std::lock_guard lock(mutex_);std::vector<Json> messages;
    while(!incoming_.empty()&&messages.size()<limit) {messages.push_back(std::move(incoming_.front()));incoming_.pop_front();}
    return messages;
}
void Client::loop() {
    std::string buffer,current;std::size_t offset=0;
    try {
        while(!stopping_) {
            if(current.empty()) {
                std::lock_guard lock(mutex_);
                if(!outgoing_.empty()) {current=std::move(outgoing_.front());outgoing_.pop_front();offset=0;}
            }
            pollfd fds[2]{{read_,POLLIN,0},{write_,short(current.empty()?0:POLLOUT),0}};
            int polled=::poll(fds,2,10);
            if(polled<0) {if(errno==EINTR) continue;throw std::runtime_error("Narrative poll failed");}
            if(!current.empty()&&(fds[1].revents&(POLLOUT|POLLERR|POLLHUP))) {
                auto n=::write(write_,current.data()+offset,current.size()-offset);
                if(n>0) {offset+=static_cast<std::size_t>(n);if(offset==current.size()) current.clear();}
                else if(n<0&&errno!=EAGAIN&&errno!=EINTR) throw std::runtime_error("Narrative pipe write failed");
            }
            if(fds[0].revents&(POLLIN|POLLHUP|POLLERR)) {
                char chunk[4096];auto n=::read(read_,chunk,sizeof(chunk));
                if(n==0) {if(!buffer.empty()) fail("Worker ended with partial JSONL frame");break;}
                if(n<0) {if(errno==EAGAIN||errno==EINTR) continue;throw std::runtime_error("Narrative pipe read failed");}
                buffer.append(chunk,static_cast<std::size_t>(n));
                std::size_t newline;
                while((newline=buffer.find('\n'))!=std::string::npos) {
                    Json message=protocol_.decode(buffer.substr(0,newline));buffer.erase(0,newline+1);
                    auto seq=static_cast<std::size_t>(message.at("sequence").number());
                    if(seq<=peer_sequence_) throw std::runtime_error("Worker sequence replay/out-of-order");
                    peer_sequence_=seq;
                    std::lock_guard lock(mutex_);
                    if(incoming_.size()>=queue_limit) throw std::runtime_error("incoming queue overflow (64 messages)");
                    incoming_.push_back(std::move(message));high_water_=std::max(high_water_,incoming_.size());
                }
                if(buffer.size()>max_line) throw std::runtime_error("Worker oversized JSONL frame");
            }
        }
    } catch(const std::exception& e) {fail(e.what());}
    {std::lock_guard lock(mutex_);death_time_=now();}
    dead_=true;
}
void Client::terminate_for_test() {if(pid_>0) kill(pid_,SIGKILL);}
void Client::terminate_protocol_failure() {
    protocol_termination_=true;
    if(pid_>0) kill(pid_,SIGKILL);
}
void Client::close() {
    if(pid_>0) {
        if(!dead_&&!stopping_) send("shutdown");
        double deadline=now()+1.;int status=0;bool reaped=false;
        while(now()<deadline) {
            auto result=waitpid(pid_,&status,WNOHANG);
            if(result==pid_||(result<0&&errno==ECHILD)) {reaped=true;break;}
            if(result<0&&errno!=EINTR) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        if(!reaped) {forced_=true;kill(pid_,SIGKILL);while(waitpid(pid_,&status,0)<0&&errno==EINTR) {}}
        exit_status_=status;pid_=-1;
    }
    stopping_=true;if(io_.joinable()) io_.join();close_fd(read_);close_fd(write_);
    std::lock_guard lock(mutex_);incoming_.clear();outgoing_.clear();
}
Json Client::diagnostics() const {
    std::lock_guard lock(mutex_);
    return Json::Object{{"queue_limit",queue_limit},{"maximum_line_bytes",max_line},{"incoming_high_water",high_water_},
        {"outgoing_high_water",out_high_water_},{"incoming_size",incoming_.size()},{"outgoing_size",outgoing_.size()},
        {"error",error_},{"worker_eof",bool(dead_)},{"io_exit_monotonic",death_time_},{"worker_reaped",pid_==-1},
        {"forced_termination",forced_},{"protocol_failure_termination_requested",protocol_termination_},{"wait_status",exit_status_}};
}
}
