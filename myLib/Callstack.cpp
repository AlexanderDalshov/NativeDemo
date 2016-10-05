#include "Callstack.h"

#include <iomanip>
#include <cstring>
#include <cassert>

#define BACKTRACE_GCC    1
#define BACKTRACE_UNWIND 2
#define BACKTRACE_WIN    3

#ifndef BACKTRACE_WAY

#if defined(_WIN32)
#define BACKTRACE_WAY BACKTRACE_WIN
#elif defined(__APPLE__)
#define BACKTRACE_WAY BACKTRACE_GCC
#elif defined(__unix__)
#define BACKTRACE_WAY BACKTRACE_UNWIND
#endif

#endif // BACKTRACE_WAY

#define SYMBOLIZATION_GCC      1
#define SYMBOLIZATION_MANUAL   2
#define SYMBOLIZATION_WIN      3

#ifndef SYMBOLIZATION_WAY

#if defined(_WIN32)
#define SYMBOLIZATION_WAY SYMBOLIZATION_WIN
#elif defined(__APPLE__)
#define SYMBOLIZATION_WAY SYMBOLIZATION_GCC
#elif defined(__unix__)
#define SYMBOLIZATION_WAY SYMBOLIZATION_MANUAL
#endif

#endif // SYMBOLIZATION_WAY

#ifndef NO_CXXABI
#if defined(HAVE_CXXABI_H)
#define NO_CXXABI 0
#else // defined(HAVE_CXXABI_H)
#define NO_CXXABI 1
#endif // defined(HAVE_CXXABI_H)
#endif // NO_CXXABI

#ifndef NO_DLFCN
#if defined(_WIN32)
#define NO_DLFCN 1
#else // defined(_WIN32)
#define NO_DLFCN 0
#endif // defined(_WIN32)
#endif // NO_DLFCN

#ifndef NO_PROCFS
#if defined(__linux__)
#define NO_PROCFS 0
#else // defined(__linux__)
#define NO_PROCFS 1
#endif // defined(__linux__)
#endif // NO_PROCFS

#ifndef USE_HASH
#if BACKTRACE_WAY == BACKTRACE_WIN
#define USE_HASH 1
#else // BACKTRACE_WAY == BACKTRACE_WIN
#define USE_HASH 0
#endif // BACKTRACE_WAY == BACKTRACE_WIN
#endif // USE_HASH

#if BACKTRACE_WAY == BACKTRACE_WIN
#include <cstdint>
typedef std::uint64_t HashType;
#define NTDDI_VERSION NTDDI_WINXPSP2
#include <Windows.h>
#include <WinNT.h>
#endif // BACKTRACE_WAY == BACKTRACE_WIN

#if BACKTRACE_WAY == BACKTRACE_GCC || SYMBOLIZATION_WAY == SYMBOLIZATION_GCC
#include <execinfo.h>
#endif // BACKTRACE_WAY == BACKTRACE_GCC || SYMBOLIZATION_WAY == SYMBOLIZATION_GCC

#if BACKTRACE_WAY == BACKTRACE_UNWIND
#include <unwind.h>
#endif // BACKTRACE_WAY == BACKTRACE_UNWIND

#if SYMBOLIZATION_WAY == SYMBOLIZATION_WIN
#include <mutex>
#include <DbgHelp.h>
#pragma comment(lib, "Dbghelp")
#endif // SYMBOLIZATION_WAY == SYMBOLIZATION_WIN

#if !NO_PROCFS
#include <functional>
#include <fstream>
#include <list>
#endif // !NO_PROCFS

#if !NO_CXXABI
#include <cxxabi.h>
#endif // !NO_CXXABI

#if !NO_DLFCN
#include <dlfcn.h>
#endif // !NO_DLFCN

namespace
{

#if BACKTRACE_WAY == BACKTRACE_GCC

std::size_t Backtrace(std::size_t ignore, void** addresses, std::size_t limit)
{
    ++ignore;

    std::size_t stackSize = stackSize = ignore + limit;
    std::unique_ptr<void*[]> buf(new void*[stackSize]);
    stackSize = ::backtrace(buf.get(), (int)stackSize);
    
    if (ignore >= stackSize)
    {
        return 0;
    }
    stackSize -= ignore;
    std::memcpy(addresses, buf.get() + ignore, stackSize * sizeof(void*));
    return stackSize;
}

#elif BACKTRACE_WAY == BACKTRACE_UNWIND

struct Tracer
{
    std::size_t Ignore;
    void** Curr;
    void** End;

