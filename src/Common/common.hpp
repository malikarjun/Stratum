#pragma once

#include "includes.hpp"

namespace stm {

using namespace std;
using namespace Eigen;
namespace fs = std::filesystem;

using fRay = ParametrizedLine<float,3>;
using dRay = ParametrizedLine<double,3>;

#pragma region misc concepts
template<typename T>
struct remove_const_tuple { using type = remove_const_t<T>; };
template<typename Tx, typename Ty>
struct remove_const_tuple<pair<Tx, Ty>> { using type = pair<remove_const_t<Tx>, remove_const_t<Ty>>; };
template<typename... Types>
struct remove_const_tuple<tuple<Types...>> { using type = tuple<remove_const_t<Types>...>; };
template<typename T> using remove_const_tuple_t = typename remove_const_tuple<T>::type;

template<typename _Type, template<typename...> typename _Template>
constexpr bool is_specialization_v = false;
template<template<class...> typename _Template, typename...Args>
constexpr bool is_specialization_v<_Template<Args...>, _Template> = true;

template<typename T> concept is_pair = is_specialization_v<T, pair>;
template<typename T> concept is_tuple = is_specialization_v<T, tuple>;

template<typename T> constexpr bool is_dynamic_span_v = false;
template<typename T> constexpr bool is_dynamic_span_v<span<T,dynamic_extent>> = true;

template<typename R> concept fixed_sized_range = is_specialization_v<R, std::array> || (is_specialization_v<R, span> && !is_dynamic_span_v<R>);
template<typename R> concept resizable_range = ranges::sized_range<R> && !fixed_sized_range<R> && requires(R r, size_t n) { r.resize(n); };

template<typename R> concept associative_range = ranges::range<R> &&
	requires { typename R::key_type; typename R::value_type; } &&
	requires(R r, ranges::range_value_t<R> v) { { r.emplace(v) } -> is_pair; };

template<typename V, typename T> concept view_of = ranges::view<V> && same_as<ranges::range_value_t<V>, T>;
template<typename R, typename T> concept range_of = ranges::range<R> && same_as<ranges::range_value_t<R>, T>;
#pragma endregion
#pragma region misc math expressions
template<unsigned_integral T>
constexpr T floorlog2i(T n) { return sizeof(T)*8 - countl_zero<T>(n) - 1; }

template<typename T> requires(is_arithmetic_v<T>)
constexpr T sign(T x) { return (x == 0) ? 0 : (x < 0) ? -1 : 1; }

template<floating_point T> constexpr T degrees(const T& r) { return r * (T)180/numbers::pi_v<float>; }
template<floating_point T> constexpr T radians(const T& d) { return d * numbers::pi_v<float>/(T)180; }

template<integral T> constexpr T align_up_mask(T value, size_t mask) { return (T)(((size_t)value + mask) & ~mask); }
template<integral T> constexpr T align_down_mask(T value, size_t mask) { return (T)((size_t)value & ~mask); }
template<integral T> constexpr T align_up(T value, size_t alignment) { return align_up_mask(value, alignment - 1); }
template<integral T> constexpr T align_down(T value, size_t alignment) { return align_down_mask(value, alignment - 1); }

template<typename T>
inline T signed_distance(const Hyperplane<T,3>& plane, const AlignedBox<T,3>& box) {
	auto normal = plane.normal();
	Hyperplane<T,3> dilatatedPlane(normal, plane.offset() - abs(box.sizes().dot(normal)));
	auto n = lerp(box.max(), box.min(), max(normal.sign(), Matrix<T,3>::Zero()));
	return dilatatedPlane.signedDistance(n);
}
#pragma endregion

constexpr unsigned long long operator"" _kB(unsigned long long x) { return x*1024; }
constexpr unsigned long long operator"" _mB(unsigned long long x) { return x*1024*1024; }
constexpr unsigned long long operator"" _gB(unsigned long long x) { return x*1024*1024*1024; }

enum class ConsoleColor {
	eBlack	= 0,
	eRed		= 1,
	eGreen	= 2,
	eBlue		= 4,
	eBold 	= 8,
	eYellow   = eRed | eGreen,
	eCyan		  = eGreen | eBlue,
	eMagenta  = eRed | eBlue,
	eWhite 		= eRed | eGreen | eBlue,
};
template<typename... Args>
inline void fprintf_color(ConsoleColor color, FILE* str, const char* format, Args&&... a) {
	#ifdef WIN32
	int c = 0;
	if ((int)color & (int)ConsoleColor::eRed) 	c |= FOREGROUND_RED;
	if ((int)color & (int)ConsoleColor::eGreen) c |= FOREGROUND_GREEN;
	if ((int)color & (int)ConsoleColor::eBlue) 	c |= FOREGROUND_BLUE;
	if ((int)color & (int)ConsoleColor::eBold) 	c |= FOREGROUND_INTENSITY;
	if      (str == stdin)  SetConsoleTextAttribute(GetStdHandle(STD_INPUT_HANDLE) , c);
	else if (str == stdout) SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), c);
	else if (str == stderr) SetConsoleTextAttribute(GetStdHandle(STD_ERROR_HANDLE) , c);
	#else
	switch (color) {
		case ConsoleColor::eBlack:   	fprintf(str, "\x1B[0;30m"); break;
		case ConsoleColor::eRed:  		fprintf(str, "\x1B[0;31m"); break;
		case ConsoleColor::eGreen:  	fprintf(str, "\x1B[0;32m"); break;
		case ConsoleColor::eYellow:  	fprintf(str, "\x1B[0;33m"); break;
		case ConsoleColor::eBlue:  		fprintf(str, "\x1B[0;34m"); break;
		case ConsoleColor::eMagenta:  	fprintf(str, "\x1B[0;35m"); break;
		case ConsoleColor::eCyan:  		fprintf(str, "\x1B[0;36m"); break;
		case ConsoleColor::eWhite:  	fprintf(str, "\x1B[0m"); break;
	}
	#endif
	
	fprintf(str, format, forward<Args>(a)...);

	#ifdef WIN32
	if      (str == stdin)  SetConsoleTextAttribute(GetStdHandle(STD_INPUT_HANDLE) , FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
	else if (str == stdout) SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
	else if (str == stderr) SetConsoleTextAttribute(GetStdHandle(STD_ERROR_HANDLE) , FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
	#else
	fprintf(str, "\x1B[0m");
	#endif
}
template<typename... Args> inline void printf_color(ConsoleColor color, const char* format, Args&&... a) { 
	fprintf_color(color, stdout, format, forward<Args>(a)...);
}

inline wstring s2ws(const string &str) {
	 if (str.empty()) return wstring();
	#ifdef WIN32
	 int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
	 wstring wstr(size_needed, 0);
	 MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstr[0], size_needed);
	return wstr;
	 #endif
	#ifdef __linux
	 wstring_convert<codecvt_utf8<wchar_t>> wstr;
	 return wstr.from_bytes(str);
	#endif
}
inline string ws2s(const wstring &wstr) {
	if (wstr.empty()) return string();
	#ifdef WIN32
	int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
	string str(size_needed, 0);
	WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &str[0], size_needed, NULL, NULL);
	return str;
	#endif
	#ifdef __linux
	 wstring_convert<codecvt_utf8<wchar_t>> str;
	 return str.to_bytes(wstr);
	#endif
}

