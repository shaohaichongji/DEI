#ifndef __INSTANT_COMPILE_DEPEND_SS_YAML_H__
#define __INSTANT_COMPILE_DEPEND_SS_YAML_H__

#include "yaml-cpp/parser.h"
#include "yaml-cpp/emitter.h"
#include "yaml-cpp/emitterstyle.h"
#include "yaml-cpp/stlemitter.h"
#include "yaml-cpp/exceptions.h"

#include "yaml-cpp/node/node.h"
#include "yaml-cpp/node/impl.h"
#include "yaml-cpp/node/convert.h"
#include "yaml-cpp/node/iterator.h"
#include "yaml-cpp/node/detail/impl.h"
#include "yaml-cpp/node/parse.h"
#include "yaml-cpp/node/emit.h"

#if _DEBUG
#pragma comment(lib,"yaml-cppd.lib")
#else
#pragma comment(lib,"yaml-cpp.lib")
#endif

#endif // !__INSTANT_COMPILE_DEPEND_SS_YAML_H__

