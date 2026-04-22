#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <initializer_list>
#include <map>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "module_api.h"

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace wio
{
    using String = std::string;
    using i8 = std::int8_t;
    using i16 = std::int16_t;
    using i32 = std::int32_t;
    using i64 = std::int64_t;
    using u8 = std::uint8_t;
    using u16 = std::uint16_t;
    using u32 = std::uint32_t;
    using u64 = std::uint64_t;
    using f32 = float;
    using f64 = double;
    using isize = std::intptr_t;
    using usize = std::uintptr_t;
    using uchar = unsigned char;
    using byte = std::byte;
    using string = std::string;
    using object_handle = std::uintptr_t;

    template <typename T>
    using DArray = std::vector<T>;

    template <typename T, std::size_t N>
    using SArray = std::array<T, N>;

    template <typename K, typename V>
    using Dict = std::unordered_map<K, V>;

    template <typename K, typename V>
    using Tree = std::map<K, V>;
}

namespace wio::sdk
{
    enum class FieldAccess
    {
        Unknown,
        Public,
        Protected,
        Private
    };

    enum class ErrorCode
    {
        InvalidArgument,
        LibraryOpenFailed,
        SymbolLookupFailed,
        ApiUnavailable,
        InvalidApiDescriptor,
        ExportNotFound,
        TypeNotFound,
        CommandNotFound,
        EventNotFound,
        EventHookNotFound,
        FieldNotFound,
        MethodNotFound,
        FieldNotWritable,
        SignatureMismatch,
        InvokeFailed,
        LifecycleFailed,
        ReloadFailed,
        IoFailure
    };

    class Error : public std::runtime_error
    {
    public:
        Error(ErrorCode code, std::string message)
            : std::runtime_error(std::move(message)),
              code_(code)
        {
        }

        [[nodiscard]] ErrorCode code() const noexcept
        {
            return code_;
        }

    private:
        ErrorCode code_;
    };

    class TypeDescriptorView
    {
    public:
        TypeDescriptorView() = default;

        explicit TypeDescriptorView(const WioModuleTypeDescriptor* descriptor) noexcept
            : descriptor_(descriptor)
        {
        }

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return descriptor_ != nullptr;
        }

        [[nodiscard]] const WioModuleTypeDescriptor* raw() const noexcept
        {
            return descriptor_;
        }

        [[nodiscard]] std::string_view name() const noexcept
        {
            return descriptor_ != nullptr && descriptor_->displayName != nullptr
                ? std::string_view(descriptor_->displayName)
                : std::string_view{};
        }

        [[nodiscard]] std::string_view logical_name() const noexcept
        {
            return descriptor_ != nullptr && descriptor_->logicalTypeName != nullptr
                ? std::string_view(descriptor_->logicalTypeName)
                : std::string_view{};
        }

        [[nodiscard]] WioModuleTypeDescriptorKind kind() const noexcept
        {
            return descriptor_ != nullptr ? descriptor_->kind : WIO_MODULE_TYPE_DESC_UNKNOWN;
        }

        [[nodiscard]] bool is_known() const noexcept
        {
            return descriptor_ != nullptr && descriptor_->kind != WIO_MODULE_TYPE_DESC_UNKNOWN;
        }

        [[nodiscard]] bool is_primitive() const noexcept
        {
            return kind() == WIO_MODULE_TYPE_DESC_PRIMITIVE;
        }

        [[nodiscard]] bool is_string() const noexcept
        {
            return kind() == WIO_MODULE_TYPE_DESC_STRING;
        }

        [[nodiscard]] bool is_object() const noexcept
        {
            return kind() == WIO_MODULE_TYPE_DESC_OBJECT;
        }

        [[nodiscard]] bool is_component() const noexcept
        {
            return kind() == WIO_MODULE_TYPE_DESC_COMPONENT;
        }

        [[nodiscard]] bool is_dynamic_array() const noexcept
        {
            return kind() == WIO_MODULE_TYPE_DESC_DYNAMIC_ARRAY;
        }

        [[nodiscard]] bool is_static_array() const noexcept
        {
            return kind() == WIO_MODULE_TYPE_DESC_STATIC_ARRAY;
        }

        [[nodiscard]] bool is_array() const noexcept
        {
            return is_dynamic_array() || is_static_array();
        }

        [[nodiscard]] bool is_dict() const noexcept
        {
            return kind() == WIO_MODULE_TYPE_DESC_DICT;
        }

        [[nodiscard]] bool is_tree() const noexcept
        {
            return kind() == WIO_MODULE_TYPE_DESC_TREE;
        }

        [[nodiscard]] bool is_associative_container() const noexcept
        {
            return is_dict() || is_tree();
        }

        [[nodiscard]] bool is_container() const noexcept
        {
            return is_array() || is_associative_container();
        }

        [[nodiscard]] bool is_function() const noexcept
        {
            return kind() == WIO_MODULE_TYPE_DESC_FUNCTION;
        }

        [[nodiscard]] bool is_opaque() const noexcept
        {
            return kind() == WIO_MODULE_TYPE_DESC_OPAQUE;
        }

        [[nodiscard]] WioAbiType abi_type() const noexcept
        {
            return descriptor_ != nullptr ? descriptor_->abiType : WIO_ABI_UNKNOWN;
        }

        [[nodiscard]] bool has_abi_type() const noexcept
        {
            return abi_type() != WIO_ABI_UNKNOWN;
        }

        [[nodiscard]] bool is_void() const noexcept
        {
            return abi_type() == WIO_ABI_VOID;
        }

        [[nodiscard]] bool is_bool() const noexcept
        {
            return abi_type() == WIO_ABI_BOOL;
        }

        [[nodiscard]] bool is_char() const noexcept
        {
            return abi_type() == WIO_ABI_CHAR;
        }

        [[nodiscard]] bool is_uchar() const noexcept
        {
            return abi_type() == WIO_ABI_UCHAR;
        }

        [[nodiscard]] bool is_byte() const noexcept
        {
            return abi_type() == WIO_ABI_BYTE;
        }

        [[nodiscard]] bool is_i8() const noexcept
        {
            return abi_type() == WIO_ABI_I8;
        }

        [[nodiscard]] bool is_i16() const noexcept
        {
            return abi_type() == WIO_ABI_I16;
        }

        [[nodiscard]] bool is_i32() const noexcept
        {
            return abi_type() == WIO_ABI_I32;
        }

        [[nodiscard]] bool is_i64() const noexcept
        {
            return abi_type() == WIO_ABI_I64;
        }

        [[nodiscard]] bool is_u8() const noexcept
        {
            return abi_type() == WIO_ABI_U8;
        }

        [[nodiscard]] bool is_u16() const noexcept
        {
            return abi_type() == WIO_ABI_U16;
        }

        [[nodiscard]] bool is_u32() const noexcept
        {
            return abi_type() == WIO_ABI_U32;
        }

        [[nodiscard]] bool is_u64() const noexcept
        {
            return abi_type() == WIO_ABI_U64;
        }

        [[nodiscard]] bool is_isize() const noexcept
        {
            return abi_type() == WIO_ABI_ISIZE;
        }

        [[nodiscard]] bool is_usize() const noexcept
        {
            return abi_type() == WIO_ABI_USIZE;
        }

        [[nodiscard]] bool is_pointer_sized_integer() const noexcept
        {
            return is_isize() || is_usize();
        }

        [[nodiscard]] bool is_f32() const noexcept
        {
            return abi_type() == WIO_ABI_F32;
        }

        [[nodiscard]] bool is_f64() const noexcept
        {
            return abi_type() == WIO_ABI_F64;
        }

        [[nodiscard]] bool is_signed_integer() const noexcept
        {
            return is_i8() || is_i16() || is_i32() || is_i64() || is_isize();
        }

        [[nodiscard]] bool is_unsigned_integer() const noexcept
        {
            return is_u8() || is_u16() || is_u32() || is_u64() || is_usize();
        }

        [[nodiscard]] bool is_integer() const noexcept
        {
            return is_signed_integer() || is_unsigned_integer();
        }

        [[nodiscard]] bool is_floating_point() const noexcept
        {
            return is_f32() || is_f64();
        }

        [[nodiscard]] bool is_numeric() const noexcept
        {
            return is_integer() || is_floating_point();
        }

        [[nodiscard]] std::uint64_t static_extent() const noexcept
        {
            return descriptor_ != nullptr ? descriptor_->staticArraySize : 0u;
        }

        [[nodiscard]] bool has_element_type() const noexcept
        {
            return descriptor_ != nullptr && descriptor_->elementType != nullptr;
        }

        [[nodiscard]] TypeDescriptorView element_type() const noexcept
        {
            return TypeDescriptorView(descriptor_ != nullptr ? descriptor_->elementType : nullptr);
        }

        [[nodiscard]] bool has_key_type() const noexcept
        {
            return descriptor_ != nullptr && descriptor_->keyType != nullptr;
        }

        [[nodiscard]] TypeDescriptorView key_type() const noexcept
        {
            return TypeDescriptorView(descriptor_ != nullptr ? descriptor_->keyType : nullptr);
        }

        [[nodiscard]] bool has_value_type() const noexcept
        {
            return descriptor_ != nullptr && descriptor_->valueType != nullptr;
        }

        [[nodiscard]] TypeDescriptorView value_type() const noexcept
        {
            return TypeDescriptorView(descriptor_ != nullptr ? descriptor_->valueType : nullptr);
        }

        [[nodiscard]] bool has_return_type() const noexcept
        {
            return descriptor_ != nullptr && descriptor_->returnType != nullptr;
        }

        [[nodiscard]] TypeDescriptorView return_type() const noexcept
        {
            return TypeDescriptorView(descriptor_ != nullptr ? descriptor_->returnType : nullptr);
        }

        [[nodiscard]] std::uint32_t parameter_count() const noexcept
        {
            return descriptor_ != nullptr ? descriptor_->parameterCount : 0u;
        }

        [[nodiscard]] bool has_parameter_types() const noexcept
        {
            return descriptor_ != nullptr && descriptor_->parameterCount > 0u && descriptor_->parameterTypes != nullptr;
        }

        [[nodiscard]] TypeDescriptorView parameter_type(std::uint32_t index) const noexcept
        {
            if (descriptor_ == nullptr || descriptor_->parameterTypes == nullptr || index >= descriptor_->parameterCount)
                return TypeDescriptorView();
            return TypeDescriptorView(descriptor_->parameterTypes[index]);
        }

