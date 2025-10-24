#pragma once

#ifndef SetFreeMemory
#define SetFreeMemory(pv) ExFreePoolWithTag(pv, ':tes')
#endif

#ifndef SetAllocMemory
#define SetAllocMemory(cb) ExAllocatePoolWithTag(NonPagedPoolNx, cb, ':tes')
#endif

class __declspec(novtable) ElementBase 
{
  friend class SetBase;

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
  friend class SetBase;

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
    SetFreeMemory(GetAllocationBase(pv));
  }
};

class SetBase : protected RTL_AVL_TABLE
{
  static RTL_GENERIC_COMPARE_RESULTS NTAPI compare (
    _In_ PRTL_AVL_TABLE Table,
    _In_ PVOID FirstStruct,
    _In_ PVOID SecondStruct
    );

  static PVOID NTAPI alloc(_In_ PRTL_AVL_TABLE Table, _In_ CLONG ByteSize);

  static VOID NTAPI free (_In_ PRTL_AVL_TABLE Table, _In_ PVOID Buffer);

  void Delete(PRTL_BALANCED_LINKS node);

  virtual RTL_GENERIC_COMPARE_RESULTS KeyCompare(_In_ const void* p, _In_ const void* q);

  virtual PVOID valloc(_In_ CLONG ByteSize);

  bool ForEach(PRTL_BALANCED_LINKS node, bool (WINAPI* proc)(void*, MElement*), const void* ctx);
protected:
  EX_PUSH_LOCK _M_SRWLock {};

public:

  SetBase()
  {
    RtlInitializeGenericTableAvl(this, compare, alloc, free, 0);
  }

  ~SetBase()
  {
    Delete(&BalancedRoot);
  }

  void SetNoLock()
  {
    _M_SRWLock = (ULONG_PTR)&_M_SRWLock;
  }

  void Lock()
  {
    if ((ULONG_PTR)&_M_SRWLock != _M_SRWLock)
    {
      KeEnterCriticalRegion();
      ExfAcquirePushLockExclusive(&_M_SRWLock);
    }
  }

  void Unlock()
  {
    if ((ULONG_PTR)&_M_SRWLock != _M_SRWLock)
    {
      ExfReleasePushLockExclusive(&_M_SRWLock);
      KeLeaveCriticalRegion();
    }
  }

  void LockShared()
  {
    if ((ULONG_PTR)&_M_SRWLock != _M_SRWLock)
    {
      KeEnterCriticalRegion();
      ExfAcquirePushLockShared(&_M_SRWLock);
    }
  }

  void UnlockShared()
  {
    if ((ULONG_PTR)&_M_SRWLock != _M_SRWLock)
    {
      ExfReleasePushLockShared(&_M_SRWLock);
      KeLeaveCriticalRegion();
    }
  }

  NTSTATUS Insert(_In_ const InsertRemove& pi, size_t cb, _Out_opt_ MElement** ppv = 0);
  NTSTATUS InsertLocked(_In_ const InsertRemove& pi, size_t cb, _Out_opt_ MElement** ppv = 0);

  MElement* Get(_In_ const void* pvKey);
  MElement* GetLocked(_In_ const void* pvKey, _Out_opt_ PVOID *NodeOrParent = 0);
  BOOLEAN IsExist(_In_ const void* pvKey);

  void Remove(_In_ MElement* p, _In_opt_ PVOID Node = 0, BOOL bNotLock = FALSE);

  BOOLEAN Erase(_In_ const void* pvKey, _Out_opt_ MElement** ppv = 0);

  MElement* Next(_Inout_ void** Key)
  {
    return static_cast<MElement*>(RtlEnumerateGenericTableWithoutSplayingAvl(this, Key));
  }

  ULONG Count()
  {
    return RtlNumberGenericTableElementsAvl(this);
  }

  void Invert();

  void ForEach(bool (WINAPI* proc)(void*, MElement*), const void* ctx)
  {
    LockShared();
    ForEach(BalancedRoot.RightChild, proc, ctx);
    UnlockShared();
  }

  void MultiRemove(bool (WINAPI* NeedRemove)(void*, MElement*), const void* ctx);

  MElement* operator[](_In_ ULONG i);
};