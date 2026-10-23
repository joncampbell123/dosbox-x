// Copyright (c) 2026 Michael P. Burgus - https://github.com/NeuralDrifter
#pragma once

#include <map>
#include <string>
#include <variant>
#include <vector>

namespace Agent86 {

class Json {
public:
	using Array = std::vector<Json>;
	using Object = std::map<std::string, Json>;

	Json();
	Json(std::nullptr_t);
	Json(bool value);
	Json(double value);
	Json(int value);
	Json(const char *value);
	Json(std::string value);
	Json(Array value);
	Json(Object value);

	static Json Parse(const std::string &text);
	std::string Dump() const;

	bool IsNull() const;
	bool IsBool() const;
	bool IsNumber() const;
	bool IsString() const;
	bool IsArray() const;
	bool IsObject() const;

	bool AsBool() const;
	double AsNumber() const;
	const std::string &AsString() const;
	const Array &AsArray() const;
	const Object &AsObject() const;
	Array &AsArray();
	Object &AsObject();

	const Json *Find(const std::string &key) const;
	Json *Find(const std::string &key);

private:
	using Value = std::variant<std::nullptr_t,
	                           bool,
	                           double,
	                           std::string,
	                           Array,
	                           Object>;
	Value value;
};

} // namespace Agent86