    private:
        const WioModuleTypeDescriptor* descriptor_ = nullptr;
    };

    struct FieldInfo
    {
        std::string_view name{};
        FieldAccess access = FieldAccess::Unknown;
        bool readable = false;
        bool writable = false;
        bool readOnly = false;
        TypeDescriptorView type{};
        const WioModuleField* descriptor = nullptr;

        [[nodiscard]] std::string_view type_name() const noexcept
        {
            return type.name();
        }

        [[nodiscard]] std::string_view logical_type_name() const noexcept
        {
            return type.logical_name();
        }

        [[nodiscard]] WioModuleTypeDescriptorKind kind() const noexcept
        {
            return type.kind();
        }

        [[nodiscard]] WioAbiType abi_type() const noexcept
        {
            return type.abi_type();
        }

        [[nodiscard]] bool can_read() const noexcept
        {
            return readable;
        }

        [[nodiscard]] bool can_write() const noexcept
        {
            return writable;
        }

        [[nodiscard]] bool is_read_only() const noexcept
        {
            return readOnly;
        }

        [[nodiscard]] bool is_primitive() const noexcept
        {
            return type.is_primitive();
        }

        [[nodiscard]] bool is_string() const noexcept
        {
            return type.is_string();
        }

        [[nodiscard]] bool is_object() const noexcept
        {
            return type.is_object();
        }

        [[nodiscard]] bool is_component() const noexcept
        {
            return type.is_component();
        }

        [[nodiscard]] bool is_dynamic_array() const noexcept
        {
            return type.is_dynamic_array();
        }

        [[nodiscard]] bool is_static_array() const noexcept
        {
            return type.is_static_array();
        }

        [[nodiscard]] bool is_array() const noexcept
        {
            return type.is_array();
        }

        [[nodiscard]] bool is_dict() const noexcept
        {
            return type.is_dict();
        }

        [[nodiscard]] bool is_tree() const noexcept
        {
            return type.is_tree();
        }

        [[nodiscard]] bool is_container() const noexcept
        {
            return type.is_container();
        }

        [[nodiscard]] bool is_function() const noexcept
        {
            return type.is_function();
        }

        [[nodiscard]] bool is_numeric() const noexcept
        {
            return type.is_numeric();
        }
    };

    template <typename T>
    class WioArray
    {
    public:
        using value_type = T;
        using container_type = wio::DArray<T>;
        using size_type = typename container_type::size_type;
        using iterator = typename container_type::iterator;
        using const_iterator = typename container_type::const_iterator;

        WioArray() = default;

        WioArray(container_type values)
            : values_(std::move(values))
        {
        }

        WioArray(std::initializer_list<T> values)
            : values_(values)
        {
        }

        [[nodiscard]] bool empty() const noexcept
        {
            return values_.empty();
        }

        [[nodiscard]] size_type count() const noexcept
        {
            return values_.size();
        }

        [[nodiscard]] size_type size() const noexcept
        {
            return values_.size();
        }

        void clear() noexcept(noexcept(values_.clear()))
        {
            values_.clear();
        }

        void reserve(size_type capacity)
        {
            values_.reserve(capacity);
        }

        void push(const T& value)
        {
            values_.push_back(value);
        }

        void push(T&& value)
        {
            values_.push_back(std::move(value));
        }

        template <typename... TArgs>
        T& emplace(TArgs&&... args)
        {
            return values_.emplace_back(std::forward<TArgs>(args)...);
        }

        [[nodiscard]] T* data() noexcept
        {
            return values_.data();
        }

        [[nodiscard]] const T* data() const noexcept
        {
            return values_.data();
        }

        [[nodiscard]] T& front()
        {
            return values_.front();
        }

        [[nodiscard]] const T& front() const
        {
            return values_.front();
        }

        [[nodiscard]] T& back()
        {
            return values_.back();
        }

        [[nodiscard]] const T& back() const
        {
            return values_.back();
        }

        [[nodiscard]] T& operator[](const size_type index)
        {
            return values_[index];
        }

        [[nodiscard]] const T& operator[](const size_type index) const
        {
            return values_[index];
        }

        [[nodiscard]] T& at(const size_type index)
        {
            return values_.at(index);
        }

        [[nodiscard]] const T& at(const size_type index) const
        {
            return values_.at(index);
        }

        [[nodiscard]] bool contains(const T& value) const
        {
            return std::find(values_.begin(), values_.end(), value) != values_.end();
        }

        [[nodiscard]] iterator begin() noexcept
        {
            return values_.begin();
        }

        [[nodiscard]] iterator end() noexcept
        {
            return values_.end();
        }

        [[nodiscard]] const_iterator begin() const noexcept
        {
            return values_.begin();
        }

        [[nodiscard]] const_iterator end() const noexcept
        {
            return values_.end();
        }

        [[nodiscard]] const_iterator cbegin() const noexcept
        {
            return values_.cbegin();
        }

        [[nodiscard]] const_iterator cend() const noexcept
        {
            return values_.cend();
        }

        [[nodiscard]] container_type& raw() & noexcept
        {
            return values_;
        }

        [[nodiscard]] const container_type& raw() const & noexcept
        {
            return values_;
        }

        [[nodiscard]] container_type take() &&
        {
            return std::move(values_);
        }

    private:
        container_type values_{};
    };

    template <typename T, std::size_t N>
    class WioStaticArray
    {
    public:
        using value_type = T;
        using container_type = wio::SArray<T, N>;
        using size_type = typename container_type::size_type;
        using iterator = typename container_type::iterator;
        using const_iterator = typename container_type::const_iterator;

        WioStaticArray() = default;

        WioStaticArray(container_type values)
            : values_(std::move(values))
        {
        }

        WioStaticArray(std::initializer_list<T> values)
        {
            if (values.size() != N)
            {
                std::ostringstream message;
                message << "Wio SDK static array expected exactly " << N
                        << " element(s) but received " << values.size() << '.';
                throw Error(ErrorCode::InvalidArgument, message.str());
            }

            std::copy(values.begin(), values.end(), values_.begin());
        }

        [[nodiscard]] static constexpr size_type count() noexcept
        {
            return N;
        }

        [[nodiscard]] static constexpr size_type size() noexcept
        {
            return N;
        }

        [[nodiscard]] bool empty() const noexcept
        {
            return N == 0u;
        }

        [[nodiscard]] T* data() noexcept
        {
            return values_.data();
        }

        [[nodiscard]] const T* data() const noexcept
        {
            return values_.data();
        }

        [[nodiscard]] T& front()
        {
            return values_.front();
        }

        [[nodiscard]] const T& front() const
        {
            return values_.front();
        }

        [[nodiscard]] T& back()
        {
            return values_.back();
        }

        [[nodiscard]] const T& back() const
        {
            return values_.back();
        }

        [[nodiscard]] T& operator[](const size_type index)
        {
            return values_[index];
        }

        [[nodiscard]] const T& operator[](const size_type index) const
        {
            return values_[index];
        }

        [[nodiscard]] T& at(const size_type index)
        {
            return values_.at(index);
        }

        [[nodiscard]] const T& at(const size_type index) const
        {
            return values_.at(index);
        }

        [[nodiscard]] iterator begin() noexcept
        {
            return values_.begin();
        }

        [[nodiscard]] iterator end() noexcept
        {
            return values_.end();
        }

        [[nodiscard]] const_iterator begin() const noexcept
        {
            return values_.begin();
        }

        [[nodiscard]] const_iterator end() const noexcept
        {
            return values_.end();
        }

        [[nodiscard]] const_iterator cbegin() const noexcept
        {
            return values_.cbegin();
        }

        [[nodiscard]] const_iterator cend() const noexcept
        {
            return values_.cend();
        }

        [[nodiscard]] container_type& raw() & noexcept
        {
            return values_;
        }

        [[nodiscard]] const container_type& raw() const & noexcept
        {
            return values_;
        }

        [[nodiscard]] container_type take() &&
        {
            return std::move(values_);
        }

    private:
        container_type values_{};
    };

    template <typename K, typename V>
    class WioDict
    {
    public:
        using key_type = K;
        using mapped_type = V;
        using value_type = typename wio::Dict<K, V>::value_type;
        using container_type = wio::Dict<K, V>;
        using size_type = typename container_type::size_type;
        using iterator = typename container_type::iterator;
        using const_iterator = typename container_type::const_iterator;

        WioDict() = default;

        WioDict(container_type values)
            : values_(std::move(values))
        {
        }

        WioDict(std::initializer_list<value_type> values)
            : values_(values)
        {
        }

        [[nodiscard]] bool empty() const noexcept
        {
            return values_.empty();
        }

        [[nodiscard]] size_type count() const noexcept
        {
            return values_.size();
        }

        [[nodiscard]] size_type size() const noexcept
        {
            return values_.size();
        }

        void clear() noexcept(noexcept(values_.clear()))
        {
            values_.clear();
        }

        void reserve(const size_type capacity)
        {
            values_.reserve(capacity);
        }

        [[nodiscard]] bool contains_key(const K& key) const
        {
            return values_.find(key) != values_.end();
        }

        [[nodiscard]] std::optional<V> try_get(const K& key) const
        {
            const auto iterator = values_.find(key);
            if (iterator == values_.end())
                return std::nullopt;
            return iterator->second;
        }

        [[nodiscard]] V get_or(const K& key, V defaultValue) const
        {
            const auto iterator = values_.find(key);
            if (iterator == values_.end())
                return defaultValue;
            return iterator->second;
        }

        void set(K key, V value)
        {
            values_.insert_or_assign(std::move(key), std::move(value));
        }

        [[nodiscard]] bool remove(const K& key)
        {
            return values_.erase(key) > 0u;
        }

        [[nodiscard]] V& operator[](const K& key)
        {
            return values_[key];
        }

        [[nodiscard]] const V& at(const K& key) const
        {
            return values_.at(key);
        }

        [[nodiscard]] V& at(const K& key)
        {
            return values_.at(key);
        }

        [[nodiscard]] wio::DArray<K> keys() const
        {
            wio::DArray<K> result;
            result.reserve(values_.size());
            for (const auto& entry : values_)
                result.push_back(entry.first);
            return result;
        }

        [[nodiscard]] wio::DArray<V> values() const
        {
            wio::DArray<V> result;
            result.reserve(values_.size());
            for (const auto& entry : values_)
                result.push_back(entry.second);
            return result;
        }

        [[nodiscard]] iterator begin() noexcept
        {
            return values_.begin();
        }

        [[nodiscard]] iterator end() noexcept
        {
            return values_.end();
        }

        [[nodiscard]] const_iterator begin() const noexcept
        {
            return values_.begin();
        }

        [[nodiscard]] const_iterator end() const noexcept
        {
            return values_.end();
        }

        [[nodiscard]] const_iterator cbegin() const noexcept
        {
            return values_.cbegin();
        }

        [[nodiscard]] const_iterator cend() const noexcept
        {
            return values_.cend();
        }

        [[nodiscard]] container_type& raw() & noexcept
        {
            return values_;
        }

        [[nodiscard]] const container_type& raw() const & noexcept
        {
            return values_;
        }

        [[nodiscard]] container_type take() &&
        {
            return std::move(values_);
        }

    private:
        container_type values_{};
    };

    template <typename K, typename V>
    class WioTree
    {
    public:
        using key_type = K;
        using mapped_type = V;
        using value_type = typename wio::Tree<K, V>::value_type;
        using container_type = wio::Tree<K, V>;
        using size_type = typename container_type::size_type;
        using iterator = typename container_type::iterator;
        using const_iterator = typename container_type::const_iterator;

        WioTree() = default;

        WioTree(container_type values)
            : values_(std::move(values))
        {
        }

        WioTree(std::initializer_list<value_type> values)
            : values_(values)
        {
        }

        [[nodiscard]] bool empty() const noexcept
        {
            return values_.empty();
        }

        [[nodiscard]] size_type count() const noexcept
        {
            return values_.size();
        }

        [[nodiscard]] size_type size() const noexcept
        {
            return values_.size();
        }

        void clear() noexcept(noexcept(values_.clear()))
        {
            values_.clear();
        }

        [[nodiscard]] bool contains_key(const K& key) const
        {
            return values_.find(key) != values_.end();
        }

        [[nodiscard]] std::optional<V> try_get(const K& key) const
        {
            const auto iterator = values_.find(key);
            if (iterator == values_.end())
                return std::nullopt;
            return iterator->second;
        }

        [[nodiscard]] V get_or(const K& key, V defaultValue) const
        {
            const auto iterator = values_.find(key);
            if (iterator == values_.end())
                return defaultValue;
            return iterator->second;
        }

        void set(K key, V value)
        {
            values_.insert_or_assign(std::move(key), std::move(value));
        }

        [[nodiscard]] bool remove(const K& key)
        {
            return values_.erase(key) > 0u;
        }

        [[nodiscard]] V& operator[](const K& key)
        {
            return values_[key];
        }

        [[nodiscard]] const V& at(const K& key) const
        {
            return values_.at(key);
        }

        [[nodiscard]] V& at(const K& key)
        {
            return values_.at(key);
        }

        [[nodiscard]] wio::DArray<K> keys() const
        {
            wio::DArray<K> result;
            result.reserve(values_.size());
            for (const auto& entry : values_)
                result.push_back(entry.first);
            return result;
        }

        [[nodiscard]] wio::DArray<V> values() const
        {
            wio::DArray<V> result;
            result.reserve(values_.size());
            for (const auto& entry : values_)
                result.push_back(entry.second);
            return result;
        }

        [[nodiscard]] iterator begin() noexcept
        {
            return values_.begin();
        }

        [[nodiscard]] iterator end() noexcept
        {
            return values_.end();
        }

        [[nodiscard]] const_iterator begin() const noexcept
        {
            return values_.begin();
        }

        [[nodiscard]] const_iterator end() const noexcept
        {
            return values_.end();
        }

        [[nodiscard]] const_iterator cbegin() const noexcept
        {
            return values_.cbegin();
        }

        [[nodiscard]] const_iterator cend() const noexcept
        {
            return values_.cend();
        }

        [[nodiscard]] container_type& raw() & noexcept
        {
            return values_;
        }

        [[nodiscard]] const container_type& raw() const & noexcept
        {
            return values_;
        }

        [[nodiscard]] container_type take() &&
        {
            return std::move(values_);
        }

    private:
        container_type values_{};
    };

    template <typename Signature>
    class WioFunction;

    template <typename TReturn, typename... TArgs>
    class WioFunction<TReturn(TArgs...)>
    {
    public:
        using function_type = std::function<TReturn(TArgs...)>;

        WioFunction() = default;

        WioFunction(function_type value)
            : value_(std::move(value))
        {
        }

        template <typename TCallable,
                  typename = std::enable_if_t<
                      !std::is_same_v<std::decay_t<TCallable>, WioFunction> &&
                      std::is_constructible_v<function_type, TCallable>>>
        WioFunction(TCallable&& callable)
            : value_(std::forward<TCallable>(callable))
        {
        }

        [[nodiscard]] bool valid() const noexcept
        {
            return static_cast<bool>(value_);
        }

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return valid();
        }

        [[nodiscard]] function_type& raw() & noexcept
        {
            return value_;
        }

        [[nodiscard]] const function_type& raw() const & noexcept
        {
            return value_;
        }

        [[nodiscard]] function_type take() &&
        {
            return std::move(value_);
        }

        TReturn operator()(TArgs... args) const
        {
            return value_(std::forward<TArgs>(args)...);
        }

    private:
        function_type value_{};
    };

    template <typename TReturn, typename... TArgs>
    class WioFunction<TReturn(*)(TArgs...)> : public WioFunction<TReturn(TArgs...)>
    {
    public:
        using WioFunction<TReturn(TArgs...)>::WioFunction;
    };

    template <typename TReturn, typename... TArgs>
    class WioFunction<std::function<TReturn(TArgs...)>> : public WioFunction<TReturn(TArgs...)>
    {
    public:
        using WioFunction<TReturn(TArgs...)>::WioFunction;
    };

    namespace detail
    {
#if defined(_WIN32)
        using LibraryHandle = HMODULE;
#else
        using LibraryHandle = void*;
#endif

        template <typename>
        struct FunctionTraits;

        template <typename TReturn, typename... TArgs>
        struct FunctionTraits<TReturn(TArgs...)>
        {
            using ReturnType = TReturn;
            using ArgumentTuple = std::tuple<TArgs...>;
            using StdFunction = std::function<TReturn(TArgs...)>;
            static constexpr size_t Arity = sizeof...(TArgs);

            template <size_t Index>
            using Argument = std::tuple_element_t<Index, ArgumentTuple>;
        };

        template <typename TReturn, typename... TArgs>
        struct FunctionTraits<TReturn(*)(TArgs...)> : FunctionTraits<TReturn(TArgs...)>
        {
        };

        template <typename TReturn, typename... TArgs>
        struct FunctionTraits<std::function<TReturn(TArgs...)>> : FunctionTraits<TReturn(TArgs...)>
        {
        };

        template <typename T>
        using Decay = std::remove_cv_t<std::remove_reference_t<T>>;

        inline FieldAccess toFieldAccess(const WioModuleAccessModifier accessModifier) noexcept
        {
            switch (accessModifier)
            {
            case WIO_MODULE_ACCESS_PUBLIC: return FieldAccess::Public;
            case WIO_MODULE_ACCESS_PROTECTED: return FieldAccess::Protected;
            case WIO_MODULE_ACCESS_PRIVATE: return FieldAccess::Private;
            default: return FieldAccess::Unknown;
            }
        }

        template <typename T>
        constexpr bool IsAbiScalarType =
            std::is_same_v<Decay<T>, bool> ||
            std::is_same_v<Decay<T>, char> ||
            std::is_same_v<Decay<T>, unsigned char> ||
            std::is_same_v<Decay<T>, std::byte> ||
            std::is_same_v<Decay<T>, std::intptr_t> ||
            std::is_same_v<Decay<T>, std::uintptr_t> ||
            std::is_floating_point_v<Decay<T>> ||
            std::is_integral_v<Decay<T>>;

        inline std::string dynamicLibraryErrorMessage()
        {
#if defined(_WIN32)
            const DWORD errorCode = GetLastError();
            if (errorCode == 0)
                return "Unknown Windows loader error.";

            LPSTR buffer = nullptr;
            const DWORD length = FormatMessageA(
                FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                nullptr,
                errorCode,
                MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                reinterpret_cast<LPSTR>(&buffer),
                0,
                nullptr
            );

            std::string result = length > 0 && buffer != nullptr ? std::string(buffer, length) : "Unknown Windows loader error.";
            if (buffer != nullptr)
                LocalFree(buffer);

            while (!result.empty() && (result.back() == '\r' || result.back() == '\n'))
                result.pop_back();

            return result;
#else
            const char* error = dlerror();
            return error != nullptr ? std::string(error) : "Unknown dynamic loader error.";
#endif
        }

        inline LibraryHandle openLibrary(const std::filesystem::path& path)
        {
#if defined(_WIN32)
            return LoadLibraryA(path.string().c_str());
#else
            return dlopen(path.string().c_str(), RTLD_NOW);
#endif
        }

        inline void closeLibrary(LibraryHandle handle) noexcept
        {
            if (handle == nullptr)
                return;

#if defined(_WIN32)
            FreeLibrary(handle);
#else
            dlclose(handle);
#endif
        }

        template <typename TSymbol>
        TSymbol loadSymbol(LibraryHandle handle, const char* symbolName)
        {
#if defined(_WIN32)
            auto* rawSymbol = GetProcAddress(handle, symbolName);
#else
            dlerror();
            void* rawSymbol = dlsym(handle, symbolName);
#endif
            return reinterpret_cast<TSymbol>(rawSymbol);
        }

        template <typename T>
        constexpr bool AlwaysFalse = false;

        template <typename T>
        struct IsStdVector : std::false_type
        {
        };

        template <typename TElement, typename TAllocator>
        struct IsStdVector<std::vector<TElement, TAllocator>> : std::true_type
        {
            using Element = TElement;
        };

        template <typename T>
        struct IsStdArray : std::false_type
        {
        };

        template <typename TElement, std::size_t TExtent>
        struct IsStdArray<std::array<TElement, TExtent>> : std::true_type
        {
            using Element = TElement;
            static constexpr std::size_t Extent = TExtent;
        };

        template <typename T>
        struct IsStdUnorderedMap : std::false_type
        {
        };

        template <typename TKey, typename TValue, typename THash, typename TEqual, typename TAllocator>
        struct IsStdUnorderedMap<std::unordered_map<TKey, TValue, THash, TEqual, TAllocator>> : std::true_type
        {
            using Key = TKey;
            using Value = TValue;
        };

        template <typename T>
        struct IsStdMap : std::false_type
        {
        };

        template <typename TKey, typename TValue, typename TCompare, typename TAllocator>
        struct IsStdMap<std::map<TKey, TValue, TCompare, TAllocator>> : std::true_type
        {
            using Key = TKey;
            using Value = TValue;
        };

        template <typename T>
        struct IsStdFunction : std::false_type
        {
        };

        template <typename TReturn, typename... TArgs>
        struct IsStdFunction<std::function<TReturn(TArgs...)>> : std::true_type
        {
            using Signature = TReturn(TArgs...);
        };

        template <typename T>
        constexpr WioAbiType getAbiType()
        {
            using U = Decay<T>;

            if constexpr (std::is_same_v<U, void>)
            {
                return WIO_ABI_VOID;
            }
            else if constexpr (std::is_same_v<U, bool>)
            {
                return WIO_ABI_BOOL;
            }
            else if constexpr (std::is_same_v<U, char>)
            {
                return WIO_ABI_CHAR;
            }
            else if constexpr (std::is_same_v<U, unsigned char>)
            {
                return WIO_ABI_UCHAR;
            }
            else if constexpr (std::is_same_v<U, std::byte>)
            {
                return WIO_ABI_BYTE;
            }
            else if constexpr (std::is_same_v<U, std::intptr_t>)
            {
                return WIO_ABI_ISIZE;
            }
            else if constexpr (std::is_same_v<U, std::uintptr_t>)
            {
                return WIO_ABI_USIZE;
            }
            else if constexpr (std::is_floating_point_v<U>)
            {
                if constexpr (sizeof(U) == sizeof(float))
                    return WIO_ABI_F32;
                else if constexpr (sizeof(U) == sizeof(double))
                    return WIO_ABI_F64;
            }
            else if constexpr (std::is_integral_v<U>)
            {
                if constexpr (std::is_same_v<U, signed char>)
                {
                    return WIO_ABI_I8;
                }
                else if constexpr (std::is_unsigned_v<U>)
                {
                    if constexpr (sizeof(U) == 1)
                        return WIO_ABI_U8;
                    else if constexpr (sizeof(U) == 2)
                        return WIO_ABI_U16;
                    else if constexpr (sizeof(U) == 4)
                        return WIO_ABI_U32;
                    else if constexpr (sizeof(U) == 8)
                        return WIO_ABI_U64;
                }
                else
                {
                    if constexpr (sizeof(U) == 1)
                        return WIO_ABI_I8;
                    else if constexpr (sizeof(U) == 2)
                        return WIO_ABI_I16;
                    else if constexpr (sizeof(U) == 4)
                        return WIO_ABI_I32;
                    else if constexpr (sizeof(U) == 8)
                        return WIO_ABI_I64;
                }
            }
            else
            {
                static_assert(AlwaysFalse<T>, "Unsupported Wio SDK ABI type.");
            }
        }

        inline const char* abiTypeName(WioAbiType type) noexcept
        {
            switch (type)
            {
            case WIO_ABI_UNKNOWN: return "unknown";
            case WIO_ABI_VOID: return "void";
            case WIO_ABI_BOOL: return "bool";
            case WIO_ABI_CHAR: return "char";
            case WIO_ABI_UCHAR: return "uchar";
            case WIO_ABI_BYTE: return "byte";
            case WIO_ABI_I8: return "i8";
            case WIO_ABI_I16: return "i16";
            case WIO_ABI_I32: return "i32";
            case WIO_ABI_I64: return "i64";
            case WIO_ABI_U8: return "u8";
            case WIO_ABI_U16: return "u16";
            case WIO_ABI_U32: return "u32";
            case WIO_ABI_U64: return "u64";
            case WIO_ABI_ISIZE: return "isize";
            case WIO_ABI_USIZE: return "usize";
            case WIO_ABI_F32: return "f32";
            case WIO_ABI_F64: return "f64";
            default: return "unknown";
            }
        }

        template <typename T>
        std::string hostFieldTypeName();

        template <typename Signature, size_t... Indices>
        std::string hostFunctionTypeName(std::index_sequence<Indices...>)
        {
            using Traits = FunctionTraits<Signature>;

            std::ostringstream message;
            message << "fn(";
            bool first = true;
            ((message << (std::exchange(first, false) ? "" : ", ")
                      << hostFieldTypeName<typename Traits::template Argument<Indices>>()), ...);
            message << ") -> " << hostFieldTypeName<typename Traits::ReturnType>();
            return message.str();
        }

        template <typename T>
        std::string hostFieldTypeName()
        {
            using U = Decay<T>;

            if constexpr (std::is_same_v<U, void>)
            {
                return "void";
            }
            else if constexpr (std::is_same_v<U, wio::string>)
            {
                return "string";
            }
            else if constexpr (IsAbiScalarType<U>)
            {
                return std::string(abiTypeName(getAbiType<U>()));
            }
            else if constexpr (IsStdVector<U>::value)
            {
                return hostFieldTypeName<typename IsStdVector<U>::Element>() + "[]";
            }
            else if constexpr (IsStdArray<U>::value)
            {
                std::ostringstream message;
                message << "[" << hostFieldTypeName<typename IsStdArray<U>::Element>()
                        << "; " << IsStdArray<U>::Extent << "]";
                return message.str();
            }
            else if constexpr (IsStdUnorderedMap<U>::value)
            {
                return "Dict<" + hostFieldTypeName<typename IsStdUnorderedMap<U>::Key>()
                    + ", " + hostFieldTypeName<typename IsStdUnorderedMap<U>::Value>() + ">";
            }
            else if constexpr (IsStdMap<U>::value)
            {
                return "Tree<" + hostFieldTypeName<typename IsStdMap<U>::Key>()
                    + ", " + hostFieldTypeName<typename IsStdMap<U>::Value>() + ">";
            }
            else if constexpr (IsStdFunction<U>::value)
            {
                return hostFunctionTypeName<typename IsStdFunction<U>::Signature>(
                    std::make_index_sequence<FunctionTraits<typename IsStdFunction<U>::Signature>::Arity>{}
                );
            }
            else
            {
                return "<unsupported host type>";
            }
        }

        inline std::string descriptorDisplayName(const TypeDescriptorView& type)
        {
            const auto displayName = type.name();
            if (!displayName.empty())
                return std::string(displayName);

            if (type.has_abi_type())
                return std::string(abiTypeName(type.abi_type()));

            return "<unknown>";
        }

        template <typename T>
        bool matchesTypeDescriptor(const TypeDescriptorView& type);

        template <typename Signature, size_t... Indices>
        bool matchesFunctionDescriptor(const TypeDescriptorView& type, std::index_sequence<Indices...>)
        {
            using Traits = FunctionTraits<Signature>;

            if (!type.is_function() ||
                !type.has_return_type() ||
                type.parameter_count() != static_cast<std::uint32_t>(Traits::Arity))
            {
                return false;
            }

            if (!matchesTypeDescriptor<typename Traits::ReturnType>(type.return_type()))
                return false;

            return (matchesTypeDescriptor<typename Traits::template Argument<Indices>>(type.parameter_type(static_cast<std::uint32_t>(Indices))) && ...);
        }

        template <typename T>
        bool matchesTypeDescriptor(const TypeDescriptorView& type)
        {
            using U = Decay<T>;

            if constexpr (std::is_same_v<U, void>)
            {
                return type.has_abi_type() && type.abi_type() == WIO_ABI_VOID;
            }
            else if constexpr (std::is_same_v<U, wio::string>)
            {
                return type.is_string();
            }
            else if constexpr (IsAbiScalarType<U>)
            {
                return type.is_primitive() && type.abi_type() == getAbiType<U>();
            }
            else if constexpr (IsStdVector<U>::value)
            {
                return type.is_dynamic_array() &&
                    type.has_element_type() &&
                    matchesTypeDescriptor<typename IsStdVector<U>::Element>(type.element_type());
            }
            else if constexpr (IsStdArray<U>::value)
            {
                return type.is_static_array() &&
                    type.static_extent() == IsStdArray<U>::Extent &&
                    type.has_element_type() &&
                    matchesTypeDescriptor<typename IsStdArray<U>::Element>(type.element_type());
            }
            else if constexpr (IsStdUnorderedMap<U>::value)
            {
                return type.is_dict() &&
                    type.has_key_type() &&
                    type.has_value_type() &&
                    matchesTypeDescriptor<typename IsStdUnorderedMap<U>::Key>(type.key_type()) &&
                    matchesTypeDescriptor<typename IsStdUnorderedMap<U>::Value>(type.value_type());
            }
            else if constexpr (IsStdMap<U>::value)
            {
                return type.is_tree() &&
                    type.has_key_type() &&
                    type.has_value_type() &&
                    matchesTypeDescriptor<typename IsStdMap<U>::Key>(type.key_type()) &&
                    matchesTypeDescriptor<typename IsStdMap<U>::Value>(type.value_type());
            }
            else if constexpr (IsStdFunction<U>::value)
            {
                return matchesFunctionDescriptor<typename IsStdFunction<U>::Signature>(
                    type,
                    std::make_index_sequence<FunctionTraits<typename IsStdFunction<U>::Signature>::Arity>{}
                );
            }
            else
            {
                return false;
            }
        }

        template <typename T>
        void validateFieldHostType(const WioModuleType* ownerType,
                                   const WioModuleField* fieldEntry,
                                   std::string_view fieldName,
                                   std::string_view bindingKind)
        {
            if (fieldEntry == nullptr)
                throw Error(ErrorCode::FieldNotFound, "Wio SDK expected a valid field descriptor.");

            const TypeDescriptorView exportedType(fieldEntry->typeDescriptor);
            if (matchesTypeDescriptor<T>(exportedType))
                return;

            std::ostringstream message;
            message << "Wio SDK host type mismatch for " << bindingKind << " field '" << fieldName << "'";
            if (ownerType != nullptr && ownerType->logicalName != nullptr)
                message << " on exported type '" << ownerType->logicalName << "'";
            message << ": field exports '" << descriptorDisplayName(exportedType)
                    << "' but host requested '" << hostFieldTypeName<T>() << "'.";
            throw Error(ErrorCode::SignatureMismatch, message.str());
        }

        inline const char* invokeStatusName(std::int32_t status) noexcept
        {
            switch (status)
            {
            case WIO_INVOKE_OK: return "ok";
            case WIO_INVOKE_EXPORT_NOT_FOUND: return "export_not_found";
            case WIO_INVOKE_BAD_ARGUMENTS: return "bad_arguments";
            case WIO_INVOKE_TYPE_MISMATCH: return "type_mismatch";
            case WIO_INVOKE_RESULT_REQUIRED: return "result_required";
            case WIO_INVOKE_NOT_CALLABLE: return "not_callable";
            default: return "unknown";
            }
        }

        template <typename T>
        WioValue toWioValue(T&& value)
        {
            using U = Decay<T>;

            WioValue result{};
            result.type = getAbiType<U>();

            if constexpr (std::is_same_v<U, bool>)
            {
                result.value.v_bool = value;
            }
            else if constexpr (std::is_same_v<U, char>)
            {
                result.value.v_char = value;
            }
            else if constexpr (std::is_same_v<U, unsigned char>)
            {
                result.value.v_uchar = value;
            }
            else if constexpr (std::is_same_v<U, std::byte>)
            {
                result.value.v_byte = std::to_integer<std::uint8_t>(value);
            }
            else if constexpr (std::is_same_v<U, std::intptr_t>)
            {
                result.value.v_isize = value;
            }
            else if constexpr (std::is_same_v<U, std::uintptr_t>)
            {
                result.value.v_usize = value;
            }
            else if constexpr (std::is_floating_point_v<U>)
            {
                if constexpr (sizeof(U) == sizeof(float))
                    result.value.v_f32 = static_cast<float>(value);
                else
                    result.value.v_f64 = static_cast<double>(value);
            }
            else if constexpr (std::is_integral_v<U>)
            {
                if constexpr (std::is_same_v<U, signed char>)
                    result.value.v_i8 = static_cast<std::int8_t>(value);
                else if constexpr (std::is_unsigned_v<U>)
                {
                    if constexpr (sizeof(U) == 1)
                        result.value.v_u8 = static_cast<std::uint8_t>(value);
                    else if constexpr (sizeof(U) == 2)
                        result.value.v_u16 = static_cast<std::uint16_t>(value);
                    else if constexpr (sizeof(U) == 4)
                        result.value.v_u32 = static_cast<std::uint32_t>(value);
                    else
                        result.value.v_u64 = static_cast<std::uint64_t>(value);
                }
                else
                {
                    if constexpr (sizeof(U) == 1)
                        result.value.v_i8 = static_cast<std::int8_t>(value);
                    else if constexpr (sizeof(U) == 2)
                        result.value.v_i16 = static_cast<std::int16_t>(value);
                    else if constexpr (sizeof(U) == 4)
                        result.value.v_i32 = static_cast<std::int32_t>(value);
                    else
                        result.value.v_i64 = static_cast<std::int64_t>(value);
                }
            }

            return result;
        }

        template <typename T>
        T fromWioValue(const WioValue& value)
        {
            using U = Decay<T>;
            const WioAbiType expectedType = getAbiType<U>();
            if (value.type != expectedType)
            {
                std::ostringstream message;
                message << "Wio SDK type mismatch: expected '" << abiTypeName(expectedType)
                        << "' but received '" << abiTypeName(value.type) << "'.";
                throw Error(ErrorCode::SignatureMismatch, message.str());
            }

            if constexpr (std::is_same_v<U, bool>)
            {
                return static_cast<T>(value.value.v_bool);
            }
            else if constexpr (std::is_same_v<U, char>)
            {
                return static_cast<T>(value.value.v_char);
            }
            else if constexpr (std::is_same_v<U, unsigned char>)
            {
                return static_cast<T>(value.value.v_uchar);
            }
            else if constexpr (std::is_same_v<U, std::byte>)
            {
                return static_cast<T>(std::byte{ value.value.v_byte });
            }
            else if constexpr (std::is_same_v<U, std::intptr_t>)
            {
                return static_cast<T>(value.value.v_isize);
            }
            else if constexpr (std::is_same_v<U, std::uintptr_t>)
            {
                return static_cast<T>(value.value.v_usize);
            }
            else if constexpr (std::is_floating_point_v<U>)
            {
                if constexpr (sizeof(U) == sizeof(float))
                    return static_cast<T>(value.value.v_f32);
                else
                    return static_cast<T>(value.value.v_f64);
            }
            else if constexpr (std::is_integral_v<U>)
            {
                if constexpr (std::is_same_v<U, signed char>)
                    return static_cast<T>(value.value.v_i8);
                else if constexpr (std::is_unsigned_v<U>)
                {
                    if constexpr (sizeof(U) == 1)
                        return static_cast<T>(value.value.v_u8);
                    else if constexpr (sizeof(U) == 2)
                        return static_cast<T>(value.value.v_u16);
                    else if constexpr (sizeof(U) == 4)
                        return static_cast<T>(value.value.v_u32);
                    else
                        return static_cast<T>(value.value.v_u64);
                }
                else
                {
                    if constexpr (sizeof(U) == 1)
                        return static_cast<T>(value.value.v_i8);
                    else if constexpr (sizeof(U) == 2)
                        return static_cast<T>(value.value.v_i16);
                    else if constexpr (sizeof(U) == 4)
                        return static_cast<T>(value.value.v_i32);
                    else
                        return static_cast<T>(value.value.v_i64);
                }
            }
        }

        template <typename Signature, size_t... Indices>
        void validateExportSignature(const WioModuleExport* exportEntry,
                                     std::string_view name,
                                     std::string_view bindingKind,
                                     std::index_sequence<Indices...>)
        {
            using Traits = FunctionTraits<Signature>;

            if (exportEntry == nullptr || exportEntry->invoke == nullptr)
            {
                std::ostringstream message;
                message << "Wio SDK could not resolve " << bindingKind << " '" << name << "'.";
                throw Error(ErrorCode::ExportNotFound, message.str());
            }

            constexpr std::uint32_t expectedCount = static_cast<std::uint32_t>(Traits::Arity);
            if (exportEntry->parameterCount != expectedCount)
            {
                std::ostringstream message;
                message << "Wio SDK signature mismatch for " << bindingKind << " '" << name
                        << "': expected " << expectedCount << " parameters but module exports "
                        << exportEntry->parameterCount << ".";
                throw Error(ErrorCode::SignatureMismatch, message.str());
            }

            const WioAbiType expectedReturnType = getAbiType<typename Traits::ReturnType>();
            if (exportEntry->returnType != expectedReturnType)
            {
                std::ostringstream message;
                message << "Wio SDK signature mismatch for " << bindingKind << " '" << name
                        << "': expected return type '" << abiTypeName(expectedReturnType)
                        << "' but module exports '" << abiTypeName(exportEntry->returnType) << "'.";
                throw Error(ErrorCode::SignatureMismatch, message.str());
            }

            const std::array<WioAbiType, Traits::Arity> expectedParameterTypes = {
                getAbiType<typename Traits::template Argument<Indices>>()...
            };

            for (size_t i = 0; i < expectedParameterTypes.size(); ++i)
            {
                const WioAbiType actualType = exportEntry->parameterTypes != nullptr
                    ? exportEntry->parameterTypes[i]
                    : WIO_ABI_UNKNOWN;

                if (actualType != expectedParameterTypes[i])
                {
                    std::ostringstream message;
                    message << "Wio SDK signature mismatch for " << bindingKind << " '" << name
                            << "': parameter " << i << " expected '" << abiTypeName(expectedParameterTypes[i])
                            << "' but module exports '" << abiTypeName(actualType) << "'.";
                    throw Error(ErrorCode::SignatureMismatch, message.str());
                }
            }
        }

        template <typename Signature>
        void validateExportSignature(const WioModuleExport* exportEntry,
                                     std::string_view name,
                                     std::string_view bindingKind)
        {
            using Traits = FunctionTraits<Signature>;
            validateExportSignature<Signature>(
                exportEntry,
                name,
                bindingKind,
                std::make_index_sequence<Traits::Arity>{}
            );
        }

        template <typename Signature, size_t... Indices>
        auto bindExport(const WioModuleExport* exportEntry,
                        std::string name,
                        std::string bindingKind,
                        std::index_sequence<Indices...>) -> typename FunctionTraits<Signature>::StdFunction
        {
            using Traits = FunctionTraits<Signature>;
            validateExportSignature<Signature>(exportEntry, name, bindingKind);

            return typename Traits::StdFunction(
                [exportEntry, name = std::move(name), bindingKind = std::move(bindingKind)](typename Traits::template Argument<Indices>... args) -> typename Traits::ReturnType
                {
                    constexpr size_t argCount = Traits::Arity;
                    std::array<WioValue, argCount> wioArgs{ toWioValue(args)... };
                    WioValue result{};

                    const std::int32_t status = exportEntry->invoke(
                        argCount > 0 ? wioArgs.data() : nullptr,
                        static_cast<std::uint32_t>(argCount),
                        std::is_void_v<typename Traits::ReturnType> ? nullptr : &result
                    );

                    if (status != WIO_INVOKE_OK)
                    {
                        std::ostringstream message;
                        message << "Wio SDK invoke failure for " << bindingKind << " '" << name
                                << "': " << invokeStatusName(status) << '.';
                        throw Error(ErrorCode::InvokeFailed, message.str());
                    }

                    if constexpr (std::is_void_v<typename Traits::ReturnType>)
                    {
                        return;
                    }
                    else
                    {
                        return fromWioValue<typename Traits::ReturnType>(result);
                    }
                }
            );
        }

        template <typename Signature>
        auto bindExport(const WioModuleExport* exportEntry,
                        std::string name,
                        std::string bindingKind) -> typename FunctionTraits<Signature>::StdFunction
        {
            using Traits = FunctionTraits<Signature>;
            return bindExport<Signature>(
                exportEntry,
                std::move(name),
                std::move(bindingKind),
                std::make_index_sequence<Traits::Arity>{}
            );
        }

        template <typename Signature, size_t... Indices>
        auto bindReloadable(std::function<typename FunctionTraits<Signature>::StdFunction(std::string_view)> resolver,
                            std::string name,
                            std::index_sequence<Indices...>) -> typename FunctionTraits<Signature>::StdFunction
        {
            using Traits = FunctionTraits<Signature>;
            return typename Traits::StdFunction(
                [resolver = std::move(resolver), name = std::move(name)](typename Traits::template Argument<Indices>... args) -> typename Traits::ReturnType
                {
                    auto callable = resolver(name);
                    if constexpr (std::is_void_v<typename Traits::ReturnType>)
                    {
                        callable(std::forward<typename Traits::template Argument<Indices>>(args)...);
                        return;
                    }
                    else
                    {
                        return callable(std::forward<typename Traits::template Argument<Indices>>(args)...);
                    }
                }
            );
        }

        template <typename Signature>
        auto bindReloadable(std::function<typename FunctionTraits<Signature>::StdFunction(std::string_view)> resolver,
                            std::string name) -> typename FunctionTraits<Signature>::StdFunction
        {
            using Traits = FunctionTraits<Signature>;
            return bindReloadable<Signature>(
                std::move(resolver),
                std::move(name),
                std::make_index_sequence<Traits::Arity>{}
            );
        }

        inline const WioModuleType* requireModuleType(const WioModuleApi* api,
                                                      std::string_view logicalName,
                                                      WioModuleTypeKind expectedKind)
        {
            if (api == nullptr)
                throw Error(ErrorCode::ApiUnavailable, "Wio SDK expected a valid module API pointer.");

            const std::string ownedName(logicalName);
            const WioModuleType* typeEntry = WioFindModuleType(api, ownedName.c_str());
            if (typeEntry == nullptr)
            {
                std::ostringstream message;
                message << "Wio SDK could not find exported type '" << ownedName << "'.";
                throw Error(ErrorCode::TypeNotFound, message.str());
            }

            if (typeEntry->kind != expectedKind)
            {
                std::ostringstream message;
                message << "Wio SDK resolved '" << ownedName << "' as the wrong exported type kind.";
                throw Error(ErrorCode::SignatureMismatch, message.str());
            }

            return typeEntry;
        }

        inline const WioModuleField* requireTypeField(const WioModuleType* typeEntry,
                                                      std::string_view fieldName,
                                                      std::string_view bindingKind)
        {
            if (typeEntry == nullptr)
                throw Error(ErrorCode::TypeNotFound, "Wio SDK expected a valid exported type descriptor.");

            const std::string ownedFieldName(fieldName);
            const WioModuleField* fieldEntry = WioFindModuleField(typeEntry, ownedFieldName.c_str());
            if (fieldEntry == nullptr)
            {
                std::ostringstream message;
                message << "Wio SDK could not find " << bindingKind << " field '" << ownedFieldName
                        << "' on exported type '" << (typeEntry->logicalName ? typeEntry->logicalName : "<unknown>") << "'.";
                throw Error(ErrorCode::FieldNotFound, message.str());
            }

            return fieldEntry;
        }

        inline FieldInfo makeFieldInfo(const WioModuleField& fieldEntry)
        {
            return FieldInfo{
                fieldEntry.fieldName != nullptr ? std::string_view(fieldEntry.fieldName) : std::string_view{},
                toFieldAccess(fieldEntry.accessModifier),
                (fieldEntry.flags & WIO_MODULE_FIELD_READABLE) != 0u,
                (fieldEntry.flags & WIO_MODULE_FIELD_WRITABLE) != 0u,
                (fieldEntry.flags & WIO_MODULE_FIELD_READONLY) != 0u,
                TypeDescriptorView(fieldEntry.typeDescriptor),
                &fieldEntry
            };
        }

        inline std::vector<FieldInfo> listTypeFields(const WioModuleType* typeEntry)
        {
            std::vector<FieldInfo> fields;
            if (typeEntry == nullptr || typeEntry->fields == nullptr || typeEntry->fieldCount == 0u)
                return fields;

            fields.reserve(typeEntry->fieldCount);
            for (std::uint32_t index = 0; index < typeEntry->fieldCount; ++index)
                fields.push_back(makeFieldInfo(typeEntry->fields[index]));

            return fields;
        }

        inline FieldInfo getTypeFieldInfo(const WioModuleType* typeEntry,
                                          std::string_view fieldName,
                                          std::string_view bindingKind)
        {
            return makeFieldInfo(*requireTypeField(typeEntry, fieldName, bindingKind));
        }

        inline std::uintptr_t createModuleInstance(const WioModuleType* typeEntry, std::string_view bindingKind)
        {
            if (typeEntry == nullptr || typeEntry->createExport == nullptr || typeEntry->createExport->invoke == nullptr)
            {
                std::ostringstream message;
                message << "Wio SDK could not create exported " << bindingKind << " type '"
                        << (typeEntry && typeEntry->logicalName ? typeEntry->logicalName : "<unknown>") << "'.";
                throw Error(ErrorCode::InvokeFailed, message.str());
            }

            WioValue result{};
            const std::int32_t status = typeEntry->createExport->invoke(nullptr, 0u, &result);
            if (status != WIO_INVOKE_OK)
            {
                std::ostringstream message;
                message << "Wio SDK failed to create exported " << bindingKind << " type '"
                        << (typeEntry->logicalName ? typeEntry->logicalName : "<unknown>")
                        << "': " << invokeStatusName(status) << '.';
                throw Error(ErrorCode::InvokeFailed, message.str());
            }

            if (result.type != WIO_ABI_USIZE)
            {
                std::ostringstream message;
                message << "Wio SDK received an invalid instance handle result while creating exported "
                        << bindingKind << " type '" << (typeEntry->logicalName ? typeEntry->logicalName : "<unknown>") << "'.";
                throw Error(ErrorCode::SignatureMismatch, message.str());
            }

            return result.value.v_usize;
        }

        inline const WioModuleExport* requireTypeConstructor(const WioModuleType* typeEntry,
                                                             std::uint32_t parameterCount,
                                                             const WioAbiType* parameterTypes,
                                                             std::string_view bindingKind)
        {
            if (typeEntry == nullptr)
                throw Error(ErrorCode::TypeNotFound, "Wio SDK expected a valid exported type descriptor.");

            if (parameterCount == 0 && typeEntry->createExport != nullptr)
                return typeEntry->createExport;

            if (const WioModuleConstructor* constructorEntry = WioFindModuleConstructor(typeEntry, parameterCount, parameterTypes);
                constructorEntry != nullptr)
            {
                return constructorEntry->exportEntry;
            }

            std::ostringstream message;
            message << "Wio SDK could not find a matching " << bindingKind << " constructor for exported type '"
                    << (typeEntry->logicalName ? typeEntry->logicalName : "<unknown>")
                    << "' with " << parameterCount << " parameters.";
            throw Error(ErrorCode::SignatureMismatch, message.str());
        }

        template <typename... TArgs>
        std::uintptr_t constructModuleInstance(const WioModuleType* typeEntry,
                                               std::string_view bindingKind,
                                               TArgs&&... args)
        {
            constexpr std::uint32_t parameterCount = static_cast<std::uint32_t>(sizeof...(TArgs));
            const std::array<WioAbiType, sizeof...(TArgs)> parameterTypes{ getAbiType<TArgs>()... };
            const WioModuleExport* constructorExport = requireTypeConstructor(
                typeEntry,
                parameterCount,
                parameterCount > 0 ? parameterTypes.data() : nullptr,
                bindingKind
            );

            if (constructorExport == nullptr || constructorExport->invoke == nullptr)
            {
                std::ostringstream message;
                message << "Wio SDK could not invoke a constructor for exported " << bindingKind
                        << " type '" << (typeEntry && typeEntry->logicalName ? typeEntry->logicalName : "<unknown>") << "'.";
                throw Error(ErrorCode::InvokeFailed, message.str());
            }

            if (constructorExport->returnType != WIO_ABI_USIZE)
            {
                std::ostringstream message;
                message << "Wio SDK received an invalid constructor signature for exported " << bindingKind
                        << " type '" << (typeEntry && typeEntry->logicalName ? typeEntry->logicalName : "<unknown>") << "'.";
                throw Error(ErrorCode::SignatureMismatch, message.str());
            }

            const std::array<WioValue, sizeof...(TArgs)> constructorArgs{ toWioValue(std::forward<TArgs>(args))... };
            WioValue result{};
            const std::int32_t status = constructorExport->invoke(
                parameterCount > 0 ? constructorArgs.data() : nullptr,
                parameterCount,
                &result
            );

            if (status != WIO_INVOKE_OK)
            {
                std::ostringstream message;
                message << "Wio SDK failed to construct exported " << bindingKind << " type '"
                        << (typeEntry && typeEntry->logicalName ? typeEntry->logicalName : "<unknown>")
                        << "': " << invokeStatusName(status) << '.';
                throw Error(ErrorCode::InvokeFailed, message.str());
            }

            if (result.type != WIO_ABI_USIZE)
            {
                std::ostringstream message;
                message << "Wio SDK received an invalid instance handle result while constructing exported "
                        << bindingKind << " type '" << (typeEntry && typeEntry->logicalName ? typeEntry->logicalName : "<unknown>") << "'.";
                throw Error(ErrorCode::SignatureMismatch, message.str());
            }

            return result.value.v_usize;
        }

        inline void destroyModuleInstance(const WioModuleType* typeEntry, std::uintptr_t handle) noexcept
        {
            if (typeEntry == nullptr || typeEntry->destroyExport == nullptr || typeEntry->destroyExport->invoke == nullptr || handle == 0)
                return;

            WioValue args[1]{};
            args[0] = toWioValue(handle);
            typeEntry->destroyExport->invoke(args, 1u, nullptr);
        }

        template <typename T>
        T getModuleFieldValue(const WioModuleType* typeEntry,
                              std::uintptr_t handle,
                              std::string_view fieldName,
                              std::string_view bindingKind)
        {
            const WioModuleField* fieldEntry = requireTypeField(typeEntry, fieldName, bindingKind);
            if ((fieldEntry->flags & WIO_MODULE_FIELD_READABLE) == 0u || fieldEntry->getterExport == nullptr)
            {
                std::ostringstream message;
                message << "Wio SDK field '" << fieldName << "' on exported type '"
                        << (typeEntry && typeEntry->logicalName ? typeEntry->logicalName : "<unknown>")
                        << "' is not readable.";
                throw Error(ErrorCode::FieldNotFound, message.str());
            }

            if constexpr (IsAbiScalarType<T>)
            {
                if (fieldEntry->getterExport->invoke != nullptr)
                {
                    WioValue args[1]{};
                    args[0] = toWioValue(handle);

                    WioValue result{};
                    const std::int32_t status = fieldEntry->getterExport->invoke(args, 1u, &result);
                    if (status != WIO_INVOKE_OK)
                    {
                        std::ostringstream message;
                        message << "Wio SDK failed to read field '" << fieldName << "' from exported type '"
                                << (typeEntry && typeEntry->logicalName ? typeEntry->logicalName : "<unknown>")
                                << "': " << invokeStatusName(status) << '.';
                        throw Error(ErrorCode::InvokeFailed, message.str());
                    }

                    return fromWioValue<T>(result);
                }
            }

            if (fieldEntry->getterExport->rawFunction == nullptr)
            {
                std::ostringstream message;
                message << "Wio SDK field '" << fieldName << "' on exported type '"
                        << (typeEntry && typeEntry->logicalName ? typeEntry->logicalName : "<unknown>")
                        << "' does not expose a raw getter bridge for this host type.";
                throw Error(ErrorCode::SignatureMismatch, message.str());
            }

            using Getter = T(*)(std::uintptr_t);
            auto getter = reinterpret_cast<Getter>(const_cast<void*>(fieldEntry->getterExport->rawFunction));
            return getter(handle);
        }

        template <typename T>
        void setModuleFieldValue(const WioModuleType* typeEntry,
                                 std::uintptr_t handle,
                                 std::string_view fieldName,
                                 T&& value,
                                 std::string_view bindingKind)
        {
            const WioModuleField* fieldEntry = requireTypeField(typeEntry, fieldName, bindingKind);
            if ((fieldEntry->flags & WIO_MODULE_FIELD_WRITABLE) == 0u || fieldEntry->setterExport == nullptr)
            {
                std::ostringstream message;
                message << "Wio SDK field '" << fieldName << "' on exported type '"
                        << (typeEntry && typeEntry->logicalName ? typeEntry->logicalName : "<unknown>")
                        << "' is not writable.";
                throw Error(ErrorCode::FieldNotWritable, message.str());
            }

            if constexpr (IsAbiScalarType<T>)
            {
                if (fieldEntry->setterExport->invoke != nullptr)
                {
                    WioValue args[2]{};
                    args[0] = toWioValue(handle);
                    args[1] = toWioValue(std::forward<T>(value));

                    const std::int32_t status = fieldEntry->setterExport->invoke(args, 2u, nullptr);
                    if (status != WIO_INVOKE_OK)
                    {
                        std::ostringstream message;
                        message << "Wio SDK failed to write field '" << fieldName << "' on exported type '"
                                << (typeEntry && typeEntry->logicalName ? typeEntry->logicalName : "<unknown>")
                                << "': " << invokeStatusName(status) << '.';
                        throw Error(ErrorCode::InvokeFailed, message.str());
                    }
                    return;
                }
            }

            if (fieldEntry->setterExport->rawFunction == nullptr)
            {
                std::ostringstream message;
                message << "Wio SDK field '" << fieldName << "' on exported type '"
                        << (typeEntry && typeEntry->logicalName ? typeEntry->logicalName : "<unknown>")
                        << "' does not expose a raw setter bridge for this host type.";
                throw Error(ErrorCode::SignatureMismatch, message.str());
            }

            using Setter = void(*)(std::uintptr_t, Decay<T>);
            auto setter = reinterpret_cast<Setter>(const_cast<void*>(fieldEntry->setterExport->rawFunction));
            setter(handle, std::forward<T>(value));
        }

        inline std::uintptr_t getModuleFieldHandle(const WioModuleType* typeEntry,
                                                   std::uintptr_t handle,
                                                   std::string_view fieldName,
                                                   std::string_view bindingKind)
        {
            return getModuleFieldValue<std::uintptr_t>(typeEntry, handle, fieldName, bindingKind);
        }

        template <typename Signature, size_t... Indices>
        auto bindBoundMethod(const WioModuleType* typeEntry,
                             std::uintptr_t handle,
                             std::string methodName,
                             std::string bindingKind,
                             std::index_sequence<Indices...>) -> typename FunctionTraits<Signature>::StdFunction
        {
            using Traits = FunctionTraits<Signature>;
            if (typeEntry == nullptr)
                throw Error(ErrorCode::TypeNotFound, "Wio SDK expected a valid exported type descriptor.");

            const WioAbiType expectedReturnType = getAbiType<typename Traits::ReturnType>();
            const std::array<WioAbiType, Traits::Arity + 1> expectedParameterTypes = {
                WIO_ABI_USIZE,
                getAbiType<typename Traits::template Argument<Indices>>()...
            };

            const WioModuleMethod* methodEntry = WioFindModuleMethodOverload(
                typeEntry,
                methodName.c_str(),
                expectedReturnType,
                static_cast<std::uint32_t>(expectedParameterTypes.size()),
                expectedParameterTypes.data()
            );

            if (methodEntry == nullptr)
            {
                std::ostringstream message;
                message << "Wio SDK could not find " << bindingKind << " method overload '" << methodName
                        << "' on exported type '" << (typeEntry->logicalName ? typeEntry->logicalName : "<unknown>") << "'.";
                throw Error(ErrorCode::MethodNotFound, message.str());
            }

            const WioModuleExport* exportEntry = methodEntry ? methodEntry->exportEntry : nullptr;

            if (exportEntry == nullptr || exportEntry->invoke == nullptr)
            {
                std::ostringstream message;
                message << "Wio SDK could not bind " << bindingKind << " method '" << methodName << "'.";
                throw Error(ErrorCode::MethodNotFound, message.str());
            }

            constexpr std::uint32_t expectedCount = static_cast<std::uint32_t>(Traits::Arity + 1);
            if (exportEntry->parameterCount != expectedCount)
            {
                std::ostringstream message;
                message << "Wio SDK signature mismatch for " << bindingKind << " method '" << methodName
                        << "': expected " << expectedCount << " parameters including the instance handle but module exports "
                        << exportEntry->parameterCount << '.';
                throw Error(ErrorCode::SignatureMismatch, message.str());
            }

            if (exportEntry->parameterTypes == nullptr || exportEntry->parameterTypes[0] != WIO_ABI_USIZE)
            {
                std::ostringstream message;
                message << "Wio SDK signature mismatch for " << bindingKind << " method '" << methodName
                        << "': first parameter must be an exported instance handle.";
                throw Error(ErrorCode::SignatureMismatch, message.str());
            }

            if (exportEntry->returnType != expectedReturnType)
            {
                std::ostringstream message;
                message << "Wio SDK signature mismatch for " << bindingKind << " method '" << methodName
                        << "': expected return type '" << abiTypeName(expectedReturnType)
                        << "' but module exports '" << abiTypeName(exportEntry->returnType) << "'.";
                throw Error(ErrorCode::SignatureMismatch, message.str());
            }

            for (size_t i = 0; i < Traits::Arity; ++i)
            {
                const WioAbiType actualType = exportEntry->parameterTypes[i + 1];
                if (actualType != expectedParameterTypes[i + 1])
                {
                    std::ostringstream message;
                    message << "Wio SDK signature mismatch for " << bindingKind << " method '" << methodName
                            << "': parameter " << i << " expected '" << abiTypeName(expectedParameterTypes[i + 1])
                            << "' but module exports '" << abiTypeName(actualType) << "'.";
                    throw Error(ErrorCode::SignatureMismatch, message.str());
                }
            }

            return typename Traits::StdFunction(
                [exportEntry, handle, methodName = std::move(methodName), bindingKind = std::move(bindingKind)](typename Traits::template Argument<Indices>... args) -> typename Traits::ReturnType
                {
                    constexpr size_t argCount = Traits::Arity + 1;
                    std::array<WioValue, argCount> wioArgs{ toWioValue(handle), toWioValue(args)... };
                    WioValue result{};

                    const std::int32_t status = exportEntry->invoke(
                        wioArgs.data(),
                        static_cast<std::uint32_t>(argCount),
                        std::is_void_v<typename Traits::ReturnType> ? nullptr : &result
                    );

                    if (status != WIO_INVOKE_OK)
                    {
                        std::ostringstream message;
                        message << "Wio SDK invoke failure for " << bindingKind << " method '" << methodName
                                << "': " << invokeStatusName(status) << '.';
                        throw Error(ErrorCode::InvokeFailed, message.str());
                    }

                    if constexpr (std::is_void_v<typename Traits::ReturnType>)
                    {
                        return;
                    }
                    else
                    {
                        return fromWioValue<typename Traits::ReturnType>(result);
                    }
                }
            );
        }

        template <typename Signature>
        auto bindBoundMethod(const WioModuleType* typeEntry,
                             std::uintptr_t handle,
                             std::string methodName,
                             std::string bindingKind) -> typename FunctionTraits<Signature>::StdFunction
        {
            using Traits = FunctionTraits<Signature>;
            return bindBoundMethod<Signature>(
                typeEntry,
                handle,
                std::move(methodName),
                std::move(bindingKind),
                std::make_index_sequence<Traits::Arity>{}
            );
        }
    }

    class WioFieldAccessor;
    class WioObject;
    class WioComponent;
    class WioObjectType;
    class WioComponentType;

    class WioFieldAccessor
    {
    public:
        WioFieldAccessor() = default;

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return fieldEntry_ != nullptr;
        }

        [[nodiscard]] const FieldInfo& info() const noexcept
        {
            return info_;
        }

        [[nodiscard]] std::string_view name() const noexcept
        {
            return info_.name;
        }

        [[nodiscard]] TypeDescriptorView type() const noexcept
        {
            return info_.type;
        }

        [[nodiscard]] FieldAccess access() const noexcept
        {
            return info_.access;
        }

        [[nodiscard]] bool can_read() const noexcept
        {
            return info_.can_read();
        }

        [[nodiscard]] bool can_write() const noexcept
        {
            return info_.can_write();
        }

        [[nodiscard]] bool is_read_only() const noexcept
        {
            return info_.is_read_only();
        }

        template <typename T>
        [[nodiscard]] bool can_access_as() const noexcept
        {
            return detail::matchesTypeDescriptor<T>(info_.type);
        }

        template <typename T>
        [[nodiscard]] T get_as() const
        {
            detail::validateFieldHostType<T>(type_, fieldEntry_, name(), bindingKind_);
            return detail::getModuleFieldValue<T>(type_, handle_, name(), bindingKind_);
        }

        template <typename T>
        void set_as(T&& value) const
        {
            detail::validateFieldHostType<detail::Decay<T>>(type_, fieldEntry_, name(), bindingKind_);
            detail::setModuleFieldValue(type_, handle_, name(), std::forward<T>(value), bindingKind_);
        }

        [[nodiscard]] WioValue get_scalar_value() const;
        void set_scalar_value(const WioValue& value) const;

        [[nodiscard]] wio::string get_string() const
        {
            return get_as<wio::string>();
        }

        void set_string(wio::string value) const
        {
            set_as(std::move(value));
        }

        template <typename T>
        [[nodiscard]] WioArray<T> get_array() const
        {
            return WioArray<T>(get_as<wio::DArray<T>>());
        }

        template <typename T>
        void set_array(WioArray<T> value) const
        {
            set_as(std::move(value).take());
        }

        template <typename T>
        void set_array(wio::DArray<T> value) const
        {
            set_as(std::move(value));
        }

        template <typename T, std::size_t N>
        [[nodiscard]] WioStaticArray<T, N> get_static_array() const
        {
            return WioStaticArray<T, N>(get_as<wio::SArray<T, N>>());
        }

        template <typename T, std::size_t N>
        void set_static_array(WioStaticArray<T, N> value) const
        {
            set_as(std::move(value).take());
        }

        template <typename T, std::size_t N>
        void set_static_array(wio::SArray<T, N> value) const
        {
            set_as(std::move(value));
        }

        template <typename K, typename V>
        [[nodiscard]] WioDict<K, V> get_dict() const
        {
            return WioDict<K, V>(get_as<wio::Dict<K, V>>());
        }

        template <typename K, typename V>
        void set_dict(WioDict<K, V> value) const
        {
            set_as(std::move(value).take());
        }

        template <typename K, typename V>
        void set_dict(wio::Dict<K, V> value) const
        {
            set_as(std::move(value));
        }

        template <typename K, typename V>
        [[nodiscard]] WioTree<K, V> get_tree() const
        {
            return WioTree<K, V>(get_as<wio::Tree<K, V>>());
        }

        template <typename K, typename V>
        void set_tree(WioTree<K, V> value) const
        {
            set_as(std::move(value).take());
        }

        template <typename K, typename V>
        void set_tree(wio::Tree<K, V> value) const
        {
            set_as(std::move(value));
        }

        template <typename Signature>
        [[nodiscard]] WioFunction<Signature> get_function() const
        {
            using RawFunction = typename detail::FunctionTraits<Signature>::StdFunction;
            return WioFunction<Signature>(get_as<RawFunction>());
        }

        template <typename Signature>
        void set_function(WioFunction<Signature> value) const
        {
            set_as(std::move(value).take());
        }

        template <typename Signature>
        void set_function(typename detail::FunctionTraits<Signature>::StdFunction value) const
        {
            set_as(std::move(value));
        }

        [[nodiscard]] WioObject get_object() const;
        [[nodiscard]] WioComponent get_component() const;
        void set_object(const WioObject& value) const;
        void set_component(const WioComponent& value) const;

    private:
        friend class WioObject;
        friend class WioComponent;

        WioFieldAccessor(const WioModuleApi* api,
                         const WioModuleType* typeEntry,
                         std::uintptr_t handle,
                         const WioModuleField* fieldEntry,
                         std::string_view bindingKind) noexcept
            : api_(api),
              type_(typeEntry),
              handle_(handle),
              fieldEntry_(fieldEntry),
              bindingKind_(bindingKind),
              info_(fieldEntry != nullptr ? detail::makeFieldInfo(*fieldEntry) : FieldInfo{})
        {
        }

        const WioModuleApi* api_ = nullptr;
        const WioModuleType* type_ = nullptr;
        std::uintptr_t handle_ = 0;
        const WioModuleField* fieldEntry_ = nullptr;
        std::string_view bindingKind_{};
        FieldInfo info_{};
    };

    class WioObject
    {
    public:
        WioObject() = default;

        WioObject(const WioObject&) = delete;
        WioObject& operator=(const WioObject&) = delete;

        WioObject(WioObject&& other) noexcept;
        WioObject& operator=(WioObject&& other) noexcept;
        ~WioObject();

        [[nodiscard]] explicit operator bool() const noexcept;
        [[nodiscard]] std::uintptr_t handle() const noexcept;
        [[nodiscard]] bool owns_handle() const noexcept;
        [[nodiscard]] bool is_borrowed() const noexcept;
        [[nodiscard]] const WioModuleType& descriptor() const;
        [[nodiscard]] std::vector<FieldInfo> list_fields() const;
        [[nodiscard]] FieldInfo field_info(std::string_view fieldName) const;
        [[nodiscard]] WioFieldAccessor field(std::string_view fieldName) const;

        template <typename T>
        T get(std::string_view fieldName) const
        {
            detail::validateFieldHostType<T>(type_, detail::requireTypeField(type_, fieldName, "object"), fieldName, "object");
            return detail::getModuleFieldValue<T>(type_, handle_, fieldName, "object");
        }

        template <typename T>
        [[nodiscard]] WioArray<T> get_array(std::string_view fieldName) const
        {
            return WioArray<T>(get<wio::DArray<T>>(fieldName));
        }

        template <typename T, std::size_t N>
        [[nodiscard]] WioStaticArray<T, N> get_static_array(std::string_view fieldName) const
        {
            return WioStaticArray<T, N>(get<wio::SArray<T, N>>(fieldName));
        }

        template <typename K, typename V>
        [[nodiscard]] WioDict<K, V> get_dict(std::string_view fieldName) const
        {
            return WioDict<K, V>(get<wio::Dict<K, V>>(fieldName));
        }

        template <typename K, typename V>
        [[nodiscard]] WioTree<K, V> get_tree(std::string_view fieldName) const
        {
            return WioTree<K, V>(get<wio::Tree<K, V>>(fieldName));
        }

        template <typename Signature>
        [[nodiscard]] WioFunction<Signature> get_function(std::string_view fieldName) const
        {
            using RawFunction = typename detail::FunctionTraits<Signature>::StdFunction;
            return WioFunction<Signature>(get<RawFunction>(fieldName));
        }

        template <typename T>
        void set(std::string_view fieldName, T&& value) const
        {
            detail::validateFieldHostType<detail::Decay<T>>(type_, detail::requireTypeField(type_, fieldName, "object"), fieldName, "object");
            detail::setModuleFieldValue(type_, handle_, fieldName, std::forward<T>(value), "object");
        }

        template <typename T>
        void set_array(std::string_view fieldName, WioArray<T> value) const
        {
            set(fieldName, std::move(value).take());
        }

        template <typename T>
        void set_array(std::string_view fieldName, wio::DArray<T> value) const
        {
            set(fieldName, std::move(value));
        }

        template <typename T, std::size_t N>
        void set_static_array(std::string_view fieldName, WioStaticArray<T, N> value) const
        {
            set(fieldName, std::move(value).take());
        }

        template <typename T, std::size_t N>
        void set_static_array(std::string_view fieldName, wio::SArray<T, N> value) const
        {
            set(fieldName, std::move(value));
        }

        template <typename K, typename V>
        void set_dict(std::string_view fieldName, WioDict<K, V> value) const
        {
            set(fieldName, std::move(value).take());
        }

        template <typename K, typename V>
        void set_dict(std::string_view fieldName, wio::Dict<K, V> value) const
        {
            set(fieldName, std::move(value));
        }

        template <typename K, typename V>
        void set_tree(std::string_view fieldName, WioTree<K, V> value) const
        {
            set(fieldName, std::move(value).take());
        }

        template <typename K, typename V>
        void set_tree(std::string_view fieldName, wio::Tree<K, V> value) const
        {
            set(fieldName, std::move(value));
        }

        template <typename Signature>
        void set_function(std::string_view fieldName, WioFunction<Signature> value) const
        {
            set(fieldName, std::move(value).take());
        }

        template <typename Signature>
        void set_function(std::string_view fieldName, typename detail::FunctionTraits<Signature>::StdFunction value) const
        {
            set(fieldName, std::move(value));
        }

        [[nodiscard]] WioObject get_object(std::string_view fieldName) const;
        [[nodiscard]] WioComponent get_component(std::string_view fieldName) const;
        void set_object(std::string_view fieldName, const WioObject& value) const;
        void set_component(std::string_view fieldName, const WioComponent& value) const;

        template <typename Signature>
        auto method(std::string_view methodName) const -> typename detail::FunctionTraits<Signature>::StdFunction
        {
            return detail::bindBoundMethod<Signature>(type_, handle_, std::string(methodName), "object");
        }

        void reset() noexcept;

    private:
        friend class WioObjectType;
        friend class WioComponent;
        friend class WioFieldAccessor;

        WioObject(const WioModuleApi* api, const WioModuleType* typeEntry, std::uintptr_t instanceHandle, bool ownsHandle) noexcept
            : api_(api),
              type_(typeEntry),
              handle_(instanceHandle),
              ownsHandle_(ownsHandle)
        {
        }

        const WioModuleApi* api_ = nullptr;
        const WioModuleType* type_ = nullptr;
        std::uintptr_t handle_ = 0;
        bool ownsHandle_ = false;
    };

    class WioComponent
    {
    public:
        WioComponent() = default;

        WioComponent(const WioComponent&) = delete;
        WioComponent& operator=(const WioComponent&) = delete;

        WioComponent(WioComponent&& other) noexcept;
        WioComponent& operator=(WioComponent&& other) noexcept;
        ~WioComponent();

        [[nodiscard]] explicit operator bool() const noexcept;
        [[nodiscard]] std::uintptr_t handle() const noexcept;
        [[nodiscard]] bool owns_handle() const noexcept;
        [[nodiscard]] bool is_borrowed() const noexcept;
        [[nodiscard]] const WioModuleType& descriptor() const;
        [[nodiscard]] std::vector<FieldInfo> list_fields() const;
        [[nodiscard]] FieldInfo field_info(std::string_view fieldName) const;
        [[nodiscard]] WioFieldAccessor field(std::string_view fieldName) const;

        template <typename T>
        T get(std::string_view fieldName) const
        {
            detail::validateFieldHostType<T>(type_, detail::requireTypeField(type_, fieldName, "component"), fieldName, "component");
            return detail::getModuleFieldValue<T>(type_, handle_, fieldName, "component");
        }

        template <typename T>
        [[nodiscard]] WioArray<T> get_array(std::string_view fieldName) const
        {
            return WioArray<T>(get<wio::DArray<T>>(fieldName));
        }

        template <typename T, std::size_t N>
        [[nodiscard]] WioStaticArray<T, N> get_static_array(std::string_view fieldName) const
        {
            return WioStaticArray<T, N>(get<wio::SArray<T, N>>(fieldName));
        }

        template <typename K, typename V>
        [[nodiscard]] WioDict<K, V> get_dict(std::string_view fieldName) const
        {
            return WioDict<K, V>(get<wio::Dict<K, V>>(fieldName));
        }

        template <typename K, typename V>
        [[nodiscard]] WioTree<K, V> get_tree(std::string_view fieldName) const
        {
            return WioTree<K, V>(get<wio::Tree<K, V>>(fieldName));
        }

        template <typename Signature>
        [[nodiscard]] WioFunction<Signature> get_function(std::string_view fieldName) const
        {
            using RawFunction = typename detail::FunctionTraits<Signature>::StdFunction;
            return WioFunction<Signature>(get<RawFunction>(fieldName));
        }

        template <typename T>
        void set(std::string_view fieldName, T&& value) const
        {
            detail::validateFieldHostType<detail::Decay<T>>(type_, detail::requireTypeField(type_, fieldName, "component"), fieldName, "component");
            detail::setModuleFieldValue(type_, handle_, fieldName, std::forward<T>(value), "component");
        }

        template <typename T>
        void set_array(std::string_view fieldName, WioArray<T> value) const
        {
            set(fieldName, std::move(value).take());
        }

        template <typename T>
        void set_array(std::string_view fieldName, wio::DArray<T> value) const
        {
            set(fieldName, std::move(value));
        }

        template <typename T, std::size_t N>
        void set_static_array(std::string_view fieldName, WioStaticArray<T, N> value) const
        {
            set(fieldName, std::move(value).take());
        }

        template <typename T, std::size_t N>
        void set_static_array(std::string_view fieldName, wio::SArray<T, N> value) const
        {
            set(fieldName, std::move(value));
        }

        template <typename K, typename V>
        void set_dict(std::string_view fieldName, WioDict<K, V> value) const
        {
            set(fieldName, std::move(value).take());
        }

        template <typename K, typename V>
        void set_dict(std::string_view fieldName, wio::Dict<K, V> value) const
        {
            set(fieldName, std::move(value));
        }

        template <typename K, typename V>
        void set_tree(std::string_view fieldName, WioTree<K, V> value) const
        {
            set(fieldName, std::move(value).take());
        }

        template <typename K, typename V>
        void set_tree(std::string_view fieldName, wio::Tree<K, V> value) const
        {
            set(fieldName, std::move(value));
        }

        template <typename Signature>
        void set_function(std::string_view fieldName, WioFunction<Signature> value) const
        {
            set(fieldName, std::move(value).take());
        }

        template <typename Signature>
        void set_function(std::string_view fieldName, typename detail::FunctionTraits<Signature>::StdFunction value) const
        {
            set(fieldName, std::move(value));
        }

        [[nodiscard]] WioObject get_object(std::string_view fieldName) const;
        [[nodiscard]] WioComponent get_component(std::string_view fieldName) const;
        void set_object(std::string_view fieldName, const WioObject& value) const;
        void set_component(std::string_view fieldName, const WioComponent& value) const;

        void reset() noexcept;

    private:
        friend class WioComponentType;
        friend class WioObject;
        friend class WioFieldAccessor;

        WioComponent(const WioModuleApi* api, const WioModuleType* typeEntry, std::uintptr_t instanceHandle, bool ownsHandle) noexcept
            : api_(api),
              type_(typeEntry),
              handle_(instanceHandle),
              ownsHandle_(ownsHandle)
        {
        }

        const WioModuleApi* api_ = nullptr;
        const WioModuleType* type_ = nullptr;
        std::uintptr_t handle_ = 0;
        bool ownsHandle_ = false;
    };

    class WioObjectType
    {
    public:
        WioObjectType() = default;

        WioObjectType(const WioModuleApi* api, const WioModuleType* typeEntry) noexcept
            : api_(api),
              type_(typeEntry)
        {
        }

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return type_ != nullptr;
        }

        [[nodiscard]] const WioModuleType& descriptor() const
        {
            if (type_ == nullptr)
                throw Error(ErrorCode::TypeNotFound, "Wio SDK object type descriptor is not available.");
            return *type_;
        }

        [[nodiscard]] std::vector<FieldInfo> list_fields() const
        {
            return detail::listTypeFields(type_);
        }

        [[nodiscard]] FieldInfo field_info(std::string_view fieldName) const
        {
            return detail::getTypeFieldInfo(type_, fieldName, "object");
        }

        [[nodiscard]] WioObject create() const
        {
            return WioObject(api_, type_, detail::constructModuleInstance(type_, "object"), true);
        }

        template <typename... TArgs>
        [[nodiscard]] WioObject create(TArgs&&... args) const
        {
            return WioObject(api_, type_, detail::constructModuleInstance(type_, "object", std::forward<TArgs>(args)...), true);
        }

    private:
        const WioModuleApi* api_ = nullptr;
        const WioModuleType* type_ = nullptr;
    };

    class WioComponentType
    {
    public:
        WioComponentType() = default;

        WioComponentType(const WioModuleApi* api, const WioModuleType* typeEntry) noexcept
            : api_(api),
              type_(typeEntry)
        {
        }

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return type_ != nullptr;
        }

        [[nodiscard]] const WioModuleType& descriptor() const
        {
            if (type_ == nullptr)
                throw Error(ErrorCode::TypeNotFound, "Wio SDK component type descriptor is not available.");
            return *type_;
        }

        [[nodiscard]] std::vector<FieldInfo> list_fields() const
        {
            return detail::listTypeFields(type_);
        }

        [[nodiscard]] FieldInfo field_info(std::string_view fieldName) const
        {
            return detail::getTypeFieldInfo(type_, fieldName, "component");
        }

        [[nodiscard]] WioComponent create() const
        {
            return WioComponent(api_, type_, detail::constructModuleInstance(type_, "component"), true);
        }

        template <typename... TArgs>
        [[nodiscard]] WioComponent create(TArgs&&... args) const
        {
            return WioComponent(api_, type_, detail::constructModuleInstance(type_, "component", std::forward<TArgs>(args)...), true);
        }

    private:
        const WioModuleApi* api_ = nullptr;
        const WioModuleType* type_ = nullptr;
    };

    inline WioObject::WioObject(WioObject&& other) noexcept
    {
        *this = std::move(other);
    }

    inline WioObject& WioObject::operator=(WioObject&& other) noexcept
    {
        if (this == &other)
            return *this;

        reset();
        api_ = other.api_;
        type_ = other.type_;
        handle_ = other.handle_;
        ownsHandle_ = other.ownsHandle_;
        other.api_ = nullptr;
        other.type_ = nullptr;
        other.handle_ = 0;
        other.ownsHandle_ = false;
        return *this;
    }

    inline WioObject::~WioObject()
    {
        reset();
    }

    inline WioObject::operator bool() const noexcept
    {
        return type_ != nullptr && handle_ != 0;
    }

    inline std::uintptr_t WioObject::handle() const noexcept
    {
        return handle_;
    }

    inline bool WioObject::owns_handle() const noexcept
    {
        return ownsHandle_;
    }

    inline bool WioObject::is_borrowed() const noexcept
    {
        return *this && !ownsHandle_;
    }

    inline const WioModuleType& WioObject::descriptor() const
    {
        if (type_ == nullptr)
            throw Error(ErrorCode::TypeNotFound, "Wio SDK object handle is not bound to an exported object type.");
        return *type_;
    }

    inline std::vector<FieldInfo> WioObject::list_fields() const
    {
        return detail::listTypeFields(type_);
    }

    inline FieldInfo WioObject::field_info(std::string_view fieldName) const
    {
        return detail::getTypeFieldInfo(type_, fieldName, "object");
    }

    inline WioFieldAccessor WioObject::field(std::string_view fieldName) const
    {
        return WioFieldAccessor(api_, type_, handle_, detail::requireTypeField(type_, fieldName, "object"), "object");
    }

    inline void WioObject::reset() noexcept
    {
        if (ownsHandle_)
            detail::destroyModuleInstance(type_, handle_);

        api_ = nullptr;
        type_ = nullptr;
        handle_ = 0;
        ownsHandle_ = false;
    }

    inline WioComponent::WioComponent(WioComponent&& other) noexcept
    {
        *this = std::move(other);
    }

    inline WioComponent& WioComponent::operator=(WioComponent&& other) noexcept
    {
        if (this == &other)
            return *this;

        reset();
        api_ = other.api_;
        type_ = other.type_;
        handle_ = other.handle_;
        ownsHandle_ = other.ownsHandle_;
        other.api_ = nullptr;
        other.type_ = nullptr;
        other.handle_ = 0;
        other.ownsHandle_ = false;
        return *this;
    }

    inline WioComponent::~WioComponent()
    {
        reset();
    }

    inline WioComponent::operator bool() const noexcept
    {
        return type_ != nullptr && handle_ != 0;
    }

    inline std::uintptr_t WioComponent::handle() const noexcept
    {
        return handle_;
    }

    inline bool WioComponent::owns_handle() const noexcept
    {
        return ownsHandle_;
    }

    inline bool WioComponent::is_borrowed() const noexcept
    {
        return *this && !ownsHandle_;
    }

    inline const WioModuleType& WioComponent::descriptor() const
    {
        if (type_ == nullptr)
            throw Error(ErrorCode::TypeNotFound, "Wio SDK component handle is not bound to an exported component type.");
        return *type_;
    }

    inline std::vector<FieldInfo> WioComponent::list_fields() const
    {
        return detail::listTypeFields(type_);
    }

    inline FieldInfo WioComponent::field_info(std::string_view fieldName) const
    {
        return detail::getTypeFieldInfo(type_, fieldName, "component");
    }

    inline WioFieldAccessor WioComponent::field(std::string_view fieldName) const
    {
        return WioFieldAccessor(api_, type_, handle_, detail::requireTypeField(type_, fieldName, "component"), "component");
    }

    inline void WioComponent::reset() noexcept
    {
        if (ownsHandle_)
            detail::destroyModuleInstance(type_, handle_);

        api_ = nullptr;
        type_ = nullptr;
        handle_ = 0;
        ownsHandle_ = false;
    }

    inline WioValue WioFieldAccessor::get_scalar_value() const
    {
        if (!info_.is_primitive())
            throw Error(ErrorCode::SignatureMismatch, "Wio SDK expected a primitive field descriptor for scalar access.");

        if (!info_.can_read() || fieldEntry_ == nullptr || fieldEntry_->getterExport == nullptr || fieldEntry_->getterExport->invoke == nullptr)
            throw Error(ErrorCode::FieldNotFound, "Wio SDK scalar field is not readable.");

        WioValue args[1]{};
        args[0] = detail::toWioValue(handle_);

        WioValue result{};
        const std::int32_t status = fieldEntry_->getterExport->invoke(args, 1u, &result);
        if (status != WIO_INVOKE_OK)
        {
            std::ostringstream message;
            message << "Wio SDK failed to read scalar field '" << name() << "' from exported type '"
                    << (type_ && type_->logicalName ? type_->logicalName : "<unknown>")
                    << "': " << detail::invokeStatusName(status) << '.';
            throw Error(ErrorCode::InvokeFailed, message.str());
        }

        return result;
    }

    inline void WioFieldAccessor::set_scalar_value(const WioValue& value) const
    {
        if (!info_.is_primitive())
            throw Error(ErrorCode::SignatureMismatch, "Wio SDK expected a primitive field descriptor for scalar access.");

        if (!info_.can_write() || fieldEntry_ == nullptr || fieldEntry_->setterExport == nullptr || fieldEntry_->setterExport->invoke == nullptr)
            throw Error(ErrorCode::FieldNotWritable, "Wio SDK scalar field is not writable.");

        if (value.type != info_.abi_type())
        {
            std::ostringstream message;
            message << "Wio SDK scalar field '" << name() << "' expected '"
                    << detail::abiTypeName(info_.abi_type()) << "' but host provided '"
                    << detail::abiTypeName(value.type) << "'.";
            throw Error(ErrorCode::SignatureMismatch, message.str());
        }

        WioValue args[2]{};
        args[0] = detail::toWioValue(handle_);
        args[1] = value;

        const std::int32_t status = fieldEntry_->setterExport->invoke(args, 2u, nullptr);
        if (status != WIO_INVOKE_OK)
        {
            std::ostringstream message;
            message << "Wio SDK failed to write scalar field '" << name() << "' on exported type '"
                    << (type_ && type_->logicalName ? type_->logicalName : "<unknown>")
                    << "': " << detail::invokeStatusName(status) << '.';
            throw Error(ErrorCode::InvokeFailed, message.str());
        }
    }

    inline WioObject WioFieldAccessor::get_object() const
    {
        const auto* typeDescriptor = fieldEntry_ != nullptr ? fieldEntry_->typeDescriptor : nullptr;
        if (typeDescriptor == nullptr || typeDescriptor->kind != WIO_MODULE_TYPE_DESC_OBJECT)
            throw Error(ErrorCode::SignatureMismatch, "Wio SDK expected an object field descriptor.");

        const WioModuleType* nestedType = detail::requireModuleType(api_, typeDescriptor->logicalTypeName, WIO_MODULE_TYPE_OBJECT);
        const std::uintptr_t nestedHandle = detail::getModuleFieldHandle(type_, handle_, name(), bindingKind_);
        return WioObject(api_, nestedType, nestedHandle, false);
    }

    inline WioComponent WioFieldAccessor::get_component() const
    {
        const auto* typeDescriptor = fieldEntry_ != nullptr ? fieldEntry_->typeDescriptor : nullptr;
        if (typeDescriptor == nullptr || typeDescriptor->kind != WIO_MODULE_TYPE_DESC_COMPONENT)
            throw Error(ErrorCode::SignatureMismatch, "Wio SDK expected a component field descriptor.");

        const WioModuleType* nestedType = detail::requireModuleType(api_, typeDescriptor->logicalTypeName, WIO_MODULE_TYPE_COMPONENT);
        const std::uintptr_t nestedHandle = detail::getModuleFieldHandle(type_, handle_, name(), bindingKind_);
        return WioComponent(api_, nestedType, nestedHandle, false);
    }

    inline void WioFieldAccessor::set_object(const WioObject& value) const
    {
        const auto* typeDescriptor = fieldEntry_ != nullptr ? fieldEntry_->typeDescriptor : nullptr;
        if (typeDescriptor == nullptr || typeDescriptor->kind != WIO_MODULE_TYPE_DESC_OBJECT)
            throw Error(ErrorCode::SignatureMismatch, "Wio SDK expected an object field descriptor.");

        if (!value || value.descriptor().logicalName == nullptr || std::string_view(value.descriptor().logicalName) != std::string_view(typeDescriptor->logicalTypeName))
            throw Error(ErrorCode::SignatureMismatch, "Wio SDK object field assignment type mismatch.");

        detail::setModuleFieldValue(type_, handle_, name(), value.handle(), bindingKind_);
    }

    inline void WioFieldAccessor::set_component(const WioComponent& value) const
    {
        const auto* typeDescriptor = fieldEntry_ != nullptr ? fieldEntry_->typeDescriptor : nullptr;
        if (typeDescriptor == nullptr || typeDescriptor->kind != WIO_MODULE_TYPE_DESC_COMPONENT)
            throw Error(ErrorCode::SignatureMismatch, "Wio SDK expected a component field descriptor.");

        if (!value || value.descriptor().logicalName == nullptr || std::string_view(value.descriptor().logicalName) != std::string_view(typeDescriptor->logicalTypeName))
            throw Error(ErrorCode::SignatureMismatch, "Wio SDK component field assignment type mismatch.");

        detail::setModuleFieldValue(type_, handle_, name(), value.handle(), bindingKind_);
    }

    inline WioObject WioObject::get_object(std::string_view fieldName) const
    {
        return field(fieldName).get_object();
    }

    inline WioComponent WioObject::get_component(std::string_view fieldName) const
    {
        return field(fieldName).get_component();
    }

    inline void WioObject::set_object(std::string_view fieldName, const WioObject& value) const
    {
        field(fieldName).set_object(value);
    }

    inline void WioObject::set_component(std::string_view fieldName, const WioComponent& value) const
    {
        field(fieldName).set_component(value);
    }

    inline WioObject WioComponent::get_object(std::string_view fieldName) const
    {
        return field(fieldName).get_object();
    }

    inline WioComponent WioComponent::get_component(std::string_view fieldName) const
    {
        return field(fieldName).get_component();
    }

    inline void WioComponent::set_object(std::string_view fieldName, const WioObject& value) const
    {
        field(fieldName).set_object(value);
    }

    inline void WioComponent::set_component(std::string_view fieldName, const WioComponent& value) const
    {
        field(fieldName).set_component(value);
    }

    template <typename Signature>
    auto wio_load_export(const WioModuleApi* api, std::string_view logicalName) -> typename detail::FunctionTraits<Signature>::StdFunction;

    template <typename Signature>
    auto wio_load_command(const WioModuleApi* api, std::string_view commandName) -> typename detail::FunctionTraits<Signature>::StdFunction;

    template <typename Signature>
    auto wio_load_event_hook(const WioModuleApi* api, std::string_view hookName) -> typename detail::FunctionTraits<Signature>::StdFunction;

    template <typename Signature>
    auto wio_load_event(const WioModuleApi* api, std::string_view eventName) -> typename detail::FunctionTraits<Signature>::StdFunction;

    template <typename Signature>
    auto wio_load_function(const WioModuleApi* api, std::string_view name) -> typename detail::FunctionTraits<Signature>::StdFunction;

    [[nodiscard]] WioObjectType wio_load_object(const WioModuleApi* api, std::string_view logicalName);
    [[nodiscard]] WioComponentType wio_load_component(const WioModuleApi* api, std::string_view logicalName);

    class Module
    {
    public:
        Module() = default;

        Module(const Module&) = delete;
        Module& operator=(const Module&) = delete;

        Module(Module&& other) noexcept
        {
            *this = std::move(other);
        }

        Module& operator=(Module&& other) noexcept
        {
            if (this == &other)
                return *this;

            close();
            handle_ = other.handle_;
            api_ = other.api_;
            path_ = std::move(other.path_);
            started_ = other.started_;

            other.handle_ = nullptr;
            other.api_ = nullptr;
            other.started_ = false;
            return *this;
        }

        ~Module()
        {
            close();
        }

        [[nodiscard]] static Module open(const std::filesystem::path& libraryPath)
        {
            if (libraryPath.empty())
                throw Error(ErrorCode::InvalidArgument, "Wio SDK expected a non-empty library path.");

            std::filesystem::path absolutePath = std::filesystem::absolute(libraryPath).make_preferred();
            if (!std::filesystem::exists(absolutePath))
            {
                std::ostringstream message;
                message << "Wio SDK could not find module library '" << absolutePath.string() << "'.";
                throw Error(ErrorCode::LibraryOpenFailed, message.str());
            }

            Module module;
            module.path_ = absolutePath;
            module.handle_ = detail::openLibrary(absolutePath);
            if (module.handle_ == nullptr)
            {
                std::ostringstream message;
                message << "Wio SDK failed to open '" << absolutePath.string() << "': "
                        << detail::dynamicLibraryErrorMessage();
                throw Error(ErrorCode::LibraryOpenFailed, message.str());
            }

            WioModuleGetApiFn getApi = detail::loadSymbol<WioModuleGetApiFn>(module.handle_, "WioModuleGetApi");
            if (getApi == nullptr)
            {
                std::ostringstream message;
                message << "Wio SDK could not load WioModuleGetApi from '" << absolutePath.string() << "': "
                        << detail::dynamicLibraryErrorMessage();
                throw Error(ErrorCode::SymbolLookupFailed, message.str());
            }

            module.api_ = getApi();
            if (module.api_ == nullptr)
            {
                std::ostringstream message;
                message << "Wio SDK received a null module API pointer from '" << absolutePath.string() << "'.";
                throw Error(ErrorCode::ApiUnavailable, message.str());
            }

            if (module.api_->descriptorVersion != WIO_MODULE_API_DESCRIPTOR_VERSION)
            {
                std::ostringstream message;
                message << "Wio SDK descriptor version mismatch for '" << absolutePath.string() << "': expected "
                        << WIO_MODULE_API_DESCRIPTOR_VERSION << " but module reports "
                        << module.api_->descriptorVersion << '.';
                throw Error(ErrorCode::InvalidApiDescriptor, message.str());
            }

            return module;
        }

        [[nodiscard]] static Module load(const std::filesystem::path& libraryPath)
        {
            Module module = open(libraryPath);
            module.start();
            return module;
        }

        void start()
        {
            if (started_)
                return;

            if (api_ != nullptr && api_->load != nullptr)
            {
                const std::int32_t status = api_->load();
                if (status != 0)
                {
                    std::ostringstream message;
                    message << "Wio SDK module load failed for '" << path_.string()
                            << "' with status " << status << '.';
                    throw Error(ErrorCode::LifecycleFailed, message.str());
                }
            }

            started_ = true;
        }

        void unload() noexcept
        {
            if (!started_)
                return;

            if (api_ != nullptr && api_->unload != nullptr)
                api_->unload();

            started_ = false;
        }

        void close() noexcept
        {
            unload();
            detail::closeLibrary(handle_);
            handle_ = nullptr;
            api_ = nullptr;
            path_.clear();
        }

        [[nodiscard]] const WioModuleApi* raw_api() const noexcept
        {
            return api_;
        }

        [[nodiscard]] const WioModuleApi& api() const
        {
            if (api_ == nullptr)
                throw Error(ErrorCode::ApiUnavailable, "Wio SDK module API is not available.");
            return *api_;
        }

        [[nodiscard]] bool has_api() const noexcept
        {
            return api_ != nullptr;
        }

        [[nodiscard]] bool started() const noexcept
        {
            return started_;
        }

        [[nodiscard]] bool supports_save_state() const noexcept
        {
            return api_ != nullptr && api_->saveState != nullptr;
        }

        [[nodiscard]] bool supports_restore_state() const noexcept
        {
            return api_ != nullptr && api_->restoreState != nullptr;
        }

        void update(float deltaTime) const
        {
            if (api_ != nullptr && api_->update != nullptr)
                api_->update(deltaTime);
        }

        [[nodiscard]] std::int32_t save_state() const
        {
            if (api_ == nullptr || api_->saveState == nullptr)
                throw Error(ErrorCode::LifecycleFailed, "Wio SDK save_state() is not available for this module.");

            return api_->saveState();
        }

        void restore_state(std::int32_t snapshot) const
        {
            if (api_ == nullptr || api_->restoreState == nullptr)
                throw Error(ErrorCode::LifecycleFailed, "Wio SDK restore_state() is not available for this module.");

            const std::int32_t status = api_->restoreState(snapshot);
            if (status != 0)
            {
                std::ostringstream message;
                message << "Wio SDK restore_state() failed for '" << path_.string()
                        << "' with status " << status << '.';
                throw Error(ErrorCode::LifecycleFailed, message.str());
            }
        }

        template <typename Signature>
        auto load_export(std::string_view logicalName) const -> typename detail::FunctionTraits<Signature>::StdFunction
        {
            return wio::sdk::wio_load_export<Signature>(api_, logicalName);
        }

        template <typename Signature>
        auto load_command(std::string_view commandName) const -> typename detail::FunctionTraits<Signature>::StdFunction
        {
            return wio::sdk::wio_load_command<Signature>(api_, commandName);
        }

        template <typename Signature>
        auto load_event_hook(std::string_view hookName) const -> typename detail::FunctionTraits<Signature>::StdFunction
        {
            return wio::sdk::wio_load_event_hook<Signature>(api_, hookName);
        }

        template <typename Signature>
        auto load_event(std::string_view eventName) const -> typename detail::FunctionTraits<Signature>::StdFunction
        {
            return wio::sdk::wio_load_event<Signature>(api_, eventName);
        }

        template <typename Signature>
        auto load_function(std::string_view name) const -> typename detail::FunctionTraits<Signature>::StdFunction
        {
            return wio::sdk::wio_load_function<Signature>(api_, name);
        }

        [[nodiscard]] WioObjectType load_object(std::string_view logicalName) const
        {
            return wio::sdk::wio_load_object(api_, logicalName);
        }

        [[nodiscard]] WioComponentType load_component(std::string_view logicalName) const
        {
            return wio::sdk::wio_load_component(api_, logicalName);
        }

    private:
        detail::LibraryHandle handle_ = nullptr;
        const WioModuleApi* api_ = nullptr;
        std::filesystem::path path_;
        bool started_ = false;
    };

    struct HotReloadOptions
    {
        std::filesystem::path stagingDirectory{};
        bool preserveState = true;
        bool invokeUnload = true;
        bool autoReload = false;
    };

    class HotReloadModule
    {
    public:
        explicit HotReloadModule(std::filesystem::path libraryPath, HotReloadOptions options = {})
            : sourcePath_(std::filesystem::absolute(std::move(libraryPath)).make_preferred()),
              options_(std::move(options)),
              autoReloadEnabled_(options_.autoReload)
        {
            if (options_.stagingDirectory.empty())
                options_.stagingDirectory = sourcePath_.parent_path() / ".wio_hot_reload";
        }

        [[nodiscard]] static HotReloadModule load(const std::filesystem::path& libraryPath, HotReloadOptions options = {})
        {
            HotReloadModule module(libraryPath, std::move(options));
            module.start();
            return module;
        }

        void start()
        {
            if (module_.has_api())
                return;

            loadFresh(/*allowRestore=*/false);
        }

        void enable_auto_reload(bool enabled = true) noexcept
        {
            autoReloadEnabled_ = enabled;
        }

        [[nodiscard]] bool auto_reload_enabled() const noexcept
        {
            return autoReloadEnabled_;
        }

        void set_source_path(const std::filesystem::path& libraryPath)
        {
            sourcePath_ = std::filesystem::absolute(libraryPath).make_preferred();
        }

        [[nodiscard]] const std::filesystem::path& source_path() const noexcept
        {
            return sourcePath_;
        }

        [[nodiscard]] std::uint64_t generation() const noexcept
        {
            return generation_;
        }

        [[nodiscard]] const Module& current() const noexcept
        {
            return module_;
        }

        [[nodiscard]] const WioModuleApi* raw_api() const noexcept
        {
            return module_.raw_api();
        }

        bool reload_if_changed()
        {
            if (sourcePath_.empty() || !std::filesystem::exists(sourcePath_))
                return false;

            const auto writeTime = std::filesystem::last_write_time(sourcePath_);
            if (writeTime <= lastLoadedWriteTime_)
                return false;

            reload();
            return true;
        }

        void reload()
        {
            loadFresh(/*allowRestore=*/true);
        }

        void reload_from(const std::filesystem::path& libraryPath)
        {
            set_source_path(libraryPath);
            reload();
        }

        void update(float deltaTime)
        {
            maybeAutoReload();
            module_.update(deltaTime);
        }

        template <typename Signature>
        auto load_export(std::string_view logicalName) -> typename detail::FunctionTraits<Signature>::StdFunction
        {
            using StdFunction = typename detail::FunctionTraits<Signature>::StdFunction;
            std::function<StdFunction(std::string_view)> resolver =
                [this](std::string_view name)
                {
                    maybeAutoReload();
                    return module_.template load_export<Signature>(name);
                };
            return detail::bindReloadable<Signature>(std::move(resolver), std::string(logicalName));
        }

        template <typename Signature>
        auto load_command(std::string_view commandName) -> typename detail::FunctionTraits<Signature>::StdFunction
        {
            using StdFunction = typename detail::FunctionTraits<Signature>::StdFunction;
            std::function<StdFunction(std::string_view)> resolver =
                [this](std::string_view name)
                {
                    maybeAutoReload();
                    return module_.template load_command<Signature>(name);
                };
            return detail::bindReloadable<Signature>(std::move(resolver), std::string(commandName));
        }

        template <typename Signature>
        auto load_event_hook(std::string_view hookName) -> typename detail::FunctionTraits<Signature>::StdFunction
        {
            using StdFunction = typename detail::FunctionTraits<Signature>::StdFunction;
            std::function<StdFunction(std::string_view)> resolver =
                [this](std::string_view name)
                {
                    maybeAutoReload();
                    return module_.template load_event_hook<Signature>(name);
                };
            return detail::bindReloadable<Signature>(std::move(resolver), std::string(hookName));
        }

        template <typename Signature>
        auto load_event(std::string_view eventName) -> typename detail::FunctionTraits<Signature>::StdFunction
        {
            using StdFunction = typename detail::FunctionTraits<Signature>::StdFunction;
            std::function<StdFunction(std::string_view)> resolver =
                [this](std::string_view name)
                {
                    maybeAutoReload();
                    return module_.template load_event<Signature>(name);
                };
            return detail::bindReloadable<Signature>(std::move(resolver), std::string(eventName));
        }

        template <typename Signature>
        auto load_function(std::string_view name) -> typename detail::FunctionTraits<Signature>::StdFunction
        {
            using StdFunction = typename detail::FunctionTraits<Signature>::StdFunction;
            std::function<StdFunction(std::string_view)> resolver =
                [this](std::string_view logicalName)
                {
                    maybeAutoReload();
                    return module_.template load_function<Signature>(logicalName);
                };
            return detail::bindReloadable<Signature>(std::move(resolver), std::string(name));
        }

    private:
        void maybeAutoReload()
        {
            if (autoReloadEnabled_)
                reload_if_changed();
        }

        std::filesystem::path stageCopy()
        {
            if (!std::filesystem::exists(sourcePath_))
            {
                std::ostringstream message;
                message << "Wio SDK hot reload source library '" << sourcePath_.string() << "' does not exist.";
                throw Error(ErrorCode::ReloadFailed, message.str());
            }

            std::filesystem::create_directories(options_.stagingDirectory);
            ++generation_;

            std::ostringstream fileName;
            fileName << sourcePath_.stem().string() << ".hot." << generation_ << sourcePath_.extension().string();
            std::filesystem::path stagedPath = (options_.stagingDirectory / fileName.str()).make_preferred();

            std::error_code ec;
            std::filesystem::copy_file(sourcePath_, stagedPath, std::filesystem::copy_options::overwrite_existing, ec);
            if (ec)
            {
                std::ostringstream message;
                message << "Wio SDK failed to stage hot reload copy from '" << sourcePath_.string()
                        << "' to '" << stagedPath.string() << "': " << ec.message();
                throw Error(ErrorCode::IoFailure, message.str());
            }

            stagedArtifacts_.push_back(stagedPath);
            return stagedPath;
        }

        void pruneStagedArtifacts() noexcept
        {
            if (stagedArtifacts_.size() <= 1)
                return;

            for (size_t i = 0; i + 1 < stagedArtifacts_.size(); ++i)
            {
                std::error_code ec;
                std::filesystem::remove(stagedArtifacts_[i], ec);
            }

            stagedArtifacts_.erase(stagedArtifacts_.begin(), stagedArtifacts_.end() - 1);
        }

        void loadFresh(bool allowRestore)
        {
            std::optional<std::int32_t> snapshot;
            if (allowRestore && options_.preserveState && module_.supports_save_state())
                snapshot = module_.save_state();

            if (options_.invokeUnload)
                module_.unload();
            module_.close();

            std::filesystem::path stagedPath = stageCopy();
            Module nextModule = Module::open(stagedPath);

            if (snapshot.has_value() && nextModule.supports_restore_state())
            {
                nextModule.restore_state(*snapshot);
            }
            else
            {
                nextModule.start();
            }

            module_ = std::move(nextModule);
            lastLoadedWriteTime_ = std::filesystem::last_write_time(sourcePath_);
            pruneStagedArtifacts();
        }

        std::filesystem::path sourcePath_;
        HotReloadOptions options_;
        Module module_;
        std::filesystem::file_time_type lastLoadedWriteTime_{};
        bool autoReloadEnabled_ = false;
        std::uint64_t generation_ = 0;
        std::vector<std::filesystem::path> stagedArtifacts_;
    };

    template <typename Signature>
    auto wio_load_export(const WioModuleApi* api, std::string_view logicalName) -> typename detail::FunctionTraits<Signature>::StdFunction
    {
        if (api == nullptr)
            throw Error(ErrorCode::ApiUnavailable, "Wio SDK expected a valid module API pointer.");

        const std::string ownedName(logicalName);
        const WioModuleExport* exportEntry = WioFindModuleExport(api, ownedName.c_str());
        if (exportEntry == nullptr)
        {
            std::ostringstream message;
            message << "Wio SDK could not find export '" << ownedName << "'.";
            throw Error(ErrorCode::ExportNotFound, message.str());
        }

        return detail::bindExport<Signature>(exportEntry, ownedName, "export");
    }

    template <typename Signature>
    auto wio_load_command(const WioModuleApi* api, std::string_view commandName) -> typename detail::FunctionTraits<Signature>::StdFunction
    {
        if (api == nullptr)
            throw Error(ErrorCode::ApiUnavailable, "Wio SDK expected a valid module API pointer.");

        const std::string ownedName(commandName);
        const WioModuleCommand* commandEntry = WioFindModuleCommand(api, ownedName.c_str());
        if (commandEntry == nullptr || commandEntry->exportEntry == nullptr)
        {
            std::ostringstream message;
            message << "Wio SDK could not find command '" << ownedName << "'.";
            throw Error(ErrorCode::CommandNotFound, message.str());
        }

        return detail::bindExport<Signature>(commandEntry->exportEntry, ownedName, "command");
    }

    template <typename Signature>
    auto wio_load_event_hook(const WioModuleApi* api, std::string_view hookName) -> typename detail::FunctionTraits<Signature>::StdFunction
    {
        if (api == nullptr)
            throw Error(ErrorCode::ApiUnavailable, "Wio SDK expected a valid module API pointer.");

        const std::string ownedName(hookName);
        const WioModuleEventHook* hookEntry = WioFindModuleEventHook(api, ownedName.c_str());
        if (hookEntry == nullptr || hookEntry->exportEntry == nullptr)
        {
            std::ostringstream message;
            message << "Wio SDK could not find event hook '" << ownedName << "'.";
            throw Error(ErrorCode::EventHookNotFound, message.str());
        }

        return detail::bindExport<Signature>(hookEntry->exportEntry, ownedName, "event hook");
    }

    template <typename Signature>
    auto wio_load_event(const WioModuleApi* api, std::string_view eventName) -> typename detail::FunctionTraits<Signature>::StdFunction
    {
        using Traits = detail::FunctionTraits<Signature>;
        if constexpr (!std::is_void_v<typename Traits::ReturnType>)
        {
            throw Error(ErrorCode::SignatureMismatch, "Wio SDK event broadcasters currently require a void return type.");
        }

        if (api == nullptr)
            throw Error(ErrorCode::ApiUnavailable, "Wio SDK expected a valid module API pointer.");

        const std::string ownedName(eventName);
        bool foundAny = false;
        if (api->eventHooks != nullptr)
        {
            for (std::uint32_t i = 0; i < api->eventHookCount; ++i)
            {
                const WioModuleEventHook& hook = api->eventHooks[i];
                if (hook.eventName == nullptr || std::strcmp(hook.eventName, ownedName.c_str()) != 0)
                    continue;

                foundAny = true;
                detail::validateExportSignature<Signature>(hook.exportEntry, ownedName, "event hook");
            }
        }

        if (!foundAny)
        {
            std::ostringstream message;
            message << "Wio SDK could not find any event hooks for event '" << ownedName << "'.";
            throw Error(ErrorCode::EventNotFound, message.str());
        }

        return typename Traits::StdFunction(
            [api, ownedName](auto... args) -> void
            {
                std::array<WioValue, sizeof...(args)> wioArgs{ detail::toWioValue(args)... };
                const std::int32_t status = WioBroadcastModuleEvent(
                    api,
                    ownedName.c_str(),
                    sizeof...(args) > 0 ? wioArgs.data() : nullptr,
                    static_cast<std::uint32_t>(sizeof...(args))
                );

                if (status != WIO_INVOKE_OK)
                {
                    std::ostringstream message;
                    message << "Wio SDK failed to broadcast event '" << ownedName
                            << "': " << detail::invokeStatusName(status) << '.';
                    throw Error(ErrorCode::InvokeFailed, message.str());
                }
            }
        );
    }

    template <typename Signature>
    auto wio_load_function(const WioModuleApi* api, std::string_view name) -> typename detail::FunctionTraits<Signature>::StdFunction
    {
        if (api == nullptr)
            throw Error(ErrorCode::ApiUnavailable, "Wio SDK expected a valid module API pointer.");

        const std::string ownedName(name);

        if (const WioModuleExport* exportEntry = WioFindModuleExport(api, ownedName.c_str()); exportEntry != nullptr)
            return detail::bindExport<Signature>(exportEntry, ownedName, "export");

        if (const WioModuleCommand* commandEntry = WioFindModuleCommand(api, ownedName.c_str());
            commandEntry != nullptr && commandEntry->exportEntry != nullptr)
        {
            return detail::bindExport<Signature>(commandEntry->exportEntry, ownedName, "command");
        }

        if (const WioModuleEventHook* hookEntry = WioFindModuleEventHook(api, ownedName.c_str());
            hookEntry != nullptr && hookEntry->exportEntry != nullptr)
        {
            return detail::bindExport<Signature>(hookEntry->exportEntry, ownedName, "event hook");
        }

        std::ostringstream message;
        message << "Wio SDK could not resolve function '" << ownedName
                << "' as an export, command, or event hook.";
        throw Error(ErrorCode::ExportNotFound, message.str());
    }

    inline WioObjectType wio_load_object(const WioModuleApi* api, std::string_view logicalName)
    {
        return WioObjectType(api, detail::requireModuleType(api, logicalName, WIO_MODULE_TYPE_OBJECT));
    }

    inline WioComponentType wio_load_component(const WioModuleApi* api, std::string_view logicalName)
    {
        return WioComponentType(api, detail::requireModuleType(api, logicalName, WIO_MODULE_TYPE_COMPONENT));
    }

}

