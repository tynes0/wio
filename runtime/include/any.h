#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>
#include <typeinfo>
#include <utility>

#include <exception.h>
#include <ref.h>

namespace wio::runtime
{
    enum class AnyStorageKind : std::uint8_t
    {
        Null = 0,
        BoxedValue,
        ObjectReference,
        ManagedContainer,
        OpaquePayload
    };

    template <typename T>
    inline const void* GetAnyTypeToken() noexcept
    {
        static const int token = 0;
        return &token;
    }

    template <typename T>
    struct IsRuntimeRefType : std::false_type
    {
    };

    template <typename TObject>
    struct IsRuntimeRefType<Ref<TObject>> : std::true_type
    {
    };

    template <typename T>
    inline constexpr bool IsRuntimeRefTypeV = IsRuntimeRefType<std::remove_cvref_t<T>>::value;

    class AnyCellBase : public RefCountedObject
    {
    public:
        ~AnyCellBase() override = default;

        [[nodiscard]] virtual AnyStorageKind Kind() const noexcept = 0;
        [[nodiscard]] virtual const void* TypeToken() const noexcept = 0;
        [[nodiscard]] virtual std::string_view DebugTypeName() const noexcept = 0;
        [[nodiscard]] virtual Ref<RefCountedObject> RuntimeObject() const noexcept
        {
            return {};
        }
    };

    template <typename T>
    class AnyValueCell final : public AnyCellBase
    {
    public:
        using StoredType = std::remove_cvref_t<T>;

        explicit AnyValueCell(StoredType value)
            : value_(std::move(value))
        {
        }

        [[nodiscard]] AnyStorageKind Kind() const noexcept override
        {
            return AnyStorageKind::BoxedValue;
        }

        [[nodiscard]] const void* TypeToken() const noexcept override
        {
            return GetAnyTypeToken<StoredType>();
        }

        [[nodiscard]] std::string_view DebugTypeName() const noexcept override
        {
            return typeid(StoredType).name();
        }

        [[nodiscard]] StoredType& Value() noexcept
        {
            return value_;
        }

        [[nodiscard]] const StoredType& Value() const noexcept
        {
            return value_;
        }

    private:
        StoredType value_;
    };

    template <typename TObject>
    requires std::is_base_of_v<RefCountedObject, TObject>
    class AnyObjectCell final : public AnyCellBase
    {
    public:
        explicit AnyObjectCell(Ref<TObject> value)
            : value_(std::move(value))
        {
        }

        [[nodiscard]] AnyStorageKind Kind() const noexcept override
        {
            return AnyStorageKind::ObjectReference;
        }

        [[nodiscard]] const void* TypeToken() const noexcept override
        {
            return GetAnyTypeToken<TObject>();
        }

        [[nodiscard]] std::string_view DebugTypeName() const noexcept override
        {
            return typeid(TObject).name();
        }

        [[nodiscard]] const Ref<TObject>& Value() const noexcept
        {
            return value_;
        }

        [[nodiscard]] Ref<RefCountedObject> RuntimeObject() const noexcept override
        {
            return value_;
        }

    private:
        Ref<TObject> value_;
    };

    class AnyOpaqueCell final : public AnyCellBase
    {
    public:
        explicit AnyOpaqueCell(void* value) noexcept
            : value_(value)
        {
        }

        [[nodiscard]] AnyStorageKind Kind() const noexcept override
        {
            return AnyStorageKind::OpaquePayload;
        }

        [[nodiscard]] const void* TypeToken() const noexcept override
        {
            return GetAnyTypeToken<void*>();
        }

        [[nodiscard]] std::string_view DebugTypeName() const noexcept override
        {
            return "opaque";
        }

        [[nodiscard]] void* Value() const noexcept
        {
            return value_;
        }

    private:
        void* value_ = nullptr;
    };

    class AnyRuntimeObjectCell final : public AnyCellBase
    {
    public:
        explicit AnyRuntimeObjectCell(Ref<RefCountedObject> value)
            : value_(std::move(value))
        {
        }

        [[nodiscard]] AnyStorageKind Kind() const noexcept override
        {
            return AnyStorageKind::ObjectReference;
        }

