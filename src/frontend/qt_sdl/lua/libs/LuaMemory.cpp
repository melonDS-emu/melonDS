#include "lua/LuaMain.h"
#include "DSi.h"

std::vector<luaL_Reg> memoryFunctions;// list of registered lua_CFunctions for this library
LuaLibrary memoryLibrary("memory",&memoryFunctions);//adds "memory" to the list of luaLibraries

using namespace melonDS;

enum isbus{
	NotBus,
	ARM9_Bus,
	ARM7_Bus
};

struct memoryDomain{
	NDS* CurrentNDS;
	const char* name;
    u8* base;
    u64 size;
	isbus SystemBus=NotBus;
	void write(u8* buffer,s64 address,s64 count);
	void read(u8* buffer,s64 address,s64 count);
};
//https://github.com/TASEmulators/BizHawk/blob/master/waterbox/melon/BizDebugging.cpp#L148

template <bool arm9>
static bool SafeToPeek(u32 addr)
{
	if (arm9)
	{
		// dsp io reads are not safe
		if ((addr & 0xFFFFFF00) == 0x04004200)
		{
			return false;
		}

		switch (addr)
		{
			case 0x04000130:
			case 0x04000131:
			case 0x04000600:
			case 0x04000601:
			case 0x04000602:
			case 0x04000603:
				return false;
		}
	}
	else // arm7
	{
		if (addr >= 0x04800000 && addr <= 0x04810000)
		{
			if (addr & 1) addr--;
			addr &= 0x7FFE;
			if (addr == 0x044 || addr == 0x060)
				return false;
		}
	}

	return true;
}
//https://github.com/TASEmulators/BizHawk/blob/master/waterbox/melon/BizDebugging.cpp#L184
static void ARM9Access(u8* buffer, s64 address, s64 count,bool write,NDS* CurrentNDS)
{
	if (write)
	{
		while (count--)
		{
			if (address < CurrentNDS->ARM9.ITCMSize)
			{
				CurrentNDS->ARM9.ITCM[address & (melonDS::ITCMPhysicalSize - 1)] = *buffer;
			}
			else if ((address & CurrentNDS->ARM9.DTCMMask) == CurrentNDS->ARM9.DTCMBase)
			{
				CurrentNDS->ARM9.DTCM[address & (melonDS::DTCMPhysicalSize - 1)] = *buffer;
			}
			else
			{
				CurrentNDS->ARM9Write8(address, *buffer);
			}

			address++;
			buffer++;
		}
	}
	else
	{
		while (count--)
		{
			if (address < CurrentNDS->ARM9.ITCMSize)
			{
				*buffer = CurrentNDS->ARM9.ITCM[address & (melonDS::ITCMPhysicalSize - 1)];
			}
			else if ((address & CurrentNDS->ARM9.DTCMMask) == CurrentNDS->ARM9.DTCMBase)
			{
				*buffer = CurrentNDS->ARM9.DTCM[address & (melonDS::DTCMPhysicalSize - 1)];
			}
			else
			{
				*buffer = SafeToPeek<true>(address) ? CurrentNDS->ARM9Read8(address) : 0;
			}

			address++;
			buffer++;
		}
	}
}
//https://github.com/TASEmulators/BizHawk/blob/master/waterbox/melon/BizDebugging.cpp#L230
static void ARM7Access(u8* buffer, s64 address, s64 count, bool write,NDS* CurrentNDS)
{
	if (write)
	{
		while (count--)
		{
			CurrentNDS->ARM7Write8(address, *buffer);

			address++;
			buffer++;
		}
	}
	else
	{
		while (count--)
		{
			*buffer = SafeToPeek<false>(address) ? CurrentNDS->ARM7Read8(address) : 0;

			address++;
			buffer++;
		}
	}
}

