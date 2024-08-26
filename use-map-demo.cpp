#include "stdafx.h"

_NT_BEGIN

#include "map.h"

class M1 : public MElement
{
public:
	ULONG v;
	char str[];

	~M1()
	{
		DbgPrint("%s<%p>(%u \"%hs\")\n", __FUNCTION__, this, v, str);
	}

private:

	virtual const void* key() const
	{
		return str;
	}

public:

	M1(PCSTR pcsz, ULONG v = 0) : v(v)
	{
		strcpy(str, pcsz);
		DbgPrint("%s<%p>(%u \"%hs\")\n", __FUNCTION__, this, v, str);
	}

	M1(PCSTR pcsz, size_t len, ULONG v = 0) : v(v)
	{
		memcpy(str, pcsz, len);
		str[len] = 0;
		DbgPrint("%s<%p>(%u \"%hs\")\n", __FUNCTION__, this, v, str);
	}

};

class M1InsertRemove : public InsertRemove
{
	ULONG v;

	virtual void OnInsert(ElementBase* p) const
	{
		new(p) M1((PCSTR)key(), v);
	}

public:
	M1InsertRemove(const void* pvKey, ULONG v = 0) : InsertRemove(pvKey), v(v)
	{
	}
};

class M2InsertRemove : public InsertRemove
{
	size_t len;
	ULONG v;

	virtual void OnInsert(ElementBase* p) const
	{
		new(p) M1((PCSTR)key(), len, v);
	}

public:
	M2InsertRemove(const void* pvKey, size_t len, ULONG v = 0) : InsertRemove(pvKey), v(v), len(len)
	{
	}
};

struct StrMap : MapBase
{
	virtual RTL_GENERIC_COMPARE_RESULTS comp(_In_ const void* p, _In_ const void* q)
	{
		int i = strcmp((const char*)p, (const char*)q);
		if (0 > i) return GenericLessThan;
		if (0 < i) return GenericGreaterThan;
		return GenericEqual;
	}
};

NTSTATUS Insert(StrMap* map, PCSTR str, ULONG v, M1 ** pp = 0)
{
	return map->Insert(M1InsertRemove(str, v), offsetof(M1, str[strlen(str) + 1]), reinterpret_cast<MElement**>(pp));
}

void print_map(StrMap* map)
{
	void* Key = 0;

	DbgPrint("%x: +++++++++++\n", map->Count());
	while (M1* next = static_cast<M1*>(map->Next(&Key)))
	{
		DbgPrint("%p: [%u \"%hs\"]\n", next, next->v, next->str);
	}
	DbgPrint("%x: -----------\n", map->Count());
}

void SimplyDemo()
{
	StrMap map;

	//map.SetNoLock();

	Insert(&map, "GPU", 15);
	Insert(&map, "CPU", 10);
	Insert(&map, "RAM", 20);

	print_map(&map);

	M1* p;

	if (0 <= Insert(&map, "CPU", 25, &p))
	{
		p->Release();
	}

	switch (Insert(&map, "RAM", 22, &p))
	{
	case STATUS_OBJECT_NAME_EXISTS:
		p->v = 22;
	case STATUS_SUCCESS:
		p->Release();
		break;
	}

	Insert(&map, "UPS", 0);
	Insert(&map, "SSD", 30);

	print_map(&map);

	map.Erase("XPU");
	map.Erase("GPU");

	PVOID Node;
	if (p = static_cast<M1*>(map.Get("SSD", &Node)))
	{
		map.Remove(p, Node);
		p->Release();
	}

	if (p = static_cast<M1*>(map[map.Count() / 2]))
	{
		map.Remove(p);
		p->Release();
	}

	print_map(&map);
}

//////////////////////////////////////////////////////////////////////////

#include <C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.36.32532\include\delayimp.h>

struct __declspec(novtable) WalkImport
{
	union {
		PIMAGE_IMPORT_DESCRIPTOR piid;
		PCImgDelayDescr pdid;
		PVOID pv;
	};

	virtual USHORT DirectoryEntry() = 0;
	virtual ULONG size() = 0;
	virtual ULONG Name() = 0;
	virtual void Next() = 0;
	virtual ULONG rvaINT() = 0;
	virtual ULONG rvaIAT() = 0;

	PVOID Init(PVOID hmod, PULONG size)
	{
		return pv = RtlImageDirectoryEntryToData(hmod, TRUE, DirectoryEntry(), size);
	}
};

struct CImport : WalkImport
{
	virtual USHORT DirectoryEntry()
	{
		return IMAGE_DIRECTORY_ENTRY_IMPORT;
	}

	virtual ULONG size()
	{
		return sizeof(IMAGE_IMPORT_DESCRIPTOR);
	}

	virtual ULONG Name()
	{
		return piid->Name;
	}

	virtual void Next()
	{
		++piid;
	}

	virtual ULONG rvaINT()
	{
		return piid->OriginalFirstThunk;
	}

	virtual ULONG rvaIAT()
	{
		return piid->FirstThunk;
	}
};

struct CDImport : WalkImport
{
	virtual USHORT DirectoryEntry()
	{
		return IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT;
	}

	virtual ULONG size()
	{
		return sizeof(ImgDelayDescr);
	}

	virtual ULONG Name()
	{
		return pdid->grAttrs & dlattrRva ? pdid->rvaDLLName : 0;
	}

	virtual void Next()
	{
		++pdid;
	}

	virtual ULONG rvaINT()
	{
		return pdid->rvaINT;
	}

	virtual ULONG rvaIAT()
	{
		return pdid->rvaIAT;
	}
};