    Tracer(void** begin, std::size_t ignore, std::size_t limit) :
        Ignore(ignore),
        Curr(begin),
        End(begin + limit)
    {
    }

    static _Unwind_Reason_Code trace(_Unwind_Context *context, void *arg)
    {
        Tracer* tracer = static_cast<Tracer*>(arg);
        if (tracer->Ignore)
        {
            --tracer->Ignore;
        }
        else if (tracer->Curr != tracer->End)
        {
            if ((*tracer->Curr = (void*)_Unwind_GetIP(context)) != 0)
            {
                ++tracer->Curr;
            }
            if (tracer->Curr == tracer->End)
            {
                return _URC_END_OF_STACK;
            }
        }
        return _URC_NO_REASON;
    }
};

std::size_t Backtrace(std::size_t ignore, void** addresses, std::size_t limit)
{
    Tracer tracer(addresses, ++ignore, limit);
    _Unwind_Backtrace(Tracer::trace, (void*)&tracer);
    std::size_t stackSize = tracer.Curr - addresses;
    return stackSize;
}

#elif BACKTRACE_WAY == BACKTRACE_WIN

std::size_t Backtrace(std::size_t ignore, void** addresses, std::size_t limit, HashType* hash = nullptr)
{
    DWORD hashBuffer = 0;
    WORD captured = ::RtlCaptureStackBackTrace(static_cast<DWORD>(++ignore), static_cast<DWORD>(limit), addresses, &hashBuffer);
    if (hash != nullptr)
    {
        *hash = hashBuffer;
    }
    return captured;
}

#endif // BACKTRACE_WAY

class Demangler
{
public:
    explicit Demangler(char const* mangled) :
        m_demangled(mangled)
#if !NO_CXXABI
        ,
        m_guard(nullptr, std::free)
#endif
    {
#if !NO_CXXABI
        if (mangled != nullptr)
        {
            int status = 0;
            m_guard.reset(abi::__cxa_demangle(mangled, 0, 0, &status));
            if (status == 0 && m_guard != nullptr)
            {
                m_demangled = m_guard.get();
            }
        }
#endif
    }

    char const* Name() const
    {
        return m_demangled;
    }

private:
    char const* m_demangled;
#if !NO_CXXABI
    typedef std::unique_ptr<char, void(*)(void*)> Guard;
    Guard m_guard;
#endif
};


#if SYMBOLIZATION_WAY == SYMBOLIZATION_WIN


typedef std::mutex MutexType;
typedef std::lock_guard<MutexType> LockType;

LockType SymbolizeGuard()
{
    struct Initializer
    {
        Initializer()
        {
            LockType lock(Mutex);
            ::SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME);
            ::SymInitialize(::GetCurrentProcess(), NULL, TRUE);
        }
        ~Initializer()
        {
            LockType lock(Mutex);
            ::SymCleanup(::GetCurrentProcess());
        }
        MutexType Mutex;
    };
    static Initializer initializer;
    return LockType(initializer.Mutex);
}

class SymbolInfo : public SYMBOL_INFO
{
public:
    SymbolInfo() :
        m_buffer()
    {
        MaxNameLen = sizeof(m_buffer) + 1;
        SizeOfStruct = sizeof(SYMBOL_INFO);
    }

private:
    char m_buffer[255];
};

#elif SYMBOLIZATION_WAY == SYMBOLIZATION_GCC

bool FindFunction(char const* const* symbolized, char const** begin, char const** end)
{
    char const* entry = *symbolized;

    char const* endName = nullptr;
    char const* beginName = nullptr;

#if defined(__APPLE__)
    // 1 module      0x00006989 function + 111
    int spaceCounter = 0;
    bool spaceFound = false;
    bool objectiveC = false;
    for (endName = entry; *endName; ++endName)
    {
        if (*endName == ' ')
        {
            if (!spaceFound)
            {
                spaceFound = true;
                if (++spaceCounter == 4 && !objectiveC)
                {
                    break;
                }
            }
        }
        // for Objective C names like
        // 1 module      0x00006989 -[CCDirectorCaller doCaller:] + 37
        else if (*endName == '[')
        {
            objectiveC = true;
        }
        else if (*endName == ']')
        {
            break;
        }
        else if (spaceFound)
        {
            spaceFound = false;
            if (spaceCounter == 3)
            {
                beginName = endName;
            }
        }
    }
#else // __APPLE__
    // ./module(function+0x15c) [0x8048a6d]
    for (endName = entry; *endName; ++endName)
    {
        if (*endName == '(')
        {
            beginName = endName + 1;
        }
        else if (*endName == '+')
        {
            break;
        }
    }
#endif // __APPLE__
    if (beginName != nullptr && beginName < endName)
    {
        if (begin != nullptr)
        {
            *begin = beginName;
        }
        if (end != nullptr)
        {
            *end = endName;
        }
        return true;
    }
    return false;
}