template <typename Signature>
auto wio_load_export(const WioModuleApi* api, std::string_view logicalName) -> typename wio::sdk::detail::FunctionTraits<Signature>::StdFunction
{
    return wio::sdk::wio_load_export<Signature>(api, logicalName);
}

template <typename Signature>
auto wio_load_export(const wio::sdk::Module& module, std::string_view logicalName) -> typename wio::sdk::detail::FunctionTraits<Signature>::StdFunction
{
    return module.template load_export<Signature>(logicalName);
}

template <typename Signature>
auto wio_load_export(wio::sdk::HotReloadModule& module, std::string_view logicalName) -> typename wio::sdk::detail::FunctionTraits<Signature>::StdFunction
{
    return module.template load_export<Signature>(logicalName);
}

inline wio::sdk::WioObjectType wio_load_object(const WioModuleApi* api, std::string_view logicalName)
{
    return wio::sdk::wio_load_object(api, logicalName);
}

inline wio::sdk::WioObjectType wio_load_object(const wio::sdk::Module& module, std::string_view logicalName)
{
    return module.load_object(logicalName);
}

inline wio::sdk::WioComponentType wio_load_component(const WioModuleApi* api, std::string_view logicalName)
{
    return wio::sdk::wio_load_component(api, logicalName);
}

