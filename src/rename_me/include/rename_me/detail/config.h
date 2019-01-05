#pragma once

#if defined(_MSC_VER)
// Requires Visual Studio 2015 Update 2, see
// https://blogs.msdn.microsoft.com/vcblog/2016/03/30/optimizing-the-layout-of-empty-base-classes-in-vs2015-update-2-3/
#  define NN_EBO_CLASS __declspec(empty_bases)
#else
#  define NN_EBO_CLASS
#endif
