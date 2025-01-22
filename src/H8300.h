//test

#ifndef H8300_H
#define H8300_H



#include <array>
#include <string>
#include <memory>
#include <variant>

#include "types.h"


using namespace melonDS;
class H8300 {
public:
	H8300();
	~H8300();

	void speak();
	u8 handleSPI(u8& IRCmd, u8& val, u32&pos, bool& last);


private:
	int exampleInt;
};

#endif