class GccSymbolizer
{
public:
    explicit GccSymbolizer(void const* address) :
        // const_cast due of incorrect backtrace_symbols interface
        m_symbolized(backtrace_symbols(const_cast<void*const*>(&address), 1), std::free),
        m_functionBegin(nullptr),
        m_functionEnd(nullptr),
        m_function()
    {
        if (FindFunction(m_symbolized.get(), &m_functionBegin, &m_functionEnd))
        {
            m_function.assign(m_functionBegin, m_functionEnd);
        }
    }

    char const* Function() const
    {
        return m_function.empty() ? nullptr : m_function.c_str();
    }

    bool Ouput(std::ostream& stream) const
    {
        if (m_symbolized == nullptr || *m_symbolized == nullptr)
        {
            return false;
        }

        if (m_functionBegin != nullptr && m_functionEnd != nullptr)
        {
            for (char const* c = *m_symbolized; c != m_functionBegin; ++c)
            {
                stream << *c;
            }
            stream << Demangler(Function()).Name() << m_functionEnd;
        }
        else
        {
            stream << *m_symbolized;
        }
        return true;
    }

private:
    typedef std::unique_ptr<char*, void(*)(void*)> Guard;
    Guard m_symbolized;
    char const* m_functionBegin;
    char const* m_functionEnd;
    std::string m_function;
};

#endif // SYMBOLIZATION_WAY == SYMBOLIZATION_GCC

#if !NO_PROCFS
class ProcFsMapping
{
    struct Region
    {
        void const* Start;
        void const* End;
        std::shared_ptr<char> ModuleName;

        Region() :
            Start(),
            End(),
            ModuleName()
        {
        }
    };

    typedef std::list<Region> Regions;
    Regions m_regions;
public:
    ProcFsMapping()
    {
        std::stringstream pathStream;
        pathStream << "/proc/" << getpid() << "/maps";
        std::ifstream fileStream(pathStream.str().c_str());
        while (fileStream.good())
        {
            std::string line;
            std::getline(fileStream, line);
            Region region;
            char executable = '\0';
            char* moduleName = nullptr;
            // The format of each line is:
            // address           perms offset  dev   inode   pathname
            // 08048000-08056000 r-xp 00000000 03:0c 64593   /usr/sbin/gpm;
            // where "address" is the address space in the process that it occupies,
            // "perms" is a set of permissions (r=read, w=write, x=execute,s=shared,p=private)
            // "offset" is the offset into the file/whatever,
            // "dev" is the device(major:minor),
            // and "inode" is the inode on that device.
            // In format below %*X sais to ignore field, %*[ ] sais ignore any number of spaces,
            // %ms sais to allocate required string size.
            int count = sscanf(line.c_str(), "%p-%p %*c%*c%c%*c %*p %*2x:%*2x %*u%*[ ]%ms",
                &region.Start, &region.End, &executable, &moduleName);
            region.ModuleName.reset(moduleName, std::free);
            if (count == 4 && executable == 'x' && region.ModuleName != nullptr)
            {
                m_regions.push_back(region);
            }
        }
    }

    char const* GetModuleName(void const* address) const
    {
        Regions::const_iterator it = std::find_if(m_regions.begin(), m_regions.end(),
#ifdef _STD_HAS_CPP11_SUPPORT
            [address](Region const& region) { return region.Start <= address && region.End > address; });
#else
            (std::bind(&Region::Start, _1) <= address) && (std::bind(&Region::End, _1) > address));
#endif
        if (it != m_regions.end())
        {
            return it->ModuleName.get();
        }
        return nullptr;
    }
};

#endif

inline void OutputAddress(std::ostream& stream, void const* address)
{
    std::ios_base::fmtflags oldFlags = stream.flags();
    stream << "[";
    stream << std::hex << std::setfill('0') << std::setw(sizeof(void*) << 1) << address;
    stream.flags(oldFlags);
    stream << "]";
}

inline void OutputModule(std::ostream& stream, char const* module)
{
    std::ios_base::fmtflags oldFlags = stream.flags();
    stream << std::setfill(' ') << std::setw(16) << std::left << ((module == nullptr) ? "<unknown>" : module);
    stream.flags(oldFlags);
}

