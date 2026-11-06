#pragma once

#include <chrono>
#include <string>
#include <vector>

namespace torture {

class ChildProcess {
public:
    ChildProcess();
    ~ChildProcess();

    ChildProcess(const ChildProcess&) = delete;
    ChildProcess& operator=(const ChildProcess&) = delete;

    bool start(const std::string& executable, const std::vector<std::string>& arguments,
               std::string& error);
    bool write_line(const std::string& line, std::string& error);
    bool read_line(std::string& line, std::chrono::milliseconds timeout, bool& eof,
                   std::string& error);
    bool close_input(std::string& error);
    bool wait(std::chrono::milliseconds timeout, int& exit_code, std::string& error);
    bool terminate(std::string& error);

private:
    struct Impl;
    Impl* impl_;
};

} // namespace torture