template<ranges::contiguous_range R>
inline R read_file(const fs::path& filename) {
	ifstream file(filename, ios::ate | ios::binary);
	if (!file.is_open()) return {};
	R dst;
	dst.resize((size_t)file.tellg()/sizeof(ranges::range_value_t<R>));
	if (dst.empty()) return dst;
	file.seekg(0);
	file.clear();
	file.read(reinterpret_cast<char*>(dst.data()), dst.size()*sizeof(ranges::range_value_t<R>));
	return dst;
}
template<ranges::contiguous_range R>
inline R read_file(const fs::path& filename, R& dst) {
	ifstream file(filename, ios::ate | ios::binary);
	if (!file.is_open()) return {};
	size_t sz = (size_t)file.tellg();
	file.seekg(0);
	file.clear();
	file.read(reinterpret_cast<char*>(dst.data()), sz);
	return dst;
}
template<ranges::contiguous_range R>
inline void write_file(const fs::path& filename, const R& r) {
	ofstream file(filename, ios::ate | ios::binary);
	file.write(reinterpret_cast<char*>(r.data()), r.size()*sizeof(ranges::range_value_t<R>));
}

inline constexpr bool is_depth_stencil(vk::Format format) {
	return
		format == vk::Format::eS8Uint ||
		format == vk::Format::eD16Unorm ||
		format == vk::Format::eD16UnormS8Uint ||
		format == vk::Format::eX8D24UnormPack32 ||
		format == vk::Format::eD24UnormS8Uint ||
		format == vk::Format::eD32Sfloat ||
		format == vk::Format::eD32SfloatS8Uint;
}