inline void OutputFunction(std::ostream& stream, char const* function)
{
    stream << ((function != nullptr) ? Demangler(function).Name() : "???");
}

inline void OutputOffset(std::ostream& stream, std::size_t offset)
{
    std::ios_base::fmtflags oldFlags = stream.flags();
    stream << std::hex << std::showbase << offset;
    stream.flags(oldFlags);
}

void OutputEntryTiny(std::ostream& stream, void const* address)
{
    char const* function = nullptr;

#if !NO_DLFCN
    Dl_info info;
    info.dli_sname = nullptr;
    if (dladdr(address, &info) != 0 && info.dli_sname != nullptr)
    {
        function = info.dli_sname;
    }
#endif

#if SYMBOLIZATION_WAY == SYMBOLIZATION_GCC
    GccSymbolizer gccSymbolizer(address);
    if (gccSymbolizer.Function() != nullptr)
    {
        function = gccSymbolizer.Function();
    }
#endif

#if SYMBOLIZATION_WAY == SYMBOLIZATION_WIN
    LockType guard = SymbolizeGuard();
    SymbolInfo symbolInfo;
    DWORD64 displacement;
    if (::SymFromAddr(::GetCurrentProcess(), reinterpret_cast<DWORD64>(address), &displacement, &symbolInfo) != FALSE)
    {
        function = symbolInfo.Name;
    }
#endif // SYMBOLIZATION_WAY == SYMBOLIZATION_WIN

    if (function != nullptr)
    {
        OutputFunction(stream, function);
    }
    else
    {
        OutputAddress(stream, address);
    }
}

inline char const* FileNameFromPath(char const* path)
{
    char const* file = path;
    for (char const* p = file; *p != '\0'; ++p)
    {
        if ((*p == '\\' || *p == '/') && (*(p + 1) != '\0'))
        {
            file = p + 1;
        }
    }
    return file;
}

void OutputEntryWide(std::ostream& stream, void const* address)
{
    char const* module = nullptr;
    char const* function = nullptr;
    std::size_t offset = 0;

    char const* file = nullptr;
    int line = 0;

#if SYMBOLIZATION_WAY == SYMBOLIZATION_GCC
    GccSymbolizer symbolizer(address);
    if (symbolizer.Ouput(stream))
    {
        return;
    }
    function = symbolizer.Function();
#endif

#if !NO_DLFCN
    Dl_info info;
    info.dli_fname = nullptr;
    info.dli_sname = nullptr;
    info.dli_saddr = nullptr;
    if (dladdr(address, &info) != 0)
    {
        module = FileNameFromPath(info.dli_fname);
        if (info.dli_sname != nullptr)
        {
            function = info.dli_sname;
        }
        offset = static_cast<char const*>(address) - static_cast<char const*>(info.dli_saddr);
    }
#endif

#if !NO_PROCFS
    if (module == nullptr)
    {
        static ProcFsMapping mapping;
        module = mapping.GetModuleName(address);
    }
#endif

#if SYMBOLIZATION_WAY == SYMBOLIZATION_WIN
    LockType guard = SymbolizeGuard();
    SymbolInfo symbolInfo;
    DWORD64 displacement = 0;
    if (::SymFromAddr(::GetCurrentProcess(), reinterpret_cast<DWORD64>(address), &displacement, &symbolInfo) != FALSE)
    {
        function = symbolInfo.Name;
        offset = displacement;
    }

    DWORD dummy = 0;
    IMAGEHLP_LINE64 lineInfo = {};
    lineInfo.SizeOfStruct = sizeof(lineInfo);
    if (::SymGetLineFromAddr64(::GetCurrentProcess(), reinterpret_cast<DWORD64>(address), &dummy, &lineInfo) != FALSE)
    {
        line = lineInfo.LineNumber;
        file = FileNameFromPath(lineInfo.FileName);
    }

    IMAGEHLP_MODULE64 moduleInfo = {};
    moduleInfo.SizeOfStruct = sizeof(moduleInfo);
    if (::SymGetModuleInfo64(::GetCurrentProcess(), reinterpret_cast<DWORD64>(address), &moduleInfo) != FALSE)
    {
        module = moduleInfo.ModuleName;
    }
#endif // SYMBOLIZATION_WAY == SYMBOLIZATION_WIN

    OutputAddress(stream, address);
    stream << ' ';
    OutputModule(stream, module);
    stream << ' ';
    OutputFunction(stream, function);
    stream << " + ";
    OutputOffset(stream, offset);

    if (file != nullptr)
    {
        stream << " : " << file << "(" << line << ")";
    }
}

