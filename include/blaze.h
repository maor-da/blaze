#pragma once
#include "../glaze/include/glaze/glaze.hpp"

#include <expected>	 // c++23
#include <map>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

namespace json
{

#define __json_key(x) #x, &T::x
#define __json_key_offset(x, y) x, &T::y
#define __json_enum(x) #x, T::x
#define __json_enum_value(x, y) x, T::y

#define GET_MACRO(_1, _2, NAME, ...) NAME

// require Yes (/Zc:preprocessor)
#define json_key(...) GET_MACRO(__VA_ARGS__, __json_key_offset, __json_key)(__VA_ARGS__)
#define json_enum(...) GET_MACRO(__VA_ARGS__, __json_enum_value, __json_enum)(__VA_ARGS__)

template <int N, typename... T>
using nth_type = typename std::tuple_element<N, std::tuple<T...>>::type;

template <typename... Args>
constexpr auto struct_meta(Args&&... args)
{
	if constexpr (sizeof...(args) == 0) {
		return glz::detail::Object{glz::tuplet::tuple{}};
	} else {
		return glz::detail::Object{
			glz::group_builder<std::decay_t<decltype(glz::tuplet::make_copy_tuple(args...))>>::op(
				glz::tuplet::make_copy_tuple(args...))};
	}
}

constexpr auto enumerate_meta(auto&&... args)
{
	return glz::enumerate(args...);
}

template <class T, class S, std::size_t N>
struct key {
	const char name[N];
	T S::*offset;
};

template <class Ty, key... Ts>
struct json_struct : Ty {
	struct glaze {
		using T = Ty;
		// Thanks to https://stackoverflow.com/a/76596431/1902398
		static constexpr auto value = std::apply([](auto... args) { return struct_meta(args...); },
												 std::tuple_cat(std::tuple(Ts.name, Ts.offset)...));
	};
};

using primitive_value_t =
	std::variant<std::string, int64_t, int32_t, uint32_t, uint64_t, bool, double, std::nullptr_t, char>;

using possible_value_t = std::variant<primitive_value_t,
									  std::vector<int64_t>,
									  std::vector<int32_t>,
									  std::vector<uint32_t>,
									  std::vector<uint64_t>,
									  std::vector<bool>,
									  std::vector<double>,
									  std::vector<char>,
									  std::vector<std::string>,
									  std::vector<std::nullptr_t>>;

struct json_doc_t
	: public std::map<std::string, std::variant<possible_value_t, json_doc_t, std::vector<json_doc_t>>> {
};

template <typename T>
concept local_meta_t = glz::detail::local_meta_t<T>;

template <typename T>
concept not_local_meta_t = requires { !local_meta_t<T>; };

template <typename T>
concept dynamic_json = std::same_as<T, json_doc_t>;

// using namespace glz; this is not safe since it could shadow other declerations
template <typename T>
class _json_string
{
public:
	using doc_t = T;

	template <glz::opts settings = glz::opts{}>
	std::string pretty()
	{
		std::string s{};
		glz::write<glz::opt_true<settings, &glz::opts::prettify>>(m_Doc, s);
		return s;
	}

	template <glz::opts settings>
	std::string format()
	{
		std::string s{};
		glz::write<settings>(m_Doc, s);
		return s;
	}

	std::string string()
	{
		return glz::write_json(m_Doc);
	}

	// operator and extractors
	operator std::string()
	{
		return glz::write_json(m_Doc);
	}

protected:
	template <typename T>
	friend class _json_base;

	_json_string(doc_t& doc) : m_Doc(doc)
	{
		// set the json doc
	}

private:
	doc_t& m_Doc;
};

template <class T>
class seekable_value
{
public:
	using doc_t = T;

	seekable_value(T& doc, std::string_view key) : key(key), doc(doc) {}
	seekable_value(const seekable_value&) = delete;

