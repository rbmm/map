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

struct KAN 
{
	UCHAR m_bNonce[SHA1_DIGEST_SIZE];
	UCHAR m_bKey[16];
};

class ClientContext : public MElement
{
public:
	ULONG64 m_time = GetTickCount64() + 4000;
	WCHAR m_UserName[(2 + 4 + 15 + (11 * 5) + 2)];
	KAN m_k;
	UCHAR m_bMachineGuid[16] = {};

	virtual const void* key() const
	{
		return m_k.m_bKey;
	}

	~ClientContext()
	{
		DbgPrint("%hs<%p>(%ws) [%d]\r\n", __FUNCTION__, this, m_UserName, GetTickCount64() - m_time);
	}

	ClientContext()
	{
		*m_UserName = 0;
		DbgPrint("%hs<%p>\r\n", __FUNCTION__, this);
	}
};

class CCIr : public InsertRemove
{
	virtual const void* key() const
	{
		return reinterpret_cast<const KAN*>(__super::key())->m_bKey;
	}

	virtual void OnInsert(ElementBase* p) const
	{
		memcpy(&(new(p) ClientContext)->m_k, __super::key(), sizeof(KAN));
	}

	using InsertRemove::InsertRemove;
};

class SDB : public SetBase
{
	virtual RTL_GENERIC_COMPARE_RESULTS KeyCompare(_In_ const void* p, _In_ const void* q)
	{
		int i = memcmp(p, q, 16);
		if (0 > i) return GenericLessThan;
		if (0 < i) return GenericGreaterThan;
		return GenericEqual;
	}
};

NTSTATUS Insert(SDB& db, _Out_ KAN* p)
{
	if (NTSTATUS status = BCryptGenRandom(0, (PBYTE)p, sizeof(KAN), BCRYPT_USE_SYSTEM_PREFERRED_RNG))
	{
		return status;
	}
	return db.Insert(CCIr(p), sizeof(ClientContext));
}

bool WINAPI NeedRemove(void* time, MElement* p)
{
	return static_cast<ClientContext*>(p)->m_time < (ULONG_PTR)time;
}

void ts1()
{
	SDB db;
	KAN kan;
	ULONG n = 16;
	do 
	{
		Insert(db, &kan);
		Sleep(500);
	} while (--n);

	if (ClientContext* ctx = static_cast<ClientContext*>(db.Get(kan.m_bKey)))
	{
		wcscpy_s(ctx->m_UserName, _countof(ctx->m_UserName), L"S-1-5-21-3032776714-2974785564-2375659916-1001");
		ctx->Release();
	}

	db.MultiRemove(NeedRemove, (PVOID)(ULONG_PTR)GetTickCount64());

	RtlZeroMemory(kan.m_bNonce, sizeof(kan.m_bNonce));
	MElement* ctx;
	if (db.Erase(kan.m_bKey, &ctx))
	{
		DbgPrint(":: %ws\r\n", static_cast<ClientContext*>(ctx)->m_UserName);
		ctx->Release();
	}

	Sleep(1000);
	db.MultiRemove(NeedRemove, (PVOID)(ULONG_PTR)GetTickCount64());
	DbgPrint("***********\r\n");
}

/************************************************************************/
/* 
[2026-05-02 11:01:02] NT::ClientContext::ClientContext<000001827AD1F520>
[2026-05-02 11:01:03] NT::ClientContext::ClientContext<000001827AD1BD00>
[2026-05-02 11:01:03] NT::ClientContext::ClientContext<000001827AD1B0C0>
[2026-05-02 11:01:04] NT::ClientContext::ClientContext<000001827AD2E0F0>
[2026-05-02 11:01:05] NT::ClientContext::ClientContext<000001827AD2E230>
[2026-05-02 11:01:05] NT::ClientContext::ClientContext<000001827AD2E370>
[2026-05-02 11:01:06] NT::ClientContext::ClientContext<000001827AD2E610>
[2026-05-02 11:01:06] NT::ClientContext::ClientContext<000001827AD2E750>
[2026-05-02 11:01:07] NT::ClientContext::ClientContext<000001827AD2E890>
[2026-05-02 11:01:07] NT::ClientContext::ClientContext<000001827AD2E9D0>
[2026-05-02 11:01:08] NT::ClientContext::ClientContext<000001827AD2EB10>
[2026-05-02 11:01:08] NT::ClientContext::ClientContext<000001827AD2EC50>
[2026-05-02 11:01:09] NT::ClientContext::ClientContext<000001827AD2ED90>
[2026-05-02 11:01:09] NT::ClientContext::ClientContext<000001827AD2EED0>
[2026-05-02 11:01:10] NT::ClientContext::ClientContext<000001827AD2F010>
[2026-05-02 11:01:10] NT::ClientContext::ClientContext<000001827AD2F150>
[2026-05-02 11:01:11] NT::ClientContext::~ClientContext<000001827AD2E750>() [594]
[2026-05-02 11:01:11] NT::ClientContext::~ClientContext<000001827AD2E0F0>() [2625]
[2026-05-02 11:01:11] NT::ClientContext::~ClientContext<000001827AD1B0C0>() [3141]
[2026-05-02 11:01:11] NT::ClientContext::~ClientContext<000001827AD2E890>() [78]
[2026-05-02 11:01:11] NT::ClientContext::~ClientContext<000001827AD2E230>() [2125]
[2026-05-02 11:01:11] NT::ClientContext::~ClientContext<000001827AD1F520>() [4156]
[2026-05-02 11:01:11] NT::ClientContext::~ClientContext<000001827AD2E610>() [1094]
[2026-05-02 11:01:11] NT::ClientContext::~ClientContext<000001827AD2E370>() [1610]
[2026-05-02 11:01:11] NT::ClientContext::~ClientContext<000001827AD1BD00>() [3641]
[2026-05-02 11:01:11] :: S-1-5-21-3032776714-2974785564-2375659916-1001
[2026-05-02 11:01:11] NT::ClientContext::~ClientContext<000001827AD2F150>(S-1-5-21-3032776714-2974785564-2375659916-1001) [-3484]
[2026-05-02 11:01:12] NT::ClientContext::~ClientContext<000001827AD2E9D0>() [593]
[2026-05-02 11:01:12] NT::ClientContext::~ClientContext<000001827AD2EB10>() [78]
[2026-05-02 11:01:12] ***********
[2026-05-02 11:01:12] NT::ClientContext::~ClientContext<000001827AD2F010>() [-1954]
[2026-05-02 11:01:12] NT::ClientContext::~ClientContext<000001827AD2EC50>() [-438]
[2026-05-02 11:01:12] NT::ClientContext::~ClientContext<000001827AD2EED0>() [-1454]
[2026-05-02 11:01:12] NT::ClientContext::~ClientContext<000001827AD2ED90>() [-938]
*/
/************************************************************************/

_NT_END