#pragma once

inline void fatalError(const std::string_view message)
{
    throw std::runtime_error(message.data());
}

inline std::string hresultToString(const HRESULT hr)
{
    char str[128]{};
    sprintf_s(str, "HRESULT : 0x%08X", static_cast<UINT>(hr));
    return std::string(str);
}

inline void throwIfFailed(const HRESULT hr)
{
    if (FAILED(hr))
    {
        throw std::runtime_error(hresultToString(hr));
    }
}

inline std::wstring stringToWstring(const std::string_view inputString)
{
    std::wstring result{};
    const std::string input{inputString};

    const int32_t length = MultiByteToWideChar(CP_UTF8, 0, input.c_str(), -1, NULL, 0);
    if (length > 0)
    {
        result.resize(size_t(length) - 1);
        MultiByteToWideChar(CP_UTF8, 0, input.c_str(), -1, result.data(), length);
    }

    return std::move(result);
}

inline std::string wstringToString(const std::wstring_view inputWString)
{
    std::string result{};
    const std::wstring input{inputWString};

    const int32_t length = WideCharToMultiByte(CP_UTF8, 0, input.c_str(), -1, NULL, 0, NULL, NULL);
    if (length > 0)
    {
        result.resize(size_t(length) - 1);
        WideCharToMultiByte(CP_UTF8, 0, input.c_str(), -1, result.data(), length, NULL, NULL);
    }

    return std::move(result);
}