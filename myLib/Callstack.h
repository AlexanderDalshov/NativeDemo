#pragma once

#include <iosfwd>
#include <memory>
#include <sstream>
#include <string>
#include <utility>

namespace Navionics
{
class Callstack
{
public:
    enum
    {
        MaxStackLimit = 32
    };

public:
    explicit Callstack(std::size_t ignore = 0, std::size_t limit = MaxStackLimit);
    Callstack(const Callstack& other);
    ~Callstack();

    void const* At(std::size_t index) const;
    std::size_t Size() const;

    Callstack& operator=(const Callstack& other);
    bool operator==(const Callstack& other) const;
    bool operator<(const Callstack& other) const;
    void const* operator[](std::size_t index) const;

private:
    struct Implementation;
    std::unique_ptr<Implementation> mImpl;
};

class CallstackFormat
{
public:
    CallstackFormat(const Callstack& callstack, bool tiny, std::size_t from, std::size_t count);
    operator std::string() const;

    void Output(std::ostream& stream) const;

private:
    void OutputEntry(std::size_t index, std::ostream& stream) const;
    void OutputEntryImpl(std::size_t index, std::ostream& stream) const;

private:
    Callstack const&  m_callstack;
    bool const        m_tiny;
    std::size_t const m_from;
    std::size_t const m_end;
};

CallstackFormat Wide(const Callstack& callstack, std::size_t from = 0,
                     std::size_t count = Callstack::MaxStackLimit);
CallstackFormat Tiny(const Callstack& callstack, std::size_t from = 0,
                     std::size_t count = Callstack::MaxStackLimit);
}

std::ostream& operator<<(std::ostream& stream, const Navionics::CallstackFormat& format);