        [[nodiscard]] const void* TypeToken() const noexcept override
        {
            return GetAnyTypeToken<RefCountedObject>();
        }

        [[nodiscard]] std::string_view DebugTypeName() const noexcept override
        {
            return "wio::runtime::RefCountedObject";
        }

        [[nodiscard]] Ref<RefCountedObject> RuntimeObject() const noexcept override
        {
            return value_;
        }

    private:
        Ref<RefCountedObject> value_;
    };

    class Any
    {
    public:
        [[nodiscard]] static Any FromCell(Ref<AnyCellBase> cell)
        {
            Any any;
            any.cell_ = std::move(cell);
            return any;
        }

        Any() = default;
        Any(std::nullptr_t) noexcept
            : cell_(nullptr)
        {
        }

        Any(RefCountedObject* runtimeObject)
        {
            if (runtimeObject)
                cell_ = Ref<AnyRuntimeObjectCell>::Create(Ref<RefCountedObject>(runtimeObject));
        }

        Any(void* opaqueValue) noexcept
        {
            if (opaqueValue)
                cell_ = Ref<AnyOpaqueCell>::Create(opaqueValue);
        }

        Any(const char* text)
            : cell_(Ref<AnyValueCell<std::string>>::Create(text ? std::string(text) : std::string{}))
        {
        }

        template <std::size_t N>
        Any(const char (&text)[N])
            : Any(static_cast<const char*>(text))
        {
        }

        Any(std::string text)
            : cell_(Ref<AnyValueCell<std::string>>::Create(std::move(text)))
        {
        }

        Any(std::string_view text)
            : cell_(Ref<AnyValueCell<std::string>>::Create(std::string(text)))
        {
        }

        Any(const Any&) = default;
        Any(Any&&) noexcept = default;
        Any& operator=(const Any&) = default;
        Any& operator=(Any&&) noexcept = default;

        template <typename T>
        requires (!std::is_same_v<std::remove_cvref_t<T>, Any> &&
                  !std::is_same_v<std::remove_cvref_t<T>, std::nullptr_t> &&
                  !IsRuntimeRefTypeV<T>)
        Any(T&& value)
            : cell_(Ref<AnyValueCell<std::remove_cvref_t<T>>>::Create(std::forward<T>(value)))
        {
        }

        template <typename TObject>
        requires (std::is_base_of_v<RefCountedObject, TObject> &&
                  !std::is_base_of_v<AnyCellBase, TObject>)
        Any(const Ref<TObject>& object)
        {
            if (object)
                cell_ = Ref<AnyObjectCell<TObject>>::Create(object);
        }

        template <typename TObject>
        requires (std::is_base_of_v<RefCountedObject, TObject> &&
                  !std::is_base_of_v<AnyCellBase, TObject>)
        Any(Ref<TObject>&& object)
        {
            if (object)
                cell_ = Ref<AnyObjectCell<TObject>>::Create(std::move(object));
        }

        template <typename T>
        [[nodiscard]] static Any Box(T&& value)
        {
            using StoredType = std::remove_cvref_t<T>;
            return FromCell(Ref<AnyValueCell<StoredType>>::Create(std::forward<T>(value)));
        }

        template <typename TObject>
        requires (std::is_base_of_v<RefCountedObject, TObject> &&
                  !std::is_base_of_v<AnyCellBase, TObject>)
        [[nodiscard]] static Any FromObject(const Ref<TObject>& object)
        {
            if (!object)
                return Any(nullptr);

            return FromCell(Ref<AnyObjectCell<TObject>>::Create(object));
        }

        [[nodiscard]] static Any FromRuntimeObject(const Ref<RefCountedObject>& object)
        {
            if (!object)
                return Any(nullptr);

            return FromCell(Ref<AnyRuntimeObjectCell>::Create(object));
        }

        [[nodiscard]] static Any FromRuntimeObject(RefCountedObject* object)
        {
            if (!object)
                return Any(nullptr);

            return FromRuntimeObject(Ref<RefCountedObject>(object));
        }

        [[nodiscard]] static Any FromOpaque(void* value) noexcept
        {
            return Any(value);
        }

