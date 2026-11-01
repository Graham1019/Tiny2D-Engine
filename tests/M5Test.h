#pragma once
#include <stdexcept>
#include <iostream>
inline void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class F> void rejects(F f){bool caught=false;try{f();}catch(const std::exception&e){caught=*e.what();}check(caught,"expected diagnostic");}
