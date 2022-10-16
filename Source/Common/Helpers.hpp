#pragma once

inline std::wstring StringToWstring(const std::string_view inputString)
{
	std::wstring result{};
	const std::string input{ inputString };

	const int32_t length = MultiByteToWideChar(CP_UTF8, 0, input.c_str(), -1, NULL, 0);
	if (length > 0)
	{
		result.resize(size_t(length) - 1);
		MultiByteToWideChar(CP_UTF8, 0, input.c_str(), -1, result.data(), length);
	}

	return std::move(result);
}

inline std::string WstringToString(const std::wstring_view inputWString)
{
	std::string result{};
	const std::wstring input{ inputWString };

	const int32_t length = WideCharToMultiByte(CP_UTF8, 0, input.c_str(), -1, NULL, 0, NULL, NULL);
	if (length > 0)
	{
		result.resize(size_t(length) - 1);
		WideCharToMultiByte(CP_UTF8, 0, input.c_str(), -1, result.data(), length, NULL, NULL);
	}

	return std::move(result);
}

inline void FatalError(const std::wstring_view message)
{
	MessageBoxW(nullptr, message.data(), L"ERROR!", MB_OK | MB_ICONEXCLAMATION);
	throw std::runtime_error(WstringToString(message));
}

inline void ThrowIfFailed(const HRESULT hr)
{
	if (FAILED(hr))
	{
		throw std::runtime_error("");
	}
}

template <typename T>
static inline constexpr typename std::underlying_type<T>::type EnumClassValue(const T& value)
{
	return static_cast<std::underlying_type<T>::type>(value);
}