#pragma once

#include <iosfwd>
#include <string>
#include <memory>
#include <utility>
#include <sstream>

class Callstack
{
public:
    enum
    {
        MaxStackLimit = 32
    };

public:
    explicit Callstack(std::size_t ignore = 0, std::size_t limit = MaxStackLimit);
    Callstack(Callstack const& other);
    ~Callstack();

    void const* At(std::size_t index) const;
    std::size_t Size() const;

    Callstack& operator = (Callstack const& other);
    bool operator == (Callstack const& other) const;
    bool operator < (Callstack const& other) const;
    void const* operator[] (std::size_t index) const;

private:
    struct Implementation;
    std::unique_ptr<Implementation> m_impl;
};

class CallstackFormat
{
public:
    CallstackFormat(Callstack const& callstack, bool tiny, std::size_t from, std::size_t count);
    operator std::string() const;

    void Output(std::ostream& stream) const;

    template <typename Logger>
    void Output(typename Logger::Level level) const;
private:
    void OutputEntry(std::size_t index, std::ostream& stream) const;
    template <typename Logger>
    void OutputEntry(std::size_t index, typename Logger::Level level) const;

    void OutputEntryImpl(std::size_t index, std::ostream& stream) const;

private:
    Callstack const& m_callstack;
    bool const m_tiny;
    std::size_t const m_from;
    std::size_t const m_end;
};

template <typename Logger>
void CallstackFormat::Output(typename Logger::Level level) const
{
    for (std::size_t i = m_from; i < m_end; ++i)
    {
        OutputEntry<Logger>(i, level);
    }
}

template <typename Logger>
void CallstackFormat::OutputEntry(std::size_t index, typename Logger::Level level) const
{
    std::stringstream stream;
    OutputEntryImpl(index, stream);
    Logger(level) << stream.str();
}

CallstackFormat Wide(Callstack const& callstack, std::size_t from = 0, std::size_t count = Callstack::MaxStackLimit);
CallstackFormat Tiny(Callstack const& callstack, std::size_t from = 0, std::size_t count = Callstack::MaxStackLimit);

std::ostream& operator << (std::ostream& stream, CallstackFormat const& format);
