#include "stdafx.h"

_NT_BEGIN

#include "map.h"

class MT : public MElement
{
public:
	ULONG v;

	~MT()
	{
		//DbgPrint("%s<%p>(%x)\n", __FUNCTION__, this, v);
	}

	void operator delete(void* /*pv*/)
	{
		//DbgPrint("delete:%p\n", pv);
	}

private:

	virtual const void* key() const
	{
		return (void*)(ULONG_PTR)v;
	}

public:

	MT(ULONG v) : v(v)
	{
		//DbgPrint("%s<%p>(%x)\n", __FUNCTION__, this, v);
	}
};

struct IntMap : MapBase
{
	PBYTE _ptr;
	ULONG _cb;

	virtual RTL_GENERIC_COMPARE_RESULTS comp(_In_ const void* p, _In_ const void* q)
	{
		if (p < q) return GenericLessThan;
		if (p > q) return GenericGreaterThan;
		return GenericEqual;
	}

	virtual PVOID valloc(_In_ CLONG ByteSize)
	{
		if (_cb < ByteSize)
		{
			return 0;
		}
		PVOID pv = _ptr;

		_ptr += ByteSize, _cb -= ByteSize;

		return pv;
	}

	IntMap(PBYTE ptr, ULONG cb) : _cb(cb), _ptr(ptr)
	{
	}
};

class MTInsertRemove : public InsertRemove
{
	virtual void OnInsert(ElementBase* p) const
	{
		new(p) MT((ULONG)(ULONG_PTR)key());
	}
public:
	MTInsertRemove(ULONG v) : InsertRemove((void*)(ULONG_PTR)v)
	{
	}
};

NTSTATUS InsertMT(IntMap& map, ULONG v)
{
	return map.Insert(MTInsertRemove(v), sizeof(MT));
}

void inttest(ULONG n)
{
	ULONG cb = n * (sizeof(RTL_BALANCED_LINKS) + sizeof(MT));
	if (PBYTE buf = new UCHAR[cb])
	{
		DbgPrint("N = %u\n****************\n", n);
		{
			IntMap map(buf, cb);

			map.SetNoLock();

			if (PULONG pu = new ULONG[n])
			{
				BCryptGenRandom(0, (PUCHAR)pu, n * sizeof(ULONG), BCRYPT_USE_SYSTEM_PREFERRED_RNG);

				ULONG m = n;
				ULONG64 t = GetTickCount64();
				PULONG p = pu;
				do 
				{
					map.Insert(MTInsertRemove(*p++), sizeof(MT));
				} while (--n);

				t = GetTickCount64() - t;
				DbgPrint("insert: %u ms %u ns [%u]\n", t, (t * 1000000)/ m, map.Count());

				n = m;
				p = pu;
				t = GetTickCount64();
				do 
				{
					if (MElement* pe = map.Get((void*)(ULONG_PTR)*p++))
					{
						pe->Release();
					}
					else
					{
						__debugbreak();
					}
				} while (--n);

				t = GetTickCount64() - t;
				DbgPrint("find: %u ms %u ns\n", t, (t * 1000000)/ m);

				n = m;
				p = pu;
				t = GetTickCount64();
				do 
				{
					map.Erase((void*)(ULONG_PTR)*p++);
				} while (--n);

				t = GetTickCount64() - t;
				DbgPrint("erase: %u ms %u ns\n", t, (t * 1000000)/ m);

				if (map.Count())
				{
					__debugbreak();
				}

				delete [] pu;
			}
		}

		delete [] buf;
	}
}

_NT_END