#include "stdafx.h"

_NT_BEGIN

#include "set.h"

//////////////////////////////////////////////////////////////////////////
// #1

struct Func1 : public MElement 
{
	PCSTR _M_name;
	PVOID _M_addr;

	virtual const void* key() const
	{
		return _M_name;
	}

	Func1(PCSTR name, PVOID addr) : _M_name(name), _M_addr(addr)
	{
	}

	//++ optional, demo only
	void operator delete(void* pv)
	{
		DbgPrint("--%p\n", pv = GetAllocationBase(pv));
		SetFreeMemory((pv));
	}
	//-- optional, demo only
};

class InsertFunc1 : public InsertRemove
{
	PVOID _M_addr;

	virtual void OnInsert(ElementBase* p) const
	{
		new(p) Func1((PCSTR)key(), _M_addr);
	}
public:

	InsertFunc1(PCSTR name, PVOID addr) : InsertRemove(name), _M_addr(addr)
	{
	}
};

class FuncSet : public SetBase
{
	virtual RTL_GENERIC_COMPARE_RESULTS KeyCompare(_In_ const void* p, _In_ const void* q)
	{
		int i = strcmp((PCSTR)p, (PCSTR)q);
		if (0 > i) return GenericLessThan;
		if (0 < i) return GenericGreaterThan;
		return GenericEqual;
	}

	//++ optional, demo only
	virtual PVOID valloc(_In_ CLONG ByteSize)
	{
		PVOID pv = SetAllocMemory(ByteSize);
		DbgPrint("++%p\n", pv);
		return pv;
	}
	//-- optional, demo only
};

void AddFromModule(SetBase& funcs, HMODULE hmod, PULONG pn, PULONG pm)
{
	ULONG s;
	if (PIMAGE_EXPORT_DIRECTORY pied = (PIMAGE_EXPORT_DIRECTORY)RtlImageDirectoryEntryToData(hmod, TRUE, IMAGE_DIRECTORY_ENTRY_EXPORT, &s))
	{
		if (ULONG NumberOfNames = pied->NumberOfNames)
		{
			ULONG NumberOfFunctions = pied->NumberOfFunctions;
			PUSHORT AddressOfNameOrdinals = (PUSHORT)RtlOffsetToPointer(hmod, pied->AddressOfNameOrdinals);
			PULONG AddressOfNames = (PULONG)RtlOffsetToPointer(hmod, pied->AddressOfNames);
			PULONG AddressOfFunctions = (PULONG)RtlOffsetToPointer(hmod, pied->AddressOfFunctions);
			do 
			{
				PCSTR name = RtlOffsetToPointer(hmod, *AddressOfNames++);
				ULONG Ordinal = *AddressOfNameOrdinals++;

				if (Ordinal < NumberOfFunctions)
				{
					switch (funcs.Insert(InsertFunc1(name, RtlOffsetToPointer(hmod, AddressOfFunctions[Ordinal])), sizeof(Func1)))
					{
					case STATUS_OBJECT_NAME_EXISTS:
						++*pm;
						break;
					case STATUS_SUCCESS:
						++*pn;
						break;
					default:
						__debugbreak();
					}
				}

			} while (--NumberOfNames);
		}
	}
}

bool WINAPI proc(void*, MElement* func)
{
	DbgPrint("%p %hs\n", static_cast<Func1*>(func)->_M_addr, static_cast<Func1*>(func)->_M_name);
	return true;
}

bool WINAPI NeedRemove(void* ch, MElement* func)
{
	return (char)(ULONG_PTR)ch == *static_cast<Func1*>(func)->_M_name;
}

void STest1()
{
	FuncSet funcs;
	funcs.SetNoLock();
	ULONG m = 0, n = 0;
	AddFromModule(funcs, GetModuleHandleW(L"kernel32"), &m, &n);
	AddFromModule(funcs, GetModuleHandleW(L"kernelbase"), &m, &n);
	funcs.ForEach(proc, 0);
	funcs.MultiRemove(NeedRemove, (void*)'G');

	if (Func1* func = static_cast<Func1*>(funcs.Get("_OpenMuiStringCache")))
	{
		func->Release();

		if (funcs.Erase("WriteFileEx", (MElement**)&func))
		{
			func->Release();
		}

		funcs.Erase("LeaveCriticalSection");
	}

	funcs.Invert();
	// order is breaked
	funcs.ForEach(proc, 0);

	__nop();
}

_NT_END