// Size of an element of format, in bytes
template<typename T = uint32_t> requires(is_arithmetic_v<T>)
inline constexpr T texel_size(vk::Format format) {
	switch (format) {
	case vk::Format::eR4G4UnormPack8:
	case vk::Format::eR8Unorm:
	case vk::Format::eR8Snorm:
	case vk::Format::eR8Uscaled:
	case vk::Format::eR8Sscaled:
	case vk::Format::eR8Uint:
	case vk::Format::eR8Sint:
	case vk::Format::eR8Srgb:
	case vk::Format::eS8Uint:
		return 1;

	case vk::Format::eR4G4B4A4UnormPack16:
	case vk::Format::eB4G4R4A4UnormPack16:
	case vk::Format::eR5G6B5UnormPack16:
	case vk::Format::eB5G6R5UnormPack16:
	case vk::Format::eR5G5B5A1UnormPack16:
	case vk::Format::eB5G5R5A1UnormPack16:
	case vk::Format::eA1R5G5B5UnormPack16:
	case vk::Format::eR8G8Unorm:
	case vk::Format::eR8G8Snorm:
	case vk::Format::eR8G8Uscaled:
	case vk::Format::eR8G8Sscaled:
	case vk::Format::eR8G8Uint:
	case vk::Format::eR8G8Sint:
	case vk::Format::eR8G8Srgb:
	case vk::Format::eR16Unorm:
	case vk::Format::eR16Snorm:
	case vk::Format::eR16Uscaled:
	case vk::Format::eR16Sscaled:
	case vk::Format::eR16Uint:
	case vk::Format::eR16Sint:
	case vk::Format::eR16Sfloat:
	case vk::Format::eD16Unorm:
		return 2;

	case vk::Format::eR8G8B8Unorm:
	case vk::Format::eR8G8B8Snorm:
	case vk::Format::eR8G8B8Uscaled:
	case vk::Format::eR8G8B8Sscaled:
	case vk::Format::eR8G8B8Uint:
	case vk::Format::eR8G8B8Sint:
	case vk::Format::eR8G8B8Srgb:
	case vk::Format::eB8G8R8Unorm:
	case vk::Format::eB8G8R8Snorm:
	case vk::Format::eB8G8R8Uscaled:
	case vk::Format::eB8G8R8Sscaled:
	case vk::Format::eB8G8R8Uint:
	case vk::Format::eB8G8R8Sint:
	case vk::Format::eB8G8R8Srgb:
	case vk::Format::eD16UnormS8Uint:
		return 3;

	case vk::Format::eR8G8B8A8Unorm:
	case vk::Format::eR8G8B8A8Snorm:
	case vk::Format::eR8G8B8A8Uscaled:
	case vk::Format::eR8G8B8A8Sscaled:
	case vk::Format::eR8G8B8A8Uint:
	case vk::Format::eR8G8B8A8Sint:
	case vk::Format::eR8G8B8A8Srgb:
	case vk::Format::eB8G8R8A8Unorm:
	case vk::Format::eB8G8R8A8Snorm:
	case vk::Format::eB8G8R8A8Uscaled:
	case vk::Format::eB8G8R8A8Sscaled:
	case vk::Format::eB8G8R8A8Uint:
	case vk::Format::eB8G8R8A8Sint:
	case vk::Format::eB8G8R8A8Srgb:
	case vk::Format::eA8B8G8R8UnormPack32:
	case vk::Format::eA8B8G8R8SnormPack32:
	case vk::Format::eA8B8G8R8UscaledPack32:
	case vk::Format::eA8B8G8R8SscaledPack32:
	case vk::Format::eA8B8G8R8UintPack32:
	case vk::Format::eA8B8G8R8SintPack32:
	case vk::Format::eA8B8G8R8SrgbPack32:
	case vk::Format::eA2R10G10B10UnormPack32:
	case vk::Format::eA2R10G10B10SnormPack32:
	case vk::Format::eA2R10G10B10UscaledPack32:
	case vk::Format::eA2R10G10B10SscaledPack32:
	case vk::Format::eA2R10G10B10UintPack32:
	case vk::Format::eA2R10G10B10SintPack32:
	case vk::Format::eA2B10G10R10UnormPack32:
	case vk::Format::eA2B10G10R10SnormPack32:
	case vk::Format::eA2B10G10R10UscaledPack32:
	case vk::Format::eA2B10G10R10SscaledPack32:
	case vk::Format::eA2B10G10R10UintPack32:
	case vk::Format::eA2B10G10R10SintPack32:
	case vk::Format::eR16G16Unorm:
	case vk::Format::eR16G16Snorm:
	case vk::Format::eR16G16Uscaled:
	case vk::Format::eR16G16Sscaled:
	case vk::Format::eR16G16Uint:
	case vk::Format::eR16G16Sint:
	case vk::Format::eR16G16Sfloat:
	case vk::Format::eR32Uint:
	case vk::Format::eR32Sint:
	case vk::Format::eR32Sfloat:
	case vk::Format::eD24UnormS8Uint:
	case vk::Format::eD32Sfloat:
		return 4;

	case vk::Format::eD32SfloatS8Uint:
		return 5;
		
	case vk::Format::eR16G16B16Unorm:
	case vk::Format::eR16G16B16Snorm:
	case vk::Format::eR16G16B16Uscaled:
	case vk::Format::eR16G16B16Sscaled:
	case vk::Format::eR16G16B16Uint:
	case vk::Format::eR16G16B16Sint:
	case vk::Format::eR16G16B16Sfloat:
		return 6;

	case vk::Format::eR16G16B16A16Unorm:
	case vk::Format::eR16G16B16A16Snorm:
	case vk::Format::eR16G16B16A16Uscaled:
	case vk::Format::eR16G16B16A16Sscaled:
	case vk::Format::eR16G16B16A16Uint:
	case vk::Format::eR16G16B16A16Sint:
	case vk::Format::eR16G16B16A16Sfloat:
	case vk::Format::eR32G32Uint:
	case vk::Format::eR32G32Sint:
	case vk::Format::eR32G32Sfloat:
	case vk::Format::eR64Uint:
	case vk::Format::eR64Sint:
	case vk::Format::eR64Sfloat:
		return 8;

	case vk::Format::eR32G32B32Uint:
	case vk::Format::eR32G32B32Sint:
	case vk::Format::eR32G32B32Sfloat:
		return 12;

	case vk::Format::eR32G32B32A32Uint:
	case vk::Format::eR32G32B32A32Sint:
	case vk::Format::eR32G32B32A32Sfloat:
	case vk::Format::eR64G64Uint:
	case vk::Format::eR64G64Sint:
	case vk::Format::eR64G64Sfloat:
		return 16;

	case vk::Format::eR64G64B64Uint:
	case vk::Format::eR64G64B64Sint:
	case vk::Format::eR64G64B64Sfloat:
		return 24;

	case vk::Format::eR64G64B64A64Uint:
	case vk::Format::eR64G64B64A64Sint:
	case vk::Format::eR64G64B64A64Sfloat:
		return 32;

	}
	return 0;
}

