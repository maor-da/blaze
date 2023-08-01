#include "../include/blaze.h"

#include <stdint.h>
#include <windows.h>

#include <iostream>

void dynamic_test()
{
	printf("Testing dynamic json builder:\n");
	json::json j;
	json::json j2;
	json::json_doc_t d1;

	std::vector<int> vec{1, 2, 3, 4, 5};
	std::vector<json::json_doc_t> vec_docs{d1};

	j2["str"]		= "עברית";
	j2["int"]		= (uint64_t)30;
	j2["vec"]		= vec;
	j2["vec_docs"]	= vec_docs;

	j["str"]		= "value";
	j["int"]		= (uint64_t)30;
	j["vec"]		= vec;
	j["vec2"]		= j2;

	std::string str = j.dumps();

	printf("%s\n", str.c_str());

	printf("\n");
}

struct meta_s {
	int a;
	float b;
	std::string str;
};

void meta_test()
{
	printf("Testing defining meta on instance:\n");
	json::json<meta_s>::meta<{"a", &meta_s::a}, {"b", &meta_s::b}, {"str", &meta_s::str}> j;
	json::json<meta_s>::meta<{"a", &meta_s::a}, {"b", &meta_s::b}, {"str", &meta_s::str}> j2;
	j.doc.a			= 6;
	j.doc.b			= 9.5;
	j.doc.str		= "adfsdfsh";

	std::string str = j.dumps().pretty();

	printf("%s\n", str.c_str());
	printf("%s\n", j2.dumps().pretty().c_str());
	printf("\n");
}

struct nested_static_s {
	int a;
	float b;
	std::string str;

	struct glaze {
		using T						= nested_static_s;
		static constexpr auto value = json::struct_meta(json_key(a), json_key(b), json_key(str));
	};
};

struct static_s {
	int a;
	nested_static_s nested;

	struct glaze {
		using T = static_s;
		static constexpr auto value =
			json::struct_meta("#include", glz::file_include{}, json_key(a), json_key(nested));
	};
};

struct static_s2 {
	int a;
	char c;
	std::string str;
	nested_static_s nested;

	struct glaze {
		using T						= static_s2;
		static constexpr auto value = json::struct_meta(
			"#include", glz::file_include{}, json_key(a), json_key(c), json_key(str), json_key(nested));
	};
};

struct static_s3 {
	int a;
	// char c;
	bool b;
	double d;
	std::vector<int> v;
	std::string s;
	nested_static_s nested;

	struct glaze {
		using T						= static_s3;
		static constexpr auto value = json::struct_meta("#include",
														glz::file_include{},
														json_key(a),
														json_key(b),
														// json_key(c),
														json_key(d),
														json_key(v),
														json_key(s),
														json_key(nested));
	};
};

void static_test()
{
	printf("Testing predefined meta struct:\n");
	json::json<static_s> j;
	json::json<static_s2> j2;
	json::json<static_s3> j3;

	float b		   = j["/nested/b"];
	j["/nested/b"] = 7;	 // float{5.0};
	b			   = j["/nested/b"];

	j.doc.a		   = 4;

	// glz::set(j.doc, "/a", 5);
	j["a"]			= 5;
	int a			= j["a"];

	std::string str = j.dumps().pretty();

	printf("%s\n", j.dumps().pretty().c_str());
	printf("%s\n", j2.dumps().pretty().c_str());
	printf("%s\n", j3.dumps().pretty().c_str());

	auto ret = j.loads(R"({"a":134567})");
	if (ret) {
		printf("%s\n", j.dumps().pretty().c_str());
	}
	printf("\n");
}

struct test {
	int a;
	const char* b;
	char* c;
	std::string str = "dfgsdf";

	struct glaze {
		using T						= test;
		static constexpr auto value = json::struct_meta(json_key(a), json_key(b), json_key(c), json_key(str));
	};
};

void main()
{
	SetConsoleOutputCP(65001);

	test obj{};
	std::string buffer{};
	glz::write<glz::opts{.prettify = true}>(obj, buffer);

	printf("%s", buffer.c_str());

	dynamic_test();
	meta_test();
	static_test();

	getchar();
}