        template <typename TInterface>
        [[nodiscard]] static Any FromInterface(TInterface* interfaceValue)
        {
            if (!interfaceValue)
                return Any(nullptr);

            RefCountedObject* runtimeObject = interfaceValue->_WF_RuntimeObject();
            if (!runtimeObject)
                throw RuntimeException("Any: interface payload did not provide a runtime object.");

            return FromRuntimeObject(runtimeObject);
        }

        template <typename T>
        requires (!std::is_same_v<std::remove_cvref_t<T>, Any> &&
                  !std::is_same_v<std::remove_cvref_t<T>, std::nullptr_t> &&
                  !IsRuntimeRefTypeV<T>)
        Any& operator=(T&& value)
        {
            using StoredType = std::remove_cvref_t<T>;
            cell_ = Ref<AnyValueCell<StoredType>>::Create(std::forward<T>(value));
            return *this;
        }

        template <typename TObject>
        requires (std::is_base_of_v<RefCountedObject, TObject> &&
                  !std::is_base_of_v<AnyCellBase, TObject>)
        Any& operator=(const Ref<TObject>& object)
        {
            if (!object)
            {
                cell_ = nullptr;
                return *this;
            }

            cell_ = Ref<AnyObjectCell<TObject>>::Create(object);
            return *this;
        }

        template <typename TObject>
        requires (std::is_base_of_v<RefCountedObject, TObject> &&
                  !std::is_base_of_v<AnyCellBase, TObject>)
        Any& operator=(Ref<TObject>&& object)
        {
            if (!object)
            {
                cell_ = nullptr;
                return *this;
            }

            cell_ = Ref<AnyObjectCell<TObject>>::Create(std::move(object));
            return *this;
        }

        Any& operator=(std::nullptr_t) noexcept
        {
            cell_ = nullptr;
            return *this;
        }

        Any& operator=(RefCountedObject* runtimeObject)
        {
            if (!runtimeObject)
            {
                cell_ = nullptr;
                return *this;
            }

            cell_ = Ref<AnyRuntimeObjectCell>::Create(Ref<RefCountedObject>(runtimeObject));
            return *this;
        }

        Any& operator=(void* opaqueValue) noexcept
        {
            if (!opaqueValue)
            {
                cell_ = nullptr;
                return *this;
            }

            cell_ = Ref<AnyOpaqueCell>::Create(opaqueValue);
            return *this;
        }

        Any& operator=(const char* text)
        {
            cell_ = Ref<AnyValueCell<std::string>>::Create(text ? std::string(text) : std::string{});
            return *this;
        }

        template <std::size_t N>
        Any& operator=(const char (&text)[N])
        {
            return operator=(static_cast<const char*>(text));
        }

        Any& operator=(std::string text)
        {
            cell_ = Ref<AnyValueCell<std::string>>::Create(std::move(text));
            return *this;
        }

        Any& operator=(std::string_view text)
        {
            cell_ = Ref<AnyValueCell<std::string>>::Create(std::string(text));
            return *this;
        }

        [[nodiscard]] bool IsNull() const noexcept
        {
            return !cell_;
        }

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return !IsNull();
        }

        [[nodiscard]] AnyStorageKind Kind() const noexcept
        {
            return cell_ ? cell_->Kind() : AnyStorageKind::Null;
        }

        [[nodiscard]] const AnyCellBase* Cell() const noexcept
        {
            return cell_.Get();
        }

        [[nodiscard]] std::string_view DebugTypeName() const noexcept
        {
            return cell_ ? cell_->DebugTypeName() : std::string_view{"null"};
        }

        [[nodiscard]] bool operator==(std::nullptr_t) const noexcept
        {
            return IsNull();
        }

        [[nodiscard]] bool operator!=(std::nullptr_t) const noexcept
        {
            return !IsNull();
        }

        friend bool operator==(std::nullptr_t, const Any& value) noexcept
        {
            return value.IsNull();
        }

        friend bool operator!=(std::nullptr_t, const Any& value) noexcept
        {
            return !value.IsNull();
        }