template<typename T = uint32_t> requires(is_arithmetic_v<T>)
inline constexpr T channel_count(vk::Format format) {
	switch (format) {
		case vk::Format::eR8Unorm:
		case vk::Format::eR8Snorm:
		case vk::Format::eR8Uscaled:
		case vk::Format::eR8Sscaled:
		case vk::Format::eR8Uint:
		case vk::Format::eR8Sint:
		case vk::Format::eR8Srgb:
		case vk::Format::eR16Unorm:
		case vk::Format::eR16Snorm:
		case vk::Format::eR16Uscaled:
		case vk::Format::eR16Sscaled:
		case vk::Format::eR16Uint:
		case vk::Format::eR16Sint:
		case vk::Format::eR16Sfloat:
		case vk::Format::eR32Uint:
		case vk::Format::eR32Sint:
		case vk::Format::eR32Sfloat:
		case vk::Format::eR64Uint:
		case vk::Format::eR64Sint:
		case vk::Format::eR64Sfloat:
		case vk::Format::eD16Unorm:
		case vk::Format::eD32Sfloat:
		case vk::Format::eD16UnormS8Uint:
		case vk::Format::eD24UnormS8Uint:
		case vk::Format::eX8D24UnormPack32:
		case vk::Format::eS8Uint:
		case vk::Format::eD32SfloatS8Uint:
			return 1;
		case vk::Format::eR4G4UnormPack8:
		case vk::Format::eR8G8Unorm:
		case vk::Format::eR8G8Snorm:
		case vk::Format::eR8G8Uscaled:
		case vk::Format::eR8G8Sscaled:
		case vk::Format::eR8G8Uint:
		case vk::Format::eR8G8Sint:
		case vk::Format::eR8G8Srgb:
		case vk::Format::eR16G16Unorm:
		case vk::Format::eR16G16Snorm:
		case vk::Format::eR16G16Uscaled:
		case vk::Format::eR16G16Sscaled:
		case vk::Format::eR16G16Uint:
		case vk::Format::eR16G16Sint:
		case vk::Format::eR16G16Sfloat:
		case vk::Format::eR32G32Uint:
		case vk::Format::eR32G32Sint:
		case vk::Format::eR32G32Sfloat:
		case vk::Format::eR64G64Uint:
		case vk::Format::eR64G64Sint:
		case vk::Format::eR64G64Sfloat:
			return 2;
		case vk::Format::eR4G4B4A4UnormPack16:
		case vk::Format::eB4G4R4A4UnormPack16:
		case vk::Format::eR5G6B5UnormPack16:
		case vk::Format::eB5G6R5UnormPack16:
		case vk::Format::eR8G8B8Unorm:
		case vk::Format::eR8G8B8Snorm:
		case vk::Format::eR8G8B8Uscaled:
		case vk::Format::eR8G8B8Sscaled:
		case vk::Format::eR8G8B8Uint:
		case vk::Format::eR8G8B8Sint:
		case vk::Format::eR8G8B8Srgb:
		case vk::Format::eB8G8R8Unorm:
		case vk::Format::eB8G8R8Snorm:
		case vk::Format::eB8G8R8Uscaled:
		case vk::Format::eB8G8R8Sscaled:
		case vk::Format::eB8G8R8Uint:
		case vk::Format::eB8G8R8Sint:
		case vk::Format::eB8G8R8Srgb:
		case vk::Format::eR16G16B16Unorm:
		case vk::Format::eR16G16B16Snorm:
		case vk::Format::eR16G16B16Uscaled:
		case vk::Format::eR16G16B16Sscaled:
		case vk::Format::eR16G16B16Uint:
		case vk::Format::eR16G16B16Sint:
		case vk::Format::eR16G16B16Sfloat:
		case vk::Format::eR32G32B32Uint:
		case vk::Format::eR32G32B32Sint:
		case vk::Format::eR32G32B32Sfloat:
		case vk::Format::eR64G64B64Uint:
		case vk::Format::eR64G64B64Sint:
		case vk::Format::eR64G64B64Sfloat:
		case vk::Format::eB10G11R11UfloatPack32:
			return 3;
		case vk::Format::eR5G5B5A1UnormPack16:
		case vk::Format::eB5G5R5A1UnormPack16:
		case vk::Format::eA1R5G5B5UnormPack16:
		case vk::Format::eR8G8B8A8Unorm:
		case vk::Format::eR8G8B8A8Snorm:
		case vk::Format::eR8G8B8A8Uscaled:
		case vk::Format::eR8G8B8A8Sscaled:
		case vk::Format::eR8G8B8A8Uint:
		case vk::Format::eR8G8B8A8Sint:
		case vk::Format::eR8G8B8A8Srgb:
		case vk::Format::eB8G8R8A8Unorm:
		case vk::Format::eB8G8R8A8Snorm:
		case vk::Format::eB8G8R8A8Uscaled:
		case vk::Format::eB8G8R8A8Sscaled:
		case vk::Format::eB8G8R8A8Uint:
		case vk::Format::eB8G8R8A8Sint:
		case vk::Format::eB8G8R8A8Srgb:
		case vk::Format::eA8B8G8R8UnormPack32:
		case vk::Format::eA8B8G8R8SnormPack32:
		case vk::Format::eA8B8G8R8UscaledPack32:
		case vk::Format::eA8B8G8R8SscaledPack32:
		case vk::Format::eA8B8G8R8UintPack32:
		case vk::Format::eA8B8G8R8SintPack32:
		case vk::Format::eA8B8G8R8SrgbPack32:
		case vk::Format::eA2R10G10B10UnormPack32:
		case vk::Format::eA2R10G10B10SnormPack32:
		case vk::Format::eA2R10G10B10UscaledPack32:
		case vk::Format::eA2R10G10B10SscaledPack32:
		case vk::Format::eA2R10G10B10UintPack32:
		case vk::Format::eA2R10G10B10SintPack32:
		case vk::Format::eA2B10G10R10UnormPack32:
		case vk::Format::eA2B10G10R10SnormPack32:
		case vk::Format::eA2B10G10R10UscaledPack32:
		case vk::Format::eA2B10G10R10SscaledPack32:
		case vk::Format::eA2B10G10R10UintPack32:
		case vk::Format::eA2B10G10R10SintPack32:
		case vk::Format::eR16G16B16A16Unorm:
		case vk::Format::eR16G16B16A16Snorm:
		case vk::Format::eR16G16B16A16Uscaled:
		case vk::Format::eR16G16B16A16Sscaled:
		case vk::Format::eR16G16B16A16Uint:
		case vk::Format::eR16G16B16A16Sint:
		case vk::Format::eR16G16B16A16Sfloat:
		case vk::Format::eR32G32B32A32Uint:
		case vk::Format::eR32G32B32A32Sint:
		case vk::Format::eR32G32B32A32Sfloat:
		case vk::Format::eR64G64B64A64Uint:
		case vk::Format::eR64G64B64A64Sint:
		case vk::Format::eR64G64B64A64Sfloat:
		case vk::Format::eE5B9G9R9UfloatPack32:
		case vk::Format::eBc1RgbaUnormBlock:
		case vk::Format::eBc1RgbaSrgbBlock:
			return 4;

		// TODO: compressed formats
		
	}
	return 0;
}