void memoryDomain::write(u8* buffer,s64 address,s64 count){
	switch(this->SystemBus){
		case NotBus:
			while (count--)
				if(this->size<address)return;
				this->base[address++] = *(buffer++);
			return;
		case ARM9_Bus:
			return ARM9Access(buffer,address,count,true,this->CurrentNDS);
		case ARM7_Bus:
			return ARM7Access(buffer,address,count,true,this->CurrentNDS);
	}
}
void memoryDomain::read(u8* buffer,s64 address,s64 count){
	switch(this->SystemBus){
		case NotBus:
			while (count--)
				if (this->size<address) return;
				*(buffer++) = this->base[address++];
			return;
		case ARM9_Bus:
			return ARM9Access(buffer,address,count,false,this->CurrentNDS);
		case ARM7_Bus:
			return ARM7Access(buffer,address,count,false,this->CurrentNDS);
	}
}

namespace luaMemoryDefinitions
{
//Macro to register lua_CFunction with 'name' to the "memory" library
#define AddMemoryFunction(functPointer,name)LuaFunctionRegister name(&functPointer,#name,&memoryFunctions)

memoryDomain get_currentMemoryDomain(LuaBundle* bundle){
	if (bundle->currentMemoryDomain!=nullptr) return *bundle->currentMemoryDomain;
	memoryDomain domain;
	NDS* CurrentNDS = bundle->getEmuInstance()->getNDS();
	const auto mainRamSize = CurrentNDS->ConsoleType == 1 ? melonDS::MainRAMMaxSize : melonDS::MainRAMMaxSize / 4;
	domain.name="Main RAM";
	domain.base=CurrentNDS->MainRAM;
	domain.size=mainRamSize;
	return domain;
}

/**
 * Finds active memoryDomain that matchs the optional string function argument `narg`,
 * returns currentMemoryDomain (Default "Main RAM") if if `narg` is absent/nil,
 * or if unable to find the memoryDomain / the memoryDomain is currently unused. 
 * (based off BizHawk's API)
**/
memoryDomain L_checkForMemoryDomain(lua_State* L, int narg){
	LuaBundle* bundle = get_bundle(L);
	if (lua_isnoneornil(L,narg)) return get_currentMemoryDomain(bundle);
	const char* name = luaL_checklstring(L,narg,NULL);
	NDS* CurrentNDS = bundle->getEmuInstance()->getNDS();
	memoryDomain domain;
	domain.CurrentNDS=CurrentNDS;
    #define ADD_MEMORY_DOMAIN(domain_name,domain_base,domain_size) if (std::strcmp(name,domain_name)==0){\
		domain.name=domain_name;\
		domain.base=domain_base;\
		domain.size=domain_size;\
		return domain;\
	}
    //https://github.com/TASEmulators/BizHawk/blob/master/waterbox/melon/BizDebugging.cpp#L266
    
    const auto mainRamSize = CurrentNDS->ConsoleType == 1 ? melonDS::MainRAMMaxSize : melonDS::MainRAMMaxSize / 4;
	ADD_MEMORY_DOMAIN("Main RAM", CurrentNDS->MainRAM, mainRamSize);
	ADD_MEMORY_DOMAIN("Shared WRAM", CurrentNDS->SharedWRAM, melonDS::SharedWRAMSize);
	ADD_MEMORY_DOMAIN("ARM7 WRAM", CurrentNDS->ARM7WRAM, melonDS::ARM7WRAMSize);

	if (auto* ndsCart = CurrentNDS->GetNDSCart())
	{
		ADD_MEMORY_DOMAIN("SRAM", CurrentNDS->GetNDSSave(), CurrentNDS->GetNDSSaveLength());
		ADD_MEMORY_DOMAIN("ROM", const_cast<u8*>(ndsCart->GetROM()), ndsCart->GetROMLength());
	}

	if (auto* gbaCart = CurrentNDS->GetGBACart())
	{
		ADD_MEMORY_DOMAIN("GBA SRAM", CurrentNDS->GetGBASave(), CurrentNDS->GetGBASaveLength());
		ADD_MEMORY_DOMAIN("GBA ROM", const_cast<u8*>(gbaCart->GetROM()), gbaCart->GetROMLength());
	}