	template <class V>
	inline constexpr void operator=(V&& value)
	{
		// glz::set(doc, key, value)
		bool result{};
		glz::detail::seek_impl(
			[&](auto&& val) {
				if constexpr (std::is_assignable_v<decltype(val), decltype(value)> /*&&
							  glz::detail::non_narrowing_convertable<std::decay_t<decltype(value)>,
																std::decay_t<decltype(val)>>*/) {
					result = true;
#if defined(__GNUC__) || defined(__GNUG__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Waddress"
#endif
					// val = static_cast<decltype(val)>(value);
					val = value;
#if defined(__GNUC__) || defined(__GNUG__)
#pragma GCC diagnostic pop
#endif
				}
			},
			std::forward<T>(doc),
			key);

		if (!result) {
			throw std::runtime_error(__FUNCSIG__ "cannot assign or convert value - could be wrong type");
		}
	}

	template <class V>
	inline constexpr operator V()
	{
		// auto ret = glz::get<V>(doc, key);

		V* result{};
		glz::error_code ec{};
		glz::detail::seek_impl(
			[&](auto&& val) {
				if constexpr (!std::is_same_v<V, std::decay_t<decltype(val)>>) {
					ec = glz::error_code::get_wrong_type;
				} else if constexpr (!std::is_lvalue_reference_v<decltype(val)>) {
					ec = glz::error_code::cannot_be_referenced;
				} else {
					result = &val;
				}
			},
			std::forward<T>(doc),
			key);
		if (!result) {
			// return glz::unexpected(glz::parse_error{error_code::get_nonexistent_json_ptr});
			throw std::runtime_error(__FUNCSIG__ "get_nonexistent_json_ptr");
		} else if (bool(ec)) {
			// return glz::unexpected(glz::parse_error{ec});
			throw std::runtime_error(__FUNCSIG__ "get_wrong_type or cannot_be_referenced");
		}
		return *result;
	}

private:
	std::string key;
	doc_t& doc;
};

template <typename T>
class _json_base
{
public:
	using doc_t = T;

	// Write to string
	_NODISCARD _json_string<T> dumps()
	{
		return {m_Doc};
	}

	// Load from string
	template <glz::opts settings = glz::opts{}>
	_NODISCARD glz::expected<bool, glz::error_code> loads(std::string_view str)
	{
		auto err = glz::read<glz::opt_false<settings, &glz::opts::error_on_unknown_keys>>(m_Doc, str);
		if (err) {
			return glz::unexpected(err.ec);
		}

		return true;
	}

	// Write to file
	int dumps(int file)
	{
		throw std::runtime_error(__FUNCSIG__ "is not implemented");
	}
	// Load from file
	int loads(int file)
	{
		throw std::runtime_error(__FUNCSIG__ "is not implemented");
	}

	// https://github.com/stephenberry/JSON-Pointer
	constexpr inline auto operator[](std::string_view key)
	requires(not std::is_same_v<doc_t, json_doc_t>)
	{
		if (key.empty()) {
			return seekable_value(m_Doc, "");
		} else if (key[0] == '/') {
			return seekable_value(m_Doc, key);
		} else {
			std::string jptr = "/";
			return seekable_value(m_Doc, jptr + key.data());
		}
	}

	void loads(doc_t& doc)
	{
		m_Doc = doc;
	}

	doc_t& get_storage()
	{
		return m_Doc;
	}

protected:
	doc_t m_Doc = {};  // the only storage for the json document
};

template <class _type = json_doc_t>
class json : public _json_base<_type>
{
public:
	using doc_t	 = _type;
	using base_t = _json_base<doc_t>;
	doc_t& doc	 = base_t::m_Doc;

	template <key... Ts>
	requires not_local_meta_t<_type>
	class meta : public _json_base<json_struct<_type, Ts...>>
	{
	public:
		using base_t	   = _json_base<json_struct<_type, Ts...>>;
		base_t::doc_t& doc = base_t::m_Doc;
	};
};

/**
 * This is a dynamic version which build the json map in runtime
 * It will be slower - but produce smaller executable
 */
template <>
class json<json_doc_t> : public _json_base<json_doc_t>
{
public:
	using doc_t = json_doc_t;

	// only for json_doc type
	json_doc_t::mapped_type& operator[](json_doc_t::key_type&& key)
	{
		return m_Doc[key];
	}

	operator const json_doc_t&()
	{
		return m_Doc;
	}
};

}  // namespace json