inline vk::PipelineStageFlags guess_stage(vk::ImageLayout layout) {
	switch (layout) {
		case vk::ImageLayout::eGeneral:
			return vk::PipelineStageFlagBits::eComputeShader;

		case vk::ImageLayout::eColorAttachmentOptimal:
			return vk::PipelineStageFlagBits::eColorAttachmentOutput;
		
		case vk::ImageLayout::eShaderReadOnlyOptimal:
		case vk::ImageLayout::eDepthReadOnlyOptimal:
		case vk::ImageLayout::eStencilReadOnlyOptimal:
		case vk::ImageLayout::eDepthStencilReadOnlyOptimal:
			return vk::PipelineStageFlagBits::eFragmentShader;

		case vk::ImageLayout::eTransferSrcOptimal:
		case vk::ImageLayout::eTransferDstOptimal:
			return vk::PipelineStageFlagBits::eTransfer;

		case vk::ImageLayout::eDepthAttachmentStencilReadOnlyOptimal:
		case vk::ImageLayout::eDepthStencilAttachmentOptimal:
		case vk::ImageLayout::eStencilAttachmentOptimal:
		case vk::ImageLayout::eDepthAttachmentOptimal:
		case vk::ImageLayout::eDepthReadOnlyStencilAttachmentOptimal:
			return vk::PipelineStageFlagBits::eLateFragmentTests;

		case vk::ImageLayout::ePresentSrcKHR:
		case vk::ImageLayout::eSharedPresentKHR:
			return vk::PipelineStageFlagBits::eBottomOfPipe;

		default:
			return vk::PipelineStageFlagBits::eTopOfPipe;
	}
}