	ADD_MEMORY_DOMAIN("Instruction TCM", CurrentNDS->ARM9.ITCM, melonDS::ITCMPhysicalSize);
	ADD_MEMORY_DOMAIN("Data TCM", CurrentNDS->ARM9.DTCM, melonDS::DTCMPhysicalSize);

	ADD_MEMORY_DOMAIN("ARM9 BIOS", const_cast<u8*>(CurrentNDS->GetARM9BIOS().data()), melonDS::ARM9BIOSSize);
	ADD_MEMORY_DOMAIN("ARM7 BIOS", const_cast<u8*>(CurrentNDS->GetARM7BIOS().data()), melonDS::ARM7BIOSSize);

	ADD_MEMORY_DOMAIN("Firmware", CurrentNDS->GetFirmware().Buffer(), CurrentNDS->GetFirmware().Length());

	if (CurrentNDS->ConsoleType == 1)
	{
		auto* dsi = static_cast<melonDS::DSi*>(CurrentNDS);
		ADD_MEMORY_DOMAIN("ARM9i BIOS", dsi->ARM9iBIOS.data(), melonDS::DSiBIOSSize);
		ADD_MEMORY_DOMAIN("ARM7i BIOS", dsi->ARM7iBIOS.data(), melonDS::DSiBIOSSize);

		ADD_MEMORY_DOMAIN("NWRAM A", dsi->NWRAM_A, melonDS::NWRAMSize);
		ADD_MEMORY_DOMAIN("NWRAM B", dsi->NWRAM_B, melonDS::NWRAMSize);
		ADD_MEMORY_DOMAIN("NWRAM C", dsi->NWRAM_C, melonDS::NWRAMSize);
	}
	// *Handeled seperatly
	if (std::strcmp(name,"ARM9 System Bus")==0){ 
		domain.name="ARM9 System Bus"; 
		domain.SystemBus = ARM9_Bus;
		domain.size=1ull << 32; 
		return domain; 
	}
	if (std::strcmp(name,"ARM7 System Bus")==0){ 
		domain.name="ARM7 System Bus"; 
		domain.SystemBus= ARM7_Bus; 
		domain.size=1ull << 32; 
		return domain; 
	}

    // DEFAULT
    // DomainName not found / is not in use:
	return get_currentMemoryDomain(bundle);
}

int Lua_getcurrentmemorydomain(lua_State* L)
{
	LuaBundle* bundle = get_bundle(L);
	const char* domainName = get_currentMemoryDomain(bundle).name;
	lua_pushstring(L,domainName);
	return 1;
}
AddMemoryFunction(Lua_getcurrentmemorydomain,getcurrentmemorydomain);

int Lua_getcurrentmemorydomainsize(lua_State* L)
{
	LuaBundle* bundle = get_bundle(L);
	u64 domainSize = get_currentMemoryDomain(bundle).size;
	lua_pushinteger(L,domainSize);
	return 1;
}
AddMemoryFunction(Lua_getcurrentmemorydomainsize,getcurrentmemorydomainsize);