inline void OutputEntryImpl(std::ostream& stream, void const* address, bool tiny)
{
    tiny ? OutputEntryTiny(stream, address) : OutputEntryWide(stream, address);
}

struct ImplementationBase
{
    void* Stack[Callstack::MaxStackLimit];
    std::size_t FilledStackSize;

    ImplementationBase() :
        Stack(),
        FilledStackSize()
    {
    }

    ImplementationBase(ImplementationBase const& other) :
        Stack(),
        FilledStackSize(other.FilledStackSize)
    {
        std::memcpy(Stack, other.Stack, FilledStackSize * sizeof(void*));
    }
};

} // namespace

struct Callstack::Implementation : public ImplementationBase
{
#if USE_HASH
    HashType Hash;
#endif // USE_HASH
    Implementation(std::size_t ignore, std::size_t limit)
    {
        if (limit > MaxStackLimit)
        {
            limit = MaxStackLimit;
        }
        ++ignore;
#if USE_HASH
        FilledStackSize = Backtrace(ignore, Stack, limit, &Hash);
#else // USE_HASH
        FilledStackSize = Backtrace(ignore, Stack, limit);
#endif // USE_HASH
    }

    Implementation(Implementation const& other) :
        ImplementationBase(other)
    {
#if USE_HASH
        Hash = other.Hash;
#endif // USE_HASH
    }

    bool operator == (Implementation const& other) const
    {
#if USE_HASH
        return Hash == other.Hash;
#else // USE_HASH
        return (FilledStackSize == other.FilledStackSize) &&
            (FilledStackSize == 0 || std::memcmp(Stack, other.Stack, FilledStackSize * sizeof(void*)) == 0);
#endif // USE_HASH
    }

    bool operator < (Implementation const& other) const
    {
#if USE_HASH
        return Hash < other.Hash;
#else // USE_HASH
        return (FilledStackSize < other.FilledStackSize) ||
            (FilledStackSize == other.FilledStackSize && FilledStackSize != 0 &&
            std::memcmp(Stack, other.Stack, FilledStackSize * sizeof(void*)) < 0);
#endif // USE_HASH
    }
};

Callstack::Callstack(std::size_t ignore, std::size_t limit) :
    m_impl(new Implementation(++ignore, limit))
{
}

Callstack::Callstack(Callstack const& other) :
m_impl(new Implementation(*other.m_impl))
{
}

Callstack::~Callstack()
{
}

void const* Callstack::At(std::size_t index) const
{
    if (index >= m_impl->FilledStackSize)
    {
        return nullptr;
    }
    return m_impl->Stack[index];
}

std::size_t Callstack::Size() const
{
    return m_impl->FilledStackSize;
}

Callstack& Callstack::operator = (Callstack const& other)
{
    if (this != &other)
    {
        m_impl.reset(new Implementation(*other.m_impl));
    }
    return *this;
}

bool Callstack::operator == (Callstack const& other) const
{
    return *m_impl == *other.m_impl;
}

bool Callstack::operator < (Callstack const& other) const
{
    return *m_impl < *other.m_impl;
}

void const* Callstack::operator[] (std::size_t index) const
{
    return At(index);
}

CallstackFormat::CallstackFormat(Callstack const& callstack, bool tiny, std::size_t from, std::size_t count) :
    m_callstack(callstack),
    m_tiny(tiny),
    m_from(from),
    m_end(std::min(callstack.Size(), from + count))
{
}

void CallstackFormat::OutputEntryImpl(std::size_t index, std::ostream& stream) const
{
    assert(index >= m_from);
    assert(index < m_end);
    ::OutputEntryImpl(stream, m_callstack[index], m_tiny);
}

CallstackFormat::operator std::string() const
{
    std::stringstream stream;
    stream << *this;
    return stream.str();
}

void  CallstackFormat::Output(std::ostream& stream) const
{
    for (std::size_t i = m_from; i < m_end; ++i)
    {
        OutputEntry(i, stream);
    }
}

void CallstackFormat::OutputEntry(std::size_t index, std::ostream& stream) const
{
    stream << '\n';
    OutputEntryImpl(index, stream);
}

CallstackFormat Wide(Callstack const& callstack, std::size_t from, std::size_t count)
{
    return CallstackFormat(callstack, false, from, count);
}

CallstackFormat Tiny(Callstack const& callstack, std::size_t from, std::size_t count)
{
    return CallstackFormat(callstack, true, from, count);
}

std::ostream& operator << (std::ostream& stream, CallstackFormat const& format)
{
    format.Output(stream);
    return stream;
}
