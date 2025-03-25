#pragma once

namespace wio
{
	class ref_counter
	{
	public:
		ref_counter(long initial_count = 1) : m_count(new long{ initial_count }) {}
		ref_counter(const ref_counter& other) : m_count(other.m_count) { (*m_count)++; }
		ref_counter& operator=(const ref_counter& other) { m_count = other.m_count; (*m_count)++; return *this; }

		~ref_counter() { if (!((*m_count)--)) release(); }

		ref_counter& operator++() { (*m_count)++; return *this; }
		ref_counter& operator--() { (*m_count)--; return *this; }

		void increase() { this->operator++(); }
		void decrease() { this->operator--(); }

		long operator*() const { return *m_count; }
		long use_count() const { return *m_count; }
		operator long() const { return *m_count; }
		operator bool() const { return (*m_count) > 0; }

		void set_ref_count(long ref_count) { (*m_count) = ref_count; }
		void release() { delete m_count; m_count = nullptr; }
	private:
		long* m_count = nullptr;
	};

}