int Lua_getmemorydomainlist(lua_State* L)
{
	LuaBundle* bundle = get_bundle(L);
	NDS* CurrentNDS = bundle->getEmuInstance()->getNDS();
	std::vector<const char*> domainList;
	domainList.push_back("Main RAM");
	domainList.push_back("Shared WRAM");
	domainList.push_back("ARM7 WRAM");
	if (auto* ndsCart = CurrentNDS->GetNDSCart())
	{
		domainList.push_back("SRAM");
		domainList.push_back("ROM");
	}
	if (auto* gbaCart = CurrentNDS->GetGBACart())
	{
		domainList.push_back("GBA SRAM");
		domainList.push_back("GBA ROM");
	}
	domainList.push_back("Instruction TCM");
	domainList.push_back("Data TCM");
	domainList.push_back("ARM9 BIOS");
	domainList.push_back("ARM7 BIOS");
	domainList.push_back("Firmware");
	if (CurrentNDS->ConsoleType == 1)
	{
		domainList.push_back("ARM9i BIOS");
		domainList.push_back("ARM7i BIOS");
		domainList.push_back("NWRAM A");
		domainList.push_back("NWRAM B");
		domainList.push_back("NWRAM C");
	}
	domainList.push_back("ARM9 System Bus");
	domainList.push_back("ARM7 System Bus");
	lua_createtable(L,domainList.size(),0);
	for(int i=1;i<=domainList.size();i++){
		lua_pushstring(L,domainList.at(i-1));
		lua_seti(L,-2,i);
	}
	return 1;
}
AddMemoryFunction(Lua_getmemorydomainlist,getmemorydomainlist);

int Lua_getmemorydomainsize(lua_State* L)
{
	memoryDomain domain = L_checkForMemoryDomain(L,1);
	lua_pushinteger(L,domain.size);
	return 1;
}
AddMemoryFunction(Lua_getmemorydomainsize,getmemorydomainsize);

//TODO: memory.hash_region

int Lua_usememorydomain(lua_State* L)
{
	const char* name = luaL_checklstring(L,1,NULL);
	memoryDomain domain = L_checkForMemoryDomain(L,1);
	if (std::strcmp(name,domain.name)==0){
		LuaBundle* bundle = get_bundle(L);
		bundle->currentMemoryDomain = &domain;
		lua_pushboolean(L,true);
		return 1;
	}
	lua_pushboolean(L,false);
	return 0;
}

int Lua_read_bytes_as_array(lua_State* L)
{
    LuaBundle* bundle = get_bundle(L);
    NDS* nds = bundle->getEmuInstance()->getNDS();
    u32 address = luaL_checkinteger(L,1);
    int length = luaL_checkinteger(L,2);
	memoryDomain domain = L_checkForMemoryDomain(L,3);

	u8 buff[length];
	domain.read(buff,address,length);

    lua_createtable(L, length, 0);
    for(int i=1;i<=length;i++){
		if(domain.size<address++)break;
        lua_pushinteger(L,buff[i-1]);
        lua_seti(L,-2,i);
    }
    return 1;
}
AddMemoryFunction(Lua_read_bytes_as_array,read_bytes_as_array);

//TODO: memory.read_bytes_as_dict

int Lua_write_bytes_as_array(lua_State* L)
{
    LuaBundle* bundle = get_bundle(L);
    NDS* nds = bundle->getEmuInstance()->getNDS();
    u32 address = luaL_checkinteger(L,1);
	memoryDomain domain = L_checkForMemoryDomain(L,3);
    luaL_checktype(L,2,LUA_TTABLE);//check arg2 is a TABLE
	std::vector<u8> buff;
	lua_geti(L,2,1);//push arg2[1] to top of stack
    for(int i=1;!lua_isnil(L,-1);lua_geti(L,2,++i)){//loop until arg2[i]==nil
		if(domain.size<(address+i-2))break;
        u8 byte = luaL_checkinteger(L,-1); 
		buff.push_back(byte);
    }

	domain.write(buff.data(),address,buff.size());
    return 0; 
}

//TODO: memory.write_bytes_as_dict

void byteswap(u8* arr,size_t size){
	for(int i = 0; i < size/2; i++) {
    	std::swap(arr[i], arr[size - i - 1]);
    }
	return;
}

template <typename T, bool Be=false> int Lua_WriteData(lua_State* L)
{
	u32 address = luaL_checkinteger(L,1);
	s64 value = luaL_checkinteger(L,2);
	u8* bits = (u8*)(&value)+(sizeof(s64)-sizeof(T));
	memoryDomain domain = L_checkForMemoryDomain(L,3);
	if (address>domain.size-sizeof(T))return 0;
	if (Be) byteswap(bits,sizeof(T));
	//Using Two's complement so no need to check for sign...
	domain.write(bits,address,sizeof(T));
	return 0;
}

