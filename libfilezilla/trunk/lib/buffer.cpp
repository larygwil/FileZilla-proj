#include "libfilezilla/buffer.hpp"

#include <algorithm>
#include <cstdlib>

#include <string.h>

namespace fz {

buffer::buffer(size_t capacity)
{
	reserve(capacity);
}

buffer::buffer(buffer const& buf)
{
	if (buf.size_) {
		data_ = new unsigned char[buf.capacity_];
		memcpy(data_, buf.pos_, buf.size_);
size_ = buf.size_;
capacity_ = buf.capacity_;
pos_ = data_;
	}
}

buffer::buffer(buffer && buf)
{
	data_ = buf.data_;
	buf.data_ = nullptr;
	pos_ = buf.pos_;
	buf.pos_ = nullptr;
	size_ = buf.size_;
	capacity_ = buf.capacity_;
}

unsigned char* buffer::get(size_t write_size)
{
	if (capacity_ - (pos_ - data_) - size_ < write_size) {
		if (capacity_ - size_ > write_size) {
			memmove(data_, pos_, size_);
			pos_ = data_;
		}
		else {
			capacity_ = std::max({ size_t(1024), capacity_ * 2, capacity_ + write_size });
			unsigned char* data = new unsigned char[capacity_];
			if (size_) {
				memcpy(data, pos_, size_);
			}
			else {
				delete[] data_;
			}
			data_ = data;
			pos_ = data;
		}
	}
	return data_;
}

buffer& buffer::operator=(buffer const& buf)
{
	delete[] data_;
	if (buf.size_) {
		data_ = new unsigned char[buf.capacity_];
		memcpy(data_, buf.pos_, buf.size_);
	}
	else {
		data_ = nullptr;
	}
	size_ = buf.size_;
	capacity_ = buf.capacity_;
	pos_ = data_;

	return *this;
}

buffer& buffer::operator=(buffer && buf)
{
	delete[] data_;
	data_ = buf.data_;
	buf.data_ = nullptr;
	pos_ = buf.pos_;
	buf.pos_ = nullptr;
	size_ = buf.size_;
	capacity_ = buf.capacity_;
	return *this;
}


void buffer::add(size_t added)
{
	if (capacity_ - (pos_ - data_) - size_ < added) {
		// Hang, draw and quarter the caller.
		std::abort();
	}
	size_ += added;
}

void buffer::consume(size_t consumed)
{
	if (consumed > size_) {
		std::abort();
	}
	if (consumed == size_) {
		pos_ = data_;
		size_ = 0;
	}
	else {
		size_ -= consumed;
		pos_ += consumed;
	}
}

void buffer::clear()
{
	size_ = 0;
	pos_ = data_;
}

void buffer::append(unsigned char const* data, size_t len)
{
	memcpy(get(len), data, len);
	size_ += len;
}

void buffer::append(std::string const& str)
{
	append(reinterpret_cast<unsigned char const*>(str.c_str()), str.size());
}

void buffer::reserve(size_t capacity)
{
	if (capacity_ > capacity) {
		return;
	}

	capacity_ = std::max(size_t(1024), capacity);
	unsigned char* data = new unsigned char[capacity_];
	if (size_) {
		memcpy(data, pos_, size_);
	}
	else {
		delete[] data_;
	}
	data_ = data;
	pos_ = data_;
}

}
