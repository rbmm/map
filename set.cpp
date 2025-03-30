#include "stdafx.h"

_NT_BEGIN

#include "set.h"

RTL_GENERIC_COMPARE_RESULTS NTAPI SetBase::compare (
	_In_ PRTL_AVL_TABLE Table,
	_In_ PVOID FirstStruct,
	_In_ PVOID SecondStruct
	)
{
	return static_cast<SetBase*>(Table)->KeyCompare(
		reinterpret_cast<MElement*>(FirstStruct)->key(), reinterpret_cast<MElement*>(SecondStruct)->key());
}

PVOID NTAPI SetBase::alloc(_In_ PRTL_AVL_TABLE Table, _In_ CLONG ByteSize)
{
	return static_cast<SetBase*>(Table)->valloc(ByteSize + (ULONG)(ULONG_PTR)Table->TableContext);
}

VOID NTAPI SetBase::free (_In_ PRTL_AVL_TABLE Table, _In_ PVOID Buffer)
{
	*(void**)Table->TableContext = Buffer;
}

RTL_GENERIC_COMPARE_RESULTS SetBase::KeyCompare(_In_ const void* p, _In_ const void* q)
{
	if (p < q) return GenericLessThan;
	if (p > q) return GenericGreaterThan;
	return GenericEqual;
}

PVOID SetBase::valloc(_In_ CLONG ByteSize)
{
	return SetAllocMemory(ByteSize);
}

NTSTATUS SetBase::InsertLocked(const InsertRemove& pi, size_t cb, _Out_opt_ MElement** ppv /*= 0*/)
{
	BOOLEAN NewElement;
	NTSTATUS status = STATUS_NO_MEMORY;

	TableContext = (PVOID)cb;

	if (MElement* p = (MElement*)RtlInsertElementGenericTableAvl(this, (void*)&pi, 0, &NewElement))
	{
		status = STATUS_OBJECT_NAME_EXISTS;

		if (NewElement)
		{
			pi.OnInsert(p);
			status = STATUS_SUCCESS;
		}

		if (ppv)
		{
			p->AddRef();
			*ppv = p;
		}
	}

	TableContext = 0;

	return status;
}

NTSTATUS SetBase::Insert(const InsertRemove& pi, size_t cb, _Out_opt_ MElement** ppv /*= 0*/)
{
	Lock();

	NTSTATUS status = InsertLocked(pi, cb, ppv);
	
	Unlock();

	return status;
}

MElement* SetBase::GetLocked(const void* pvKey, _Out_ PVOID *NodeOrParent)
{
	InsertRemove ir(pvKey);

	TABLE_SEARCH_RESULT SearchResult;

	PVOID Node;

	if (!NodeOrParent)
	{
		NodeOrParent = &Node;
	}

	if (MElement* p = (MElement*)RtlLookupElementGenericTableFullAvl(this, &ir, NodeOrParent, &SearchResult))
	{
		p->AddRef();
		return p;
	}

	return 0;
}

MElement* SetBase::Get(const void* pvKey)
{
	LockShared();

	MElement* p = GetLocked(pvKey);

	UnlockShared();

	return p;
}

void SetBase::Remove(MElement* p, PVOID Node, BOOL bNotLock)
{
	if (Node && !bNotLock)
	{
		__debugbreak();
	}

	if (!bNotLock) Lock();

	TableContext = (void**)p - 1;

	if (Node)
	{
		RtlDeleteElementGenericTableAvlEx(this, Node);
	}
	else
	{
		RtlDeleteElementGenericTableAvl(this, p);
	}

	TableContext = 0;

	if (!bNotLock) Unlock();

	if (Node && Node != *((void**)p - 1))
	{
		__debugbreak();
	}

	p->Release();
}

BOOLEAN SetBase::Erase(const void* pvKey, _Out_opt_ MElement** ppv/* = 0*/)
{
	InsertRemove ir(pvKey);
	TABLE_SEARCH_RESULT SearchResult;

	Lock();

	PVOID NodeOrParent;
	MElement* p = (MElement*)RtlLookupElementGenericTableFullAvl(this, &ir, &NodeOrParent, &SearchResult);

	if (p)
	{
		TableContext = (void**)p - 1;
		RtlDeleteElementGenericTableAvlEx(this, NodeOrParent);
		TableContext = 0;
	}

	Unlock();

	if (p)
	{
		if (NodeOrParent != *((void**)p - 1))
		{
			__debugbreak();
		}

		if (ppv)
		{
			*ppv = p;
		}
		else
		{
			p->Release();
		}

		return TRUE;
	}

	return FALSE;
}

void SetBase::MultiRemove(bool (WINAPI* NeedRemove)(void*, MElement*), const void* ctx)
{
	Lock();

	PVOID Node = 0, Key = 0;
	MElement* pDel = 0;

	while (MElement* p = Next(&Key))
	{
		if (Node)
		{
			Remove(pDel, Node, TRUE);
			Node = 0;
		}

		if (NeedRemove(const_cast<void*>(ctx), p))
		{
			Node = Key;
			pDel = p;
		}
	}

	if (Node)
	{
		Remove(pDel, Node, TRUE);
	}

	Unlock();
}

MElement* SetBase::operator[](ULONG i)
{
	LockShared();
	MElement* p = static_cast<MElement*>(RtlGetElementGenericTableAvl(this, i));
	if (p) p->AddRef();
	UnlockShared();
	return p;
}

void SetBase::Delete(PRTL_BALANCED_LINKS node)
{
	if (node->LeftChild)
	{
		Delete(node->LeftChild);
	}

	if (node->RightChild)
	{
		Delete(node->RightChild);
	}

	if (node->Parent != node)
	{
		MElement* ctx = (MElement*)(node + 1);

		*((void**)ctx - 1) = node;
		ctx->Release();
	}
}

void Invert_I(PRTL_BALANCED_LINKS node)
{
	RTL_BALANCED_LINKS *LeftChild = node->LeftChild;
	RTL_BALANCED_LINKS *RightChild = node->RightChild;

	node->LeftChild = RightChild;
	node->RightChild = LeftChild;

	if (LeftChild)
	{
		Invert_I(LeftChild);
	}

	if (RightChild)
	{
		Invert_I(RightChild);
	}
}

void SetBase::Invert()
{
	Lock();
	Invert_I(BalancedRoot.RightChild);
	Unlock();
}

bool SetBase::ForEach(PRTL_BALANCED_LINKS node, bool (WINAPI* proc)(void*, MElement*), const void* ctx)
{
	if (node->LeftChild)
	{
		if (!ForEach(node->LeftChild, proc, ctx)) return false;
	}

	if (!proc(const_cast<void*>(ctx), (MElement*)(node + 1)))
	{
		return false;
	}

	if (node->RightChild)
	{
		if (!ForEach(node->RightChild, proc, ctx)) return false;
	}

	return true;
}

_NT_END