inline vk::AccessFlags guess_access_flags(vk::ImageLayout layout) {
	switch (layout) {
    case vk::ImageLayout::eUndefined:
    case vk::ImageLayout::ePresentSrcKHR:
			return vk::AccessFlagBits::eNoneKHR;

    case vk::ImageLayout::eColorAttachmentOptimal:
			return vk::AccessFlagBits::eColorAttachmentWrite;

    case vk::ImageLayout::eGeneral:
			return vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite;

    case vk::ImageLayout::eDepthAttachmentOptimal:
    case vk::ImageLayout::eStencilAttachmentOptimal:
    case vk::ImageLayout::eDepthReadOnlyStencilAttachmentOptimal:
    case vk::ImageLayout::eDepthAttachmentStencilReadOnlyOptimal:
    case vk::ImageLayout::eDepthStencilAttachmentOptimal:
			return vk::AccessFlagBits::eDepthStencilAttachmentRead | vk::AccessFlagBits::eDepthStencilAttachmentWrite;

    case vk::ImageLayout::eDepthReadOnlyOptimal:
    case vk::ImageLayout::eStencilReadOnlyOptimal:
    case vk::ImageLayout::eDepthStencilReadOnlyOptimal:
			return vk::AccessFlagBits::eDepthStencilAttachmentRead;
		
    case vk::ImageLayout::eShaderReadOnlyOptimal:
			return vk::AccessFlagBits::eShaderRead;
    case vk::ImageLayout::eTransferSrcOptimal:
			return vk::AccessFlagBits::eTransferRead;
    case vk::ImageLayout::eTransferDstOptimal:
			return vk::AccessFlagBits::eTransferWrite;
	}
	return vk::AccessFlagBits::eShaderRead;
}

