#include "stdafx.h"
#include "CppUnitTest.h"
#include "../shared_string.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace shared_string_test
{		
	TEST_CLASS(SharedStringTest)
	{
	public:
		
      TEST_METHOD(Assign) {

         shared_string s = "Test";

         Assert::IsTrue(s == "Test");

         s = "NO";

         Assert::IsTrue(s == "NO");
      }
	};
}