        template <typename T>
        [[nodiscard]] bool IsBoxed() const noexcept
        {
            using StoredType = std::remove_cvref_t<T>;
            return cell_ &&
                   cell_->Kind() == AnyStorageKind::BoxedValue &&
                   cell_->TypeToken() == GetAnyTypeToken<StoredType>();
        }

        [[nodiscard]] bool IsOpaque() const noexcept
        {
            return cell_ && cell_->Kind() == AnyStorageKind::OpaquePayload;
        }

        template <typename TObject>
        requires std::is_base_of_v<RefCountedObject, TObject>
        [[nodiscard]] bool IsObject() const noexcept
        {
            return cell_ &&
                   cell_->Kind() == AnyStorageKind::ObjectReference &&
                   cell_->TypeToken() == GetAnyTypeToken<TObject>();
        }

        template <typename TObject>
        requires std::is_base_of_v<RefCountedObject, TObject>
        [[nodiscard]] bool CanCastObject() const noexcept
        {
            const auto runtimeObject = cell_ ? cell_->RuntimeObject() : Ref<RefCountedObject>{};
            return runtimeObject && runtimeObject->_WF_CastTo(TObject::TYPE_ID) != nullptr;
        }

        template <typename TInterface>
        [[nodiscard]] bool IsInterface() const noexcept
        {
            const auto runtimeObject = cell_ ? cell_->RuntimeObject() : Ref<RefCountedObject>{};
            return runtimeObject && runtimeObject->_WF_CastTo(TInterface::TYPE_ID) != nullptr;
        }

        template <typename T>
        [[nodiscard]] T& AsBoxed()
        {
            using StoredType = std::remove_cvref_t<T>;
            if (!IsBoxed<StoredType>())
                throw RuntimeException("Any: boxed value type mismatch.");

            return static_cast<AnyValueCell<StoredType>*>(cell_.Get())->Value();
        }

        template <typename T>
        [[nodiscard]] const T& AsBoxed() const
        {
            using StoredType = std::remove_cvref_t<T>;
            if (!IsBoxed<StoredType>())
                throw RuntimeException("Any: boxed value type mismatch.");

            return static_cast<const AnyValueCell<StoredType>*>(cell_.Get())->Value();
        }

        [[nodiscard]] void* AsOpaque() const
        {
            if (!IsOpaque())
                throw RuntimeException("Any: opaque payload type mismatch.");

            return static_cast<const AnyOpaqueCell*>(cell_.Get())->Value();
        }

        template <typename TObject>
        requires std::is_base_of_v<RefCountedObject, TObject>
        [[nodiscard]] Ref<TObject> AsObject() const
        {
            if (!IsObject<TObject>())
                throw RuntimeException("Any: object reference type mismatch.");

            return static_cast<const AnyObjectCell<TObject>*>(cell_.Get())->Value();
        }

        template <typename TObject>
        requires std::is_base_of_v<RefCountedObject, TObject>
        [[nodiscard]] Ref<TObject> CastObject() const
        {
            const auto runtimeObject = cell_ ? cell_->RuntimeObject() : Ref<RefCountedObject>{};
            if (!runtimeObject)
                throw RuntimeException("Any: object reference cast requires an object payload.");

            void* casted = runtimeObject->_WF_CastTo(TObject::TYPE_ID);
            if (!casted)
                throw RuntimeException("Any: object reference cast target mismatch.");

            return Ref<TObject>(static_cast<TObject*>(casted));
        }

        template <typename TInterface>
        [[nodiscard]] TInterface* AsInterface() const
        {
            const auto runtimeObject = cell_ ? cell_->RuntimeObject() : Ref<RefCountedObject>{};
            if (!runtimeObject)
                throw RuntimeException("Any: interface cast requires an object payload.");

            void* casted = runtimeObject->_WF_CastTo(TInterface::TYPE_ID);
            if (!casted)
                throw RuntimeException("Any: interface cast target mismatch.");

            return static_cast<TInterface*>(casted);
        }

    private:
        explicit Any(Ref<AnyCellBase> cell)
            : cell_(std::move(cell))
        {
        }

        Ref<AnyCellBase> cell_;
    };
}
