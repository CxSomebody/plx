constexpr size_t log2_r(size_t n, size_t a)
{
        return n == 1 ? a : log2_r(n>>1, a+1);
}
constexpr size_t log2(size_t n)
{
        return log2_r(n, 0);
}

class dynbitset {
	typedef unsigned long word;
	static const size_t wsize = sizeof(word)*8;
	static const size_t lwsize = log2(wsize);
	std::vector<word> data;
	size_t _size;
public:
	dynbitset(size_t size): data((size+wsize-1)>>lwsize), _size(size) {}
	bool get(int index) const
	{
		assert(index >= 0 && size_t(index) < _size);
		return data[index>>lwsize] & 1ul<<(index&(wsize-1));
	}
	void set(int index)
	{
		assert(index >= 0 && size_t(index) < _size);
		data[index>>lwsize] |= 1ul<<(index&(wsize-1));
	}
	void clear(int index)
	{
		assert(index >= 0 && size_t(index) < _size);
		data[index>>lwsize] &= ~(1ul<<(index&(wsize-1)));
	}
	void set_all()
	{
		// does not preserve a property
#if 0
		size_t n = data.size();
		for (size_t i=0; i<n; i++)
			data[i] = (unsigned long) -1;
#endif
		for (size_t i=0; i<_size; i++)
			set(i);
	}
	dynbitset operator|(const dynbitset &that)
	{
		assert(_size == that._size);
		dynbitset result(*this);
		for (size_t i=0; i<data.size(); i++)
			result.data[i] |= that.data[i];
		return result;
	}
	dynbitset &operator|=(const dynbitset &that)
	{
		assert(_size == that._size);
		for (size_t i=0; i<data.size(); i++)
			data[i] |= that.data[i];
		return *this;
	}
	dynbitset operator&(const dynbitset &that)
	{
		assert(_size == that._size);
		dynbitset result(*this);
		for (size_t i=0; i<data.size(); i++)
			result.data[i] &= that.data[i];
		return result;
	}
	dynbitset &operator&=(const dynbitset &that)
	{
		assert(_size == that._size);
		for (size_t i=0; i<data.size(); i++)
			data[i] &= that.data[i];
		return *this;
	}
	bool add_all(const dynbitset &that)
	{
		assert(_size == that._size);
		bool changed = false;
		for (size_t i=0; i<data.size(); i++) {
			word old = data[i];
			data[i] |= that.data[i];
			if (data[i] != old)
				changed = true;
		}
		return changed;
	}
	bool update(const dynbitset &that)
	{
		assert(_size == that._size);
		bool changed = false;
		for (size_t i=0; i<data.size(); i++) {
			word old = data[i];
			data[i] = that.data[i];
			if (data[i] != old)
				changed = true;
		}
		return changed;
	}
	dynbitset operator-(const dynbitset &that)
	{
		assert(_size == that._size);
		dynbitset result(*this);
		for (size_t i=0; i<data.size(); i++)
			result.data[i] &= ~that.data[i];
		return result;
	}
	size_t size() const
	{
		return _size;
	}
	void foreach(std::function<void(int)> f) const
	{
		for (size_t i=0; i<_size; i++)
			if (get(i))
				f(i);
	}
	std::string tostr() const
	{
		std::stringstream ss;
		ss << '[';
		bool sep = false;
		foreach([&](int i) {
			if (sep)
				ss << ',';
			ss << i;
			sep = true;
		});
		ss << ']';
		return ss.str();
	}
};
