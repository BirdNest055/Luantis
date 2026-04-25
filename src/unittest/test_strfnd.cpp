// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "util/strfnd.h"
#include "test.h"

class TestStrfnd : public TestCase
{
	public:
		TestStrfnd() : TestCase("TestStrfnd") {}
		void runTests() override;
};

void TestStrfnd::runTests()
{
	// Test that Strfnd works with string_view (no copy)
	std::string s = "hello,world,foo";
	std::string_view sv(s);
	Strfnd sf(sv);
	
	UASSERTEQ(std::string, sf.next(","), "hello");
	UASSERTEQ(std::string, sf.next(","), "world");
	UASSERTEQ(std::string, sf.next(","), "foo");
	
	// Test that Strfnd works with const char* (converted to string_view)
	Strfnd sf2("a:b:c");
	UASSERTEQ(std::string, sf2.next(":"), "a");
	UASSERTEQ(std::string, sf2.next(":"), "b");
	UASSERTEQ(std::string, sf2.next(":"), "c");
	
	// Test with empty separator
	Strfnd sf3("hello");
	UASSERTEQ(std::string, sf3.next(""), "hello");
	
	// Test next_esc
	Strfnd sf4("hello\\,world,foo");
	UASSERTEQ(std::string, sf4.next_esc(","), "hello\\,world");
	UASSERTEQ(std::string, sf4.next_esc(","), "foo");
	
	// Test skip_over
	Strfnd sf5("   hello");
	sf5.skip_over(" ");
	UASSERTEQ(size_t, sf5.where(), 3);
	UASSERTEQ(std::string, sf5.next(""), "hello");
}

namespace {
	TestStrfnd test_strfnd;
}