inline auto format_bytes(size_t bytes) { 
	const char* units[] { "B", "KB", "MB", "GB", "TB" };
	uint32_t i = 0;
	while (bytes > 1024 && i < ranges::size(units)-1) {
		bytes /= 1024;
		i++;
	}
	return make_pair(bytes, units[i]);
}

template<typename T>
inline static void store_texel(void* data, const T v, uint32_t c, vk::Format format) {
	switch (format) {
		case vk::Format::eR8Sint:
		case vk::Format::eR8G8Sint:
		case vk::Format::eR8G8B8Sint:
		case vk::Format::eR8G8B8A8Sint:
			reinterpret_cast<int8_t*>(data)[c] = (int8_t)v;
			break;
		case vk::Format::eR8Uint:
		case vk::Format::eR8G8Uint:
		case vk::Format::eR8G8B8Uint:
		case vk::Format::eR8G8B8A8Uint:
			reinterpret_cast<uint8_t*>(data)[c] = (uint8_t)v;
			break;

		case vk::Format::eR16Sint:
		case vk::Format::eR16G16Sint:
		case vk::Format::eR16G16B16Sint:
		case vk::Format::eR16G16B16A16Sint:
			reinterpret_cast<int16_t*>(data)[c] = (int16_t)v;
			break;
		case vk::Format::eR16Uint:
		case vk::Format::eR16G16Uint:
		case vk::Format::eR16G16B16Uint:
		case vk::Format::eR16G16B16A16Uint:
			reinterpret_cast<uint16_t*>(data)[c] = (uint16_t)v;
			break;
			
		case vk::Format::eR32Sint:
		case vk::Format::eR32G32Sint:
		case vk::Format::eR32G32B32Sint:
		case vk::Format::eR32G32B32A32Sint:
			reinterpret_cast<int32_t*>(data)[c] = (int32_t)v;
			break;
		case vk::Format::eR32Uint:
		case vk::Format::eR32G32Uint:
		case vk::Format::eR32G32B32Uint:
		case vk::Format::eR32G32B32A32Uint:
			reinterpret_cast<uint32_t*>(data)[c] = (uint32_t)v;
			break;
			
		case vk::Format::eR64Sint:
		case vk::Format::eR64G64Sint:
		case vk::Format::eR64G64B64Sint:
		case vk::Format::eR64G64B64A64Sint:
			reinterpret_cast<int64_t*>(data)[c] = (int64_t)v;
			break;
		case vk::Format::eR64Uint:
		case vk::Format::eR64G64Uint:
		case vk::Format::eR64G64B64Uint:
		case vk::Format::eR64G64B64A64Uint:
			reinterpret_cast<uint64_t*>(data)[c] = (uint64_t)v;
			break;

		case vk::Format::eR8Unorm:
		case vk::Format::eR8G8Unorm:
		case vk::Format::eR8G8B8Unorm:
		case vk::Format::eR8G8B8A8Unorm:
			reinterpret_cast<uint8_t*>(data)[c] = is_floating_point_v<T> ? (uint8_t)clamp(v*numeric_limits<uint8_t>::max(), 0, numeric_limits<uint8_t>::max()) : (uint8_t)v;
			break;
		case vk::Format::eR16Unorm:
		case vk::Format::eR16G16Unorm:
		case vk::Format::eR16G16B16Unorm:
		case vk::Format::eR16G16B16A16Unorm:
			reinterpret_cast<uint16_t*>(data)[c] = is_floating_point_v<T> ? (uint16_t)clamp(v*numeric_limits<uint16_t>::max(), 0, numeric_limits<uint16_t>::max()) : (uint16_t)v;
			break;
			
		case vk::Format::eR8Snorm:
		case vk::Format::eR8G8Snorm:
		case vk::Format::eR8G8B8Snorm:
		case vk::Format::eR8G8B8A8Snorm:
			reinterpret_cast<uint8_t*>(data)[c] = is_floating_point_v<T> ? (uint8_t)clamp((v*.5 + .5)*numeric_limits<uint8_t>::max(), 0, numeric_limits<uint8_t>::max()) : (uint8_t)v;
			break;
		case vk::Format::eR16Snorm:
		case vk::Format::eR16G16Snorm:
		case vk::Format::eR16G16B16Snorm:
		case vk::Format::eR16G16B16A16Snorm:
			reinterpret_cast<uint16_t*>(data)[c] = is_floating_point_v<T> ? (uint16_t)clamp((v*.5 + .5)*numeric_limits<uint16_t>::max(), 0, numeric_limits<uint16_t>::max()) : (uint16_t)v;
			break;

		case vk::Format::eR32Sfloat:
		case vk::Format::eR32G32Sfloat:
		case vk::Format::eR32G32B32Sfloat:
		case vk::Format::eR32G32B32A32Sfloat:
			reinterpret_cast<float*>(data)[c] = (float)v;
			break;
		case vk::Format::eR64Sfloat:
		case vk::Format::eR64G64Sfloat:
		case vk::Format::eR64G64B64Sfloat:
		case vk::Format::eR64G64B64A64Sfloat:
			reinterpret_cast<double*>(data)[c] = (double)v;
			break;
	}
}