//Scuffed support for 'dummy' 24bit types...
namespace dummyIntType{
	struct u24{
		u8 bytes[3];
	};
	static_assert(sizeof(u24)==3);
	struct s24{
		u8 bytes[3];
	};
	static_assert(sizeof(s24)==3);
	template <typename T> constexpr bool isSigned = ((T)0 - 1 > 0);
	template <> constexpr bool isSigned<u24> = false;
	template <> constexpr bool isSigned<s24> = true;
}

template <typename T, bool Be=false> int Lua_ReadData(lua_State* L) 
{
	u32 address = luaL_checkinteger(L,1);
	memoryDomain domain = L_checkForMemoryDomain(L,2);
	s64 value=0;
	u8* bits = (u8*)(&value)+(sizeof(s64)-sizeof(T));
	if (address<=domain.size-sizeof(T))
		domain.read(bits,address,sizeof(T));
	if (Be) byteswap(bits,sizeof(T));
	if (dummyIntType::isSigned<T>){
		//if MSB is negative, set all 'unused' bytes in the s64 to 0xff 
		if((*(s8*)(bits))<0) memset(&value,0xff,(sizeof(s64)-sizeof(T)));
	}
    lua_pushinteger(L, value);
    return 1;
}

#define LE ,false
#define BE ,true
#define AddMemoryReadFunction(dataType,typename)\
	int Lua_##typename(lua_State*L){return Lua_ReadData<dataType>(L);};\
	AddMemoryFunction(Lua_##typename,typename)

typedef dummyIntType::u24 u24;
typedef dummyIntType::s24 s24;

AddMemoryReadFunction(s16 BE, read_s16_be);
AddMemoryReadFunction(s16 LE, read_s16_le);
AddMemoryReadFunction(s24 BE, read_s24_be);
AddMemoryReadFunction(s24 LE, read_s24_le);
AddMemoryReadFunction(s32 BE, read_s32_be);
AddMemoryReadFunction(s32 LE, read_s32_le);
AddMemoryReadFunction(s8    , read_s8);
AddMemoryReadFunction(u16 BE, read_u16_be);
AddMemoryReadFunction(u16 LE, read_u16_le);
AddMemoryReadFunction(u24 BE, read_u24_be);
AddMemoryReadFunction(u24 LE, read_u24_le);
AddMemoryReadFunction(u32 BE, read_u32_be);
AddMemoryReadFunction(u32 LE, read_u32_le);
AddMemoryReadFunction(u8    , read_u8);
AddMemoryReadFunction(u8    , readbyte);

#define AddMemoryWriteFunction(dataType,typename)\
	int Lua_##typename(lua_State*L){return Lua_WriteData<dataType>(L);};\
	AddMemoryFunction(Lua_##typename,typename)

AddMemoryWriteFunction(s16 BE, write_s16_be);
AddMemoryWriteFunction(s16 LE, write_s16_le);
AddMemoryWriteFunction(s24 BE, write_s24_be);
AddMemoryWriteFunction(s24 LE, write_s24_le);
AddMemoryWriteFunction(s32 BE, write_s32_be);
AddMemoryWriteFunction(s32 LE, write_s32_le);
AddMemoryWriteFunction(s8    , write_s8);
AddMemoryWriteFunction(u16 BE, write_u16_be);
AddMemoryWriteFunction(u16 LE, write_u16_le);
AddMemoryWriteFunction(u24 BE, write_u24_be);
AddMemoryWriteFunction(u24 LE, write_u24_le);
AddMemoryWriteFunction(u32 BE, write_u32_be);
AddMemoryWriteFunction(u32 LE, write_u32_le);
AddMemoryWriteFunction(u8    , write_u8);
AddMemoryWriteFunction(u8    , writebyte);
}