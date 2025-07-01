//
// SPDX-License-Identifier: LGPL-2.1-or-later
//
// Copyright 2025 ANSSI. All Rights Reserved.
//
// Author(s): fabienfl (ANSSI)
//
#pragma once

#include <Windows.h>

#include <cassert>
#include <memory>
#include <system_error>

namespace Guard {

template<typename T>
class DescriptorGuard
{
public:
    DescriptorGuard(T data, T invalidValue)
        : m_data(data)
        , m_invalidValue(invalidValue)
    {}

    DescriptorGuard(const DescriptorGuard &) = delete;
    DescriptorGuard &operator=(const DescriptorGuard &) = delete;

    DescriptorGuard(DescriptorGuard &&o) noexcept
        : m_data(std::move(o.m_data))
        , m_invalidValue(std::move(o.m_invalidValue))
    {
        o.m_data = m_invalidValue;
    }

    DescriptorGuard &operator=(DescriptorGuard &&o) noexcept
    {
        if (this != &o) {
            m_data = std::move(o.m_data);
            m_invalidValue = o.m_invalidValue;
            o.m_data = m_invalidValue;
        }

        return *this;
    }

    const T &value() const { return m_data; }
    T &value() { return m_data; }

    const T &operator*() const { return m_data; }
    T &operator*() { return m_data; }

    T *data() { return &m_data; }
    const T *data() const { return &m_data; }

    T release()
    {
        auto descriptor = std::move(m_data);
        m_data = m_invalidValue;
        return descriptor;
    }

    bool IsValid() const { return m_data != m_invalidValue; }

    explicit operator bool() const { return m_data != m_invalidValue; }
    bool operator==(const T &value) const { return m_data == value; }
    bool operator==(const DescriptorGuard<T> &o) const { return m_data == o.m_data; }

protected:
    ~DescriptorGuard() = default;

    T m_data;
    T m_invalidValue;
};

class Module final : public DescriptorGuard<HMODULE>
{
public:
    Module(HMODULE module = NULL)
        : DescriptorGuard(module, NULL)
    {}

    Module(Module &&o) noexcept = default;
    Module &operator=(Module &&o) = default;

    ~Module()
    {
        if (m_data == m_invalidValue) {
            return;
        }

        if (::FreeLibrary(m_data) == FALSE) {
            assert(0 && "Failed FreeLibrary");
        }
    }
};

class Bstr
{
public:
    Bstr() = default;
    explicit Bstr(BSTR bstr)
        : m_bstr(bstr)
    {}

    Bstr(const Bstr &) = delete;
    Bstr &operator=(const Bstr &) = delete;

    Bstr(Bstr &&o) noexcept
        : m_bstr(o.m_bstr)
    {
        o.m_bstr = nullptr;
    }
    Bstr &operator=(Bstr &&o) noexcept
    {
        if (m_bstr != o.m_bstr) {
            reset();
            m_bstr = o.m_bstr;
            o.m_bstr = nullptr;
        }
        return *this;
    }

    ~Bstr() { reset(); }

    BSTR *addressOf() { return &m_bstr; }
    BSTR get() const { return m_bstr; }
    explicit operator bool() const { return m_bstr != nullptr; }

    void reset()
    {
        if (m_bstr) {
            ::SysFreeString(m_bstr);
            m_bstr = nullptr;
        }
    }

private:
    BSTR m_bstr = nullptr;
};

class BeginUpdateResourceHandleW
{
    struct Context
    {
        HANDLE handle;
    };

public:
    BeginUpdateResourceHandleW(HANDLE handle = nullptr)
        : m_context({handle})
        , m_ptr(&m_context, Deleter)
    {}

    static void Deleter(Context *context)
    {
        if (context->handle == nullptr) {
            return;
        }

        if (EndUpdateResourceW(context->handle, FALSE) == FALSE) {
            assert(0 && "Failed EndUpdateResourceW");
        }
    }

    HANDLE detach()
    {
        if (m_context.handle == nullptr) {
            return nullptr;
        }

        auto handle = m_context.handle;
        m_context.handle = nullptr;
        return handle;
    }

    HANDLE value() const { return m_context.handle; }
    operator bool() const { return m_context.handle != nullptr; }
    bool operator==(const BeginUpdateResourceHandleW &o) const
    {
        return m_context.handle == o.m_context.handle;
    }

private:
    struct Context m_context;
    std::shared_ptr<void> m_ptr;
};

template<typename T>
class ComPtr
{
    struct IUnknown
    {
        virtual HRESULT __stdcall QueryInterface(void **pp) = 0;
        virtual ULONG __stdcall AddRef(void) = 0;
        virtual ULONG __stdcall Release(void) = 0;
    };

public:
    ComPtr()
        : ComPtr(nullptr)
    {}

    ComPtr(T *com)
        : m_ptr(com)
    {
        AddRef();
    }

    ComPtr(const ComPtr &other)
    {
        m_ptr = other.m_ptr;
        AddRef();
    }

    ComPtr(ComPtr &&other) noexcept
    {
        m_ptr = other.m_ptr;
        other.m_ptr = nullptr;
    }

    ComPtr &operator=(const ComPtr &other)
    {
        if (&other == this) {
            return *this;
        }

        Release();
        m_ptr = other.m_ptr;
        AddRef();
        return *this;
    }

    ~ComPtr() { Release(); }

    T *get() const { return m_ptr; }

    T *detach()
    {
        auto ptr = m_ptr;
        m_ptr = nullptr;
        return ptr;
    }

    T **operator&()
    {
        Release();
        return &m_ptr;
    }

    T *operator->() const { return m_ptr; }

    operator bool() const { return m_ptr != nullptr; }

private:
    void AddRef()
    {
        if (m_ptr == nullptr) {
            return;
        }

        auto p = reinterpret_cast<IUnknown *>(m_ptr);
        p->AddRef();
    }

    void Release()
    {
        if (m_ptr == nullptr) {
            return;
        }

        auto p = reinterpret_cast<IUnknown *>(m_ptr);
        p->Release();
        m_ptr = nullptr;
    }

    T *m_ptr = nullptr;
};

} // namespace Guard