inline static float load_texel(void* data, uint32_t c, vk::Format format) {
	switch (format) {
		case vk::Format::eR8Sint:
		case vk::Format::eR8G8Sint:
		case vk::Format::eR8G8B8Sint:
		case vk::Format::eR8G8B8A8Sint:
			return reinterpret_cast<int8_t*>(data)[c];
		case vk::Format::eR8Uint:
		case vk::Format::eR8G8Uint:
		case vk::Format::eR8G8B8Uint:
		case vk::Format::eR8G8B8A8Uint:
			return reinterpret_cast<uint8_t*>(data)[c];

		case vk::Format::eR16Sint:
		case vk::Format::eR16G16Sint:
		case vk::Format::eR16G16B16Sint:
		case vk::Format::eR16G16B16A16Sint:
			return reinterpret_cast<int16_t*>(data)[c];
		case vk::Format::eR16Uint:
		case vk::Format::eR16G16Uint:
		case vk::Format::eR16G16B16Uint:
		case vk::Format::eR16G16B16A16Uint:
			return reinterpret_cast<uint16_t*>(data)[c];
			
		case vk::Format::eR32Sint:
		case vk::Format::eR32G32Sint:
		case vk::Format::eR32G32B32Sint:
		case vk::Format::eR32G32B32A32Sint:
			return reinterpret_cast<int32_t*>(data)[c];
		case vk::Format::eR32Uint:
		case vk::Format::eR32G32Uint:
		case vk::Format::eR32G32B32Uint:
		case vk::Format::eR32G32B32A32Uint:
			return reinterpret_cast<uint32_t*>(data)[c];
			
		case vk::Format::eR64Sint:
		case vk::Format::eR64G64Sint:
		case vk::Format::eR64G64B64Sint:
		case vk::Format::eR64G64B64A64Sint:
			return reinterpret_cast<int64_t*>(data)[c];
		case vk::Format::eR64Uint:
		case vk::Format::eR64G64Uint:
		case vk::Format::eR64G64B64Uint:
		case vk::Format::eR64G64B64A64Uint:
			return reinterpret_cast<uint64_t*>(data)[c];

		case vk::Format::eR8Unorm:
		case vk::Format::eR8G8Unorm:
		case vk::Format::eR8G8B8Unorm:
		case vk::Format::eR8G8B8A8Unorm:
			return (float)reinterpret_cast<uint8_t*>(data)[c] / numeric_limits<uint8_t>::max();
		case vk::Format::eR16Unorm:
		case vk::Format::eR16G16Unorm:
		case vk::Format::eR16G16B16Unorm:
		case vk::Format::eR16G16B16A16Unorm:
			return (float)reinterpret_cast<uint16_t*>(data)[c] / numeric_limits<uint16_t>::max();

		case vk::Format::eR8Snorm:
		case vk::Format::eR8G8Snorm:
		case vk::Format::eR8G8B8Snorm:
		case vk::Format::eR8G8B8A8Snorm:
			return (float)reinterpret_cast<uint8_t*>(data)[c] / numeric_limits<uint8_t>::max() * 2 - 1;
		case vk::Format::eR16Snorm:
		case vk::Format::eR16G16Snorm:
		case vk::Format::eR16G16B16Snorm:
		case vk::Format::eR16G16B16A16Snorm:
			return (float)reinterpret_cast<uint16_t*>(data)[c] / numeric_limits<uint16_t>::max() * 2 - 1;

		case vk::Format::eR32Sfloat:
		case vk::Format::eR32G32Sfloat:
		case vk::Format::eR32G32B32Sfloat:
		case vk::Format::eR32G32B32A32Sfloat:
			return reinterpret_cast<float*>(data)[c];
		case vk::Format::eR64Sfloat:
		case vk::Format::eR64G64Sfloat:
		case vk::Format::eR64G64B64Sfloat:
		case vk::Format::eR64G64B64A64Sfloat:
			return (float)reinterpret_cast<double*>(data)[c];
	}
}

}