inline wio::sdk::WioComponentType wio_load_component(const wio::sdk::Module& module, std::string_view logicalName)
{
    return module.load_component(logicalName);
}

template <typename Signature>
auto wio_load_command(const WioModuleApi* api, std::string_view commandName) -> typename wio::sdk::detail::FunctionTraits<Signature>::StdFunction
{
    return wio::sdk::wio_load_command<Signature>(api, commandName);
}

template <typename Signature>
auto wio_load_command(const wio::sdk::Module& module, std::string_view commandName) -> typename wio::sdk::detail::FunctionTraits<Signature>::StdFunction
{
    return module.template load_command<Signature>(commandName);
}

template <typename Signature>
auto wio_load_command(wio::sdk::HotReloadModule& module, std::string_view commandName) -> typename wio::sdk::detail::FunctionTraits<Signature>::StdFunction
{
    return module.template load_command<Signature>(commandName);
}

template <typename Signature>
auto wio_load_event_hook(const WioModuleApi* api, std::string_view hookName) -> typename wio::sdk::detail::FunctionTraits<Signature>::StdFunction
{
    return wio::sdk::wio_load_event_hook<Signature>(api, hookName);
}

template <typename Signature>
auto wio_load_event_hook(const wio::sdk::Module& module, std::string_view hookName) -> typename wio::sdk::detail::FunctionTraits<Signature>::StdFunction
{
    return module.template load_event_hook<Signature>(hookName);
}

