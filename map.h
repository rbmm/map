#pragma once

class __declspec(novtable) ElementBase 
{
	friend class MapBase;

protected:

	virtual ~ElementBase() = default;

public:

	virtual const void* key() const = 0;
};

class InsertRemove : public ElementBase
{
	const void* _M_key;
	
protected:

	virtual const void* key() const
	{
		return _M_key;
	}

public:

	virtual void OnInsert(ElementBase* ) const
	{
		__debugbreak();
	}

	InsertRemove(const void* pvKey) : _M_key(pvKey)
	{
	}
};

class __declspec(novtable) MElement : public ElementBase
{
	friend class MapBase;

	LONG _M_dwRef = 1;

protected:

	void* operator new(size_t)
	{
		return 0;
	}

public:

	void* operator new(size_t, ElementBase* pv)
	{
		return pv;
	}

	static void* GetAllocationBase(void* pv)
	{
		return *((void**)pv - 1);
	}

	void AddRef()
	{
		InterlockedIncrementNoFence(&_M_dwRef);
	}

	void Release()
	{
		if (!InterlockedDecrement(&_M_dwRef))
		{
			delete this;
		}
	}

	void operator delete(void* pv)
	{
		LocalFree(GetAllocationBase(pv));
	}
};

class __declspec(novtable) MapBase : RTL_AVL_TABLE
{
	static RTL_GENERIC_COMPARE_RESULTS NTAPI compare (
		_In_ PRTL_AVL_TABLE Table,
		_In_ PVOID FirstStruct,
		_In_ PVOID SecondStruct
		)
	{
		return static_cast<MapBase*>(Table)->comp(
			reinterpret_cast<MElement*>(FirstStruct)->key(), reinterpret_cast<MElement*>(SecondStruct)->key());
	}

	static PVOID NTAPI alloc(_In_ PRTL_AVL_TABLE Table, _In_ CLONG ByteSize)
	{
		return static_cast<MapBase*>(Table)->valloc(ByteSize + (ULONG)(ULONG_PTR)Table->TableContext);
	}

	static VOID NTAPI free (_In_ PRTL_AVL_TABLE Table, _In_ PVOID Buffer)
	{
		*(void**)Table->TableContext = Buffer;
	}

	void Delete(PRTL_BALANCED_LINKS node);

	virtual RTL_GENERIC_COMPARE_RESULTS comp(_In_ const void* p, _In_ const void* q) = 0;

	virtual PVOID valloc(_In_ CLONG ByteSize)
	{
		return LocalAlloc(LMEM_FIXED, ByteSize);
	}

protected:
	SRWLOCK _M_SRWLock {};

public:

	MapBase()
	{
		RtlInitializeGenericTableAvl(this, compare, alloc, free, 0);
	}

	~MapBase()
	{
		Delete(&BalancedRoot);
	}

	void SetNoLock()
	{
		_M_SRWLock.Ptr = &_M_SRWLock;
	}

	void Lock()
	{
		if (&_M_SRWLock != _M_SRWLock.Ptr) AcquireSRWLockExclusive(&_M_SRWLock);
	}

	void Unlock()
	{
		if (&_M_SRWLock != _M_SRWLock.Ptr) ReleaseSRWLockExclusive(&_M_SRWLock);
	}

	NTSTATUS Insert(_In_ const InsertRemove& pi, size_t cb, _Out_opt_ MElement** ppv = 0);
	NTSTATUS InsertLocked(_In_ const InsertRemove& pi, size_t cb, _Out_opt_ MElement** ppv = 0);

	MElement* Get(_In_ const void* pvKey, _Out_opt_ PVOID *NodeOrParent = 0);
	MElement* GetLocked(_In_ const void* pvKey, _Out_opt_ PVOID *NodeOrParent = 0);

	void Remove(_In_ MElement* p, _In_opt_ PVOID Node = 0);

	BOOLEAN Erase(_In_ const void* pvKey);

	MElement* Next(_Inout_ void** Key)
	{
		return static_cast<MElement*>(RtlEnumerateGenericTableWithoutSplayingAvl(this, Key));
	}

	ULONG Count()
	{
		return RtlNumberGenericTableElementsAvl(this);
	}

	MElement* operator[](_In_ ULONG i);
};