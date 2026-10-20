// Minimal JSON value, parser and writer: enough to load case content and compare
// the port's output with the TypeScript engine's. Plain C++, no engine types.
#pragma once

#include "MysteryCore/Api.h"
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace mystery
{

class MYSTERYCORE_API Json
{
public:
	enum class Type { Null, Bool, Number, String, Array, Object };
	using Array = std::vector<Json>;
	/** Insertion-ordered; lookups are linear, which is fine at this size. */
	using Object = std::vector<std::pair<std::string, Json>>;

	Json() = default;
	Json(std::nullptr_t) {}
	Json(bool b) : type_(Type::Bool), bool_(b) {}
	Json(int n) : type_(Type::Number), number_(n) {}
	Json(double n) : type_(Type::Number), number_(n) {}
	Json(const char* s) : type_(Type::String), string_(s) {}
	Json(std::string s) : type_(Type::String), string_(std::move(s)) {}
	Json(Array a) : type_(Type::Array), array_(std::make_shared<Array>(std::move(a))) {}
	Json(Object o) : type_(Type::Object), object_(std::make_shared<Object>(std::move(o))) {}

	static Json MakeArray() { return Json(Array{}); }
	static Json MakeObject() { return Json(Object{}); }

	Type GetType() const { return type_; }
	bool IsNull() const { return type_ == Type::Null; }
	bool IsString() const { return type_ == Type::String; }
	bool IsArray() const { return type_ == Type::Array; }
	bool IsObject() const { return type_ == Type::Object; }

	bool AsBool() const { return bool_; }
	double AsNumber() const { return number_; }
	int AsInt() const { return static_cast<int>(number_); }
	const std::string& AsString() const { return string_; }
	const Array& AsArray() const { return *array_; }
	const Object& AsObject() const { return *object_; }
	Array& MutArray() { return *array_; }

	/** Object member, or a shared null if absent. */
	const Json& operator[](const std::string& key) const;
	/** Sets or replaces an object member. */
	Json& Set(const std::string& key, Json value);
	bool Has(const std::string& key) const;
	const Json& operator[](size_t index) const { return (*array_)[index]; }
	size_t Size() const;
	void Push(Json value) { array_->push_back(std::move(value)); }

	/** Deep equality; object member order does not matter. */
	bool operator==(const Json& other) const;
	bool operator!=(const Json& other) const { return !(*this == other); }

	std::string Dump() const;

	/** Returns false and fills Error on malformed input. */
	static bool Parse(const std::string& text, Json& out, std::string* error = nullptr);

private:
	Type type_ = Type::Null;
	bool bool_ = false;
	double number_ = 0.0;
	std::string string_;
	std::shared_ptr<Array> array_;
	std::shared_ptr<Object> object_;
};

/** First path where two values differ ("" if equal), for readable test failures. */
MYSTERYCORE_API std::string JsonDiff(const Json& expected, const Json& actual, const std::string& path = "");

} // namespace mystery