template <typename Signature>
auto wio_load_event_hook(wio::sdk::HotReloadModule& module, std::string_view hookName) -> typename wio::sdk::detail::FunctionTraits<Signature>::StdFunction
{
    return module.template load_event_hook<Signature>(hookName);
}

template <typename Signature>
auto wio_load_event(const WioModuleApi* api, std::string_view eventName) -> typename wio::sdk::detail::FunctionTraits<Signature>::StdFunction
{
    return wio::sdk::wio_load_event<Signature>(api, eventName);
}

template <typename Signature>
auto wio_load_event(const wio::sdk::Module& module, std::string_view eventName) -> typename wio::sdk::detail::FunctionTraits<Signature>::StdFunction
{
    return module.template load_event<Signature>(eventName);
}

template <typename Signature>
auto wio_load_event(wio::sdk::HotReloadModule& module, std::string_view eventName) -> typename wio::sdk::detail::FunctionTraits<Signature>::StdFunction
{
    return module.template load_event<Signature>(eventName);
}

template <typename Signature>
auto wio_load_function(const WioModuleApi* api, std::string_view name) -> typename wio::sdk::detail::FunctionTraits<Signature>::StdFunction
{
    return wio::sdk::wio_load_function<Signature>(api, name);
}

template <typename Signature>
auto wio_load_function(const wio::sdk::Module& module, std::string_view name) -> typename wio::sdk::detail::FunctionTraits<Signature>::StdFunction
{
    return module.template load_function<Signature>(name);
}

template <typename Signature>
auto wio_load_function(wio::sdk::HotReloadModule& module, std::string_view name) -> typename wio::sdk::detail::FunctionTraits<Signature>::StdFunction
{
    return module.template load_function<Signature>(name);
}
