#include "stdafx.h"

_NT_BEGIN

#include "map.h"

NTSTATUS MapBase::InsertLocked(const InsertRemove& pi, size_t cb, _Out_opt_ MElement** ppv /*= 0*/)
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

NTSTATUS MapBase::Insert(const InsertRemove& pi, size_t cb, _Out_opt_ MElement** ppv /*= 0*/)
{
	Lock();

	NTSTATUS status = InsertLocked(pi, cb, ppv);
	
	Unlock();

	return status;
}

MElement* MapBase::GetLocked(const void* pvKey, _Out_ PVOID *NodeOrParent)
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

MElement* MapBase::Get(const void* pvKey, _Out_ PVOID *NodeOrParent)
{
	Lock();

	MElement* p = GetLocked(pvKey, NodeOrParent);

	Unlock();

	return p;
}

void MapBase::Remove(MElement* p, PVOID Node)
{
	Lock();

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

	Unlock();

	if (Node && Node != *((void**)p - 1))
	{
		__debugbreak();
	}

	p->Release();
}

BOOLEAN MapBase::Erase(const void* pvKey)
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

		p->Release();

		return TRUE;
	}

	return FALSE;
}

MElement* MapBase::operator[](ULONG i)
{
	Lock();
	MElement* p = static_cast<MElement*>(RtlGetElementGenericTableAvl(this, i));
	if (p) p->AddRef();
	Unlock();
	return p;
}

void MapBase::Delete(PRTL_BALANCED_LINKS node)
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

void MapBase::Invert()
{
	Invert_I(BalancedRoot.RightChild);
}

_NT_END