void AddImport(_In_ HANDLE hSection, StrMap* map)
{
	SIZE_T ViewSize = 0;
	PVOID BaseAddress = 0;

	if (0 <= ZwMapViewOfSection(hSection, NtCurrentProcess(), &BaseAddress, 0, 0, 0, 
		&ViewSize, ViewUnmap, 0, PAGE_READONLY))
	{
		if (PIMAGE_NT_HEADERS pinth = RtlImageNtHeader(BaseAddress))
		{
			if (pinth->FileHeader.Machine == IMAGE_FILE_MACHINE_AMD64)
			{
				CDImport a;
				CImport b;

				WalkImport* arr[] = {&a, &b}, *p;

				ULONG i = _countof(arr);
				do 
				{
					p = arr[--i];

					ULONG s;
					if (p->Init(BaseAddress, &s))
					{
						if (s /= p->size())
						{
							do 
							{
								ULONG name = p->Name();

								if (!name)
								{
									break;
								}

								M1* pDLL;

								if (0 <= Insert(map, RtlOffsetToPointer(BaseAddress, name), 0, &pDLL))
								{
									pDLL->v++;
									pDLL->Release();
								}

								p->Next();

							} while (--s);
						}
					}


				} while (i);
			}
		}

		ZwUnmapViewOfSection(NtCurrentProcess(), BaseAddress);
	}
}

NTSTATUS EnumImports(StrMap* map)
{
	HANDLE hFile;
	IO_STATUS_BLOCK iosb;
	UNICODE_STRING ObjectName;
	OBJECT_ATTRIBUTES oa = { sizeof(oa), 0, &ObjectName, OBJ_CASE_INSENSITIVE };

	RtlInitUnicodeString(&ObjectName, L"\\systemroot\\system32");

	NTSTATUS status = NtOpenFile(&oa.RootDirectory,
		FILE_LIST_DIRECTORY | SYNCHRONIZE, &oa, &iosb, FILE_SHARE_READ,
		FILE_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT);

	if (0 <= status)
	{
		status = STATUS_NO_MEMORY;

		enum { buf_size = 0x10000 };

		if (PVOID buf = LocalAlloc(0, buf_size))
		{
			static const UNICODE_STRING DLL = RTL_CONSTANT_STRING(L"*.dll");

			while (0 <= (status = NtQueryDirectoryFile(oa.RootDirectory, 
				0, 0, 0, &iosb, buf, buf_size, FileDirectoryInformation,
				FALSE, const_cast<PUNICODE_STRING>(&DLL), FALSE)))
			{
				union {
					PVOID pv;
					PUCHAR pc;
					PFILE_DIRECTORY_INFORMATION pfdi;
				};

				pv = buf;

				ULONG NextEntryOffset = 0;

				do 
				{
					pc += NextEntryOffset;

					ObjectName.Buffer = pfdi->FileName;
					ObjectName.MaximumLength = ObjectName.Length = (USHORT)pfdi->FileNameLength;

					if (0 <= NtOpenFile(&hFile, FILE_READ_DATA|SYNCHRONIZE, &oa, &iosb, 
						FILE_SHARE_READ, FILE_NON_DIRECTORY_FILE|FILE_SYNCHRONOUS_IO_NONALERT))
					{
						HANDLE hSection;

						if (0 <= NtCreateSection(&hSection, SECTION_MAP_READ, 0, 0, PAGE_READONLY, SEC_IMAGE_NO_EXECUTE, hFile))
						{
							AddImport(hSection, map);

							NtClose(hSection);
						}

						NtClose(hFile);
					}

				} while (NextEntryOffset = pfdi->NextEntryOffset);
			}

			LocalFree(buf);
		}

		NtClose(oa.RootDirectory);
	}

	return status;
}

int __cdecl compare(const void * pp, const void* pq)
{
	ULONG a = (*(M1**)pp)->v, b = (*(M1**)pq)->v;

	if (a < b) return -1;
	if (a > b) return +1;
	return 0;
}

void Test2()
{
	StrMap map;
	map.SetNoLock();
	EnumImports(&map);

	if (ULONG n = map.Count())
	{
		if (M1** pp = new M1*[n])
		{
			pp += n;

			void* key = 0;
			while (M1* p = static_cast<M1*>(map.Next(&key)))
			{
				*--pp = p;
			}

			qsort(pp, n, sizeof(M1*), compare);

			pp += n;
			do 
			{
				M1* p = *--pp;
				DbgPrint("%u \"%hs\"\n", p->v, p->str);
			} while (--n);

			delete [] pp;
		}
	}
}

BOOL IsSymbol(UCHAR c)
{
	return (ULONG)(c - 'A') <= 'Z' - 'A' || (ULONG)(c - 'a') < 'z' - 'a' || (ULONG)(c - '0') <= '9' - '0';
}

void TestI()
{
	StrMap map;
	map.SetNoLock();

	PCSTR pcsz = 
		"Validates the authenticated user to either login to an existing user\n"
		"profile or fall back to creation of a new user profile. Below are few\n"
		"workflows.";

__0:

	PCSTR pc = pcsz;
	while (IsSymbol(*pc)) pc++;

	size_t len = pc - pcsz;

	char buf[0x40];
	if (len < sizeof(buf))
	{
		memcpy(buf, pcsz, len);
		buf[len] = 0;
		map.Insert(M2InsertRemove(buf, len, 0), offsetof(M1, str[len + 1]));
	}

	pcsz = pc;

	UCHAR c;

	while (!IsSymbol(c = *pcsz))
	{
		if (!c)
		{
			goto __1;
		}
		pcsz++;
	}

	goto __0;
__1:

	print_map(&map);

	map.Invert();

	print_map(&map);
}

void MapTest()
{
	TestI();
	SimplyDemo();
	Test2();
}

_NT_END