#pragma once
#include <utility>
template<typename F>
struct ScopeExit { F f; bool active{true}; ~ScopeExit(){ if(active) f(); } };
template<typename F>
ScopeExit<F> MakeScopeExit(F&& f){ return ScopeExit<F>{std::forward<F>(f)}; }
