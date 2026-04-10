#pragma once

#include <memory>
#include <atomic>
#include <type_traits>
#include <utility>
#include <cstdint>

namespace wio::runtime
{
    // ============================================================
    // RefCountedObject (Intrusive Base)
    // ============================================================
    
    /**
     * @brief Base class for objects that require intrusive reference counting.
     * * Any class that needs to be managed by Ref<T> or WeakRef<T> must publicly 
     * inherit from this class. It provides thread-safe atomic counters for both 
     * strong and weak references.
     */
    class RefCountedObject
    {
    public:
        /** @brief Default constructor initializing reference counters. */
        RefCountedObject() noexcept = default;
    
    protected:
        /**
         * @brief Copy constructor. 
         * It ignores the reference counts of the original object. The new object starts with fresh references.
         */
        RefCountedObject(const RefCountedObject&) noexcept 
            : m_Strong(0), m_Weak(1) 
        {
        }
    
        /**
         * @brief Move constructor. 
         * It ignores the reference numbers of the original object.
         */
        RefCountedObject(RefCountedObject&&) noexcept 
            : m_Strong(0), m_Weak(1) 
        {
        }
    
        /**
         * @brief Copy assignment. 
         * It prevents reference numbers from being written over. It does nothing else.
         */
        RefCountedObject& operator=(const RefCountedObject&) noexcept 
        { 
            return *this; 
        }
    
        /**
         * @brief Move assignment. 
         * It prevents reference numbers from being written over. It does nothing else.
         */
        RefCountedObject& operator=(RefCountedObject&&) noexcept 
        { 
            return *this; 
        }
    
        /** @brief Virtual destructor to ensure proper cleanup of derived types. */
        virtual ~RefCountedObject() = default;
    public:
        /**
        * @brief Retrieves the current strong reference count.
        * @return The number of active strong references.
        */
        [[nodiscard]] uint32_t StrongCount() const noexcept
        {
            return m_Strong.load(std::memory_order_acquire);
        }
    
        /**
         * @brief Retrieves the current weak reference count.
         * @return The number of active weak references (including the internal +1 hold from strong refs).
         */
        [[nodiscard]] uint32_t WeakCount() const
        {
            return m_Weak.load();
        }
    
        /**
         * @brief Virtual callback invoked exactly once when the strong reference count reaches zero.
         * Derived classes can override this to release resources or break cyclic dependencies 
         * before the physical memory is eventually deallocated.
         */
        virtual void OnZeroStrong() noexcept {}

        // NOLINTNEXTLINE(clang-diagnostic-reserved-identifier, bugprone-reserved-identifier)
        virtual bool _WF_IsA(uint64_t id) const { return false; }
        // NOLINTNEXTLINE(clang-diagnostic-reserved-identifier, bugprone-reserved-identifier)
        virtual void* _WF_CastTo(uint64_t id) { return nullptr; }
    private:
        /**
         * @brief Atomically increments the strong reference count.
         * Uses relaxed memory ordering as it only guarantees counter increment.
         */
        void IncStrong() const noexcept
        {
            m_Strong.fetch_add(1, std::memory_order_relaxed);
        }
    
        /**
         * @brief Atomically decrements the strong reference count.
         * @return The new strong reference count after decrementing.
         * Uses acquire-release semantics to ensure proper synchronization before destruction.
         */
        [[nodiscard]] uint32_t DecStrong() const noexcept
        {
            return m_Strong.fetch_sub(1, std::memory_order_acq_rel) - 1;
        }
    
        /**
         * @brief Atomically increments the weak reference count.
         * Uses relaxed memory ordering.
         */
        void IncWeak() const noexcept
        {
            m_Weak.fetch_add(1, std::memory_order_relaxed);
        }
    
        /**
         * @brief Atomically decrements the weak reference count.
         * @return The new weak reference count after decrementing.
         * Uses acquire-release semantics.
         */
        [[nodiscard]] uint32_t DecWeak() const noexcept
        {
            return m_Weak.fetch_sub(1, std::memory_order_acq_rel) - 1;
        }
        
        /**
         * @brief Attempts to increment the strong count only if it is greater than zero.
         * @return True if the increment was successful, false if the count is already zero.
         * Crucial for safely locking a WeakRef into a strong Ref.
         */
        [[nodiscard]] bool TryIncStrong() const noexcept
        {
            uint32_t count = m_Strong.load(std::memory_order_acquire);
    
            while (count != 0)
            {
                if (m_Strong.compare_exchange_weak(
                        count,
                        count + 1,
                        std::memory_order_acq_rel,
                        std::memory_order_acquire))
                {
                    return true;
                }
            }
    
            return false;
        }
    
        void DestroyInternal() const noexcept
        {
            delete this;
        }
    
        template <typename T>
        friend class Ref;
    
        template <typename T>
        friend class WeakRef;
    
        template <typename T>
        friend struct RefDeleter;
    
        /** @brief Number of active strong references. */
        mutable std::atomic<uint32_t> m_Strong{0};
        /** @brief Number of active weak references + 1 (representing the strong refs' hold on memory). */
        mutable std::atomic<uint32_t> m_Weak{1};
    };
    
    template<typename T>
    struct RefDeleter
    {
        static void Execute(T* ptr)
        {
            static_assert(sizeof(T) != 0, "T is an incomplete type! Include the header where this Ref is destroyed.");
    
            RefCountedObject* base = ptr;
    
            if (base->DecStrong() == 0)
            {
                base->OnZeroStrong();
    
                if (base->DecWeak() == 0)
                {
                    base->DestroyInternal();
                }
            }
        }
    
        static void ExecuteWeak(T* ptr)
        {
            if (!ptr)
                return;
    
            static_assert(sizeof(T) != 0, "T is an incomplete type! Include the header where this WeakRef is destroyed.");
            RefCountedObject* base = ptr;
    
            if (base->DecWeak() == 0)
            {
                base->DestroyInternal();
            }
        }
    };
    
    namespace DetailRef
    {
        // ============================================================
        // Adopt Tag (internal)
        // ============================================================
    
        /**
         * @brief Internal tag structure used to adopt an existing pointer 
         * without incrementing its strong reference count.
         */
        struct AdoptTag {};
    }
    
    // ============================================================
    // Ref (Strong Reference)
    // ============================================================
    
    /**
     * @brief Intrusive strong smart pointer for managing RefCountedObject lifetimes.
     * @tparam T The type of the managed object. Must derive from RefCountedObject.
     */
    template <typename T>
    class Ref
    {
    public:
        /** @brief The managed element type. */
        using element_type = T;
    
        // --------------------------------------------------------
    
        /** @brief Default constructor, initializes to nullptr. */
        constexpr Ref() noexcept = default;
        
        /** @brief Nullptr constructor. */
        constexpr Ref(std::nullptr_t) noexcept {}
    
        /**
         * @brief Constructs a strong reference from a raw pointer, incrementing its count.
         * @param ptr The raw pointer to manage.
         */
        explicit Ref(T* ptr) noexcept
            : m_Ptr(ptr)
        {
            // static_assert(std::is_base_of_v<RefCountedObject, T>, "T must derive from RefCountedObject");
            IncRef();
        }
    private:
    
        /**
         * @brief Internal constructor that adopts a raw pointer without incrementing the count.
         * @param ptr The raw pointer to adopt.
         */
        Ref(T* ptr, DetailRef::AdoptTag) noexcept
            : m_Ptr(ptr)
        {
        }
    public:
    
        /**
         * @brief Copy constructor. Increments the strong reference count.
         * @param other The Ref to copy from.
         */
        Ref(const Ref& other) noexcept
            : m_Ptr(other.m_Ptr)
        {
            IncRef();
        }
    
        /**
         * @brief Move constructor. Transfers ownership without altering the count.
         * @param other The Ref to move from.
         */
        Ref(Ref&& other) noexcept
            : m_Ptr(other.m_Ptr)
        {
            other.m_Ptr = nullptr;
        }
    
        /**
         * @brief Templated copy constructor for polymorphic assignment.
         * @tparam U The derived type of the other Ref.
         * @param other The derived Ref to copy from.
         */
        template <typename U>
        requires std::is_convertible_v<U*, T*>
        Ref(const Ref<U>& other) noexcept
            : m_Ptr(other.Get())
        {
            IncRef();
        }
    
        /**
         * @brief Templated move constructor for polymorphic assignment.
         * @tparam U The derived type of the other Ref.
         * @param other The derived Ref to move from.
         */
        template <typename U>
        requires std::is_convertible_v<U*, T*>
        // NOLINTNEXTLINE(cppcoreguidelines-rvalue-reference-param-not-moved)
        Ref(Ref<U>&& other) noexcept
            : m_Ptr(other.m_Ptr)
        {
            other.m_Ptr = nullptr;
        }
    
        // --------------------------------------------------------
    
        /**
         * @brief Copy assignment operator. Handles self-assignment and updates counts safely.
         * @param other The Ref to assign from.
         * @return Reference to this object.
         */
        Ref& operator=(const Ref& other) noexcept
        {
            if (this == &other)
                return *this;
    
            T* newPtr = other.m_Ptr;
            if (newPtr)
                static_cast<RefCountedObject*>(newPtr)->IncStrong();
    
            Release();
            m_Ptr = newPtr;
            return *this;
        }
    
        /**
         * @brief Move assignment operator.
         * @param other The Ref to move from.
         * @return Reference to this object.
         */
        Ref& operator=(Ref&& other) noexcept
        {
            if (this == &other)
                return *this;
    
            Release();
            m_Ptr = other.m_Ptr;
            other.m_Ptr = nullptr;
            return *this;
        }
    
        /**
         * @brief Templated copy assignment operator for polymorphic types.
         * @tparam U The derived type.
         * @param other The Ref to assign from.
         * @return Reference to this object.
         */
        template<typename U>
        requires std::is_convertible_v<U*, T*>
        Ref& operator=(const Ref<U>& other) noexcept
        {
            T* newPtr = other.Get();
    
            if (m_Ptr == newPtr)
                return *this;
    
            if (newPtr)
                static_cast<RefCountedObject*>(newPtr)->IncStrong();
    
            Release();
            m_Ptr = newPtr;
            return *this;
        }
    
        /**
         * @brief Templated move assignment operator for polymorphic types.
         * @tparam U The derived type.
         * @param other The Ref to move from.
         * @return Reference to this object.
         */
        template<typename U>
        requires std::is_convertible_v<U*, T*>
        // NOLINTNEXTLINE(cppcoreguidelines-rvalue-reference-param-not-moved)
        Ref& operator=(Ref<U>&& other) noexcept
        {
            T* newPtr = other.m_Ptr;
    
            if (m_Ptr == newPtr)
                return *this;
    
            Release();
            m_Ptr = newPtr;
            other.m_Ptr = nullptr;
            return *this;
        }
    
        // --------------------------------------------------------
    
        /** @brief Destructor. Releases the strong reference. */
        ~Ref() noexcept
        {
            Release();
        }
    
        // --------------------------------------------------------
    
        /**
         * @brief Swaps the managed pointers of two Ref objects safely.
         * @param other The other Ref object to swap with.
         */
        void Swap(Ref& other) noexcept
        {
            std::swap(m_Ptr, other.m_Ptr);
        }
    
        /** @brief Checks if the Ref is currently managing a valid object. */
        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return m_Ptr != nullptr;
        }
    
        /** @brief Member access operator. */
        [[nodiscard]] T* operator->() const noexcept { return m_Ptr; }
        
        /** @brief Dereference operator. */
        [[nodiscard]] T& operator*()  const noexcept { return *m_Ptr; }
    
        /** @brief Retrieves the underlying raw pointer. */
        [[nodiscard]] T* Get() const noexcept { return m_Ptr; }
    
        /**
         * @brief Releases the currently managed object and optionally adopts a new one.
         * @param ptr The new raw pointer to manage (defaults to nullptr).
         */
        void Reset(T* ptr = nullptr) noexcept
        {
            Ref tmp(ptr);
            Swap(tmp);
        }
    
        /**
         * @brief Safely casts the managed object to a derived type using dynamic_cast.
         * @tparam U The target derived type.
         * @return A Ref of the new type, or a null Ref if the cast fails.
         */
        template <typename U>
        [[nodiscard]] Ref<U> As() const noexcept
        {
            return Ref<U>(dynamic_cast<U*>(m_Ptr));
        }
    
        /**
         * @brief Fast cast to a derived type using static_cast. 
         * @warning It is the caller's responsibility to guarantee the true runtime type, 
         * as compile-time checks (like std::is_base_of) cannot verify this.
         * @tparam U The target derived type.
         * @return A Ref of the new type.
         */
        template <typename U>
        // requires std::is_base_of_v<T, U>
        [[nodiscard]] Ref<U> AsFast() const noexcept
        {
            return Ref<U>(static_cast<U*>(m_Ptr));
        }
        
        /**
         * @brief Checks if the managed object is of a specific type.
         * Uses dynamic_cast to determine if the underlying object can be safely cast to type U.
         * @tparam U The type to check against.
         * @return True if the object is of type U (or derived from U), false otherwise.
         */
        template <typename U>
        [[nodiscard]] bool Is() const noexcept
        {
            return static_cast<bool>(dynamic_cast<U*>(m_Ptr));
        }
    
        /**
         * @brief Factory method for creating managed objects and returning them inside a Ref.
         * @tparam Args Argument types to forward to the constructor.
         * @param args Arguments to pass to the object's constructor.
         * @return A valid Ref<T> managing the newly allocated object.
         */
        template <typename... Args>
        [[nodiscard]] static Ref Create(Args&&... args)
        {
            return Ref(new T(std::forward<Args>(args)...));
        }
    
        /**
         * @brief Relinquishes ownership of the managed object without decrementing the strong count.
         * @warning The caller is now strictly responsible for calling DecStrong() manually 
         * or wrapping the pointer back into a Ref using the AdoptTag.
         * @return The raw pointer.
         */
        [[nodiscard]] T* Detach() noexcept
        {
            T* ptr = m_Ptr;
            m_Ptr = nullptr;
            return ptr;
        }
    
        // comparisons
    
        /** @brief Equality check between two Refs. */
        [[nodiscard]] bool operator==(const Ref& other) const noexcept
        {
            return m_Ptr == other.m_Ptr;
        }
    
        /** @brief Inequality check between two Refs. */
        [[nodiscard]] bool operator!=(const Ref& other) const noexcept
        {
            return m_Ptr != other.m_Ptr;
        }
    
        /** @brief Equality check with ptr. */
        [[nodiscard]] bool operator==(T* other) const noexcept
        {
            return m_Ptr == other;
        }
    
        /** @brief Inequality check with ptr. */
        [[nodiscard]] bool operator!=(T* other) const noexcept
        {
            return m_Ptr != other;
        }
    
        /** @brief Equality check with nullptr. */
        [[nodiscard]] bool operator==(std::nullptr_t) const noexcept
        {
            return m_Ptr == nullptr;
        }
    
        /** @brief Inequality check with nullptr. */
        [[nodiscard]] bool operator!=(std::nullptr_t) const noexcept
        {
            return m_Ptr != nullptr;
        }
    
        [[nodiscard]] bool operator<(const Ref& other) const noexcept
        {
            return m_Ptr < other.m_Ptr;
        }
    
    private:
        /** @brief Internal helper to safely increment the strong count. */
        void IncRef() const noexcept
        {
            if (m_Ptr)
                static_cast<const RefCountedObject*>(m_Ptr)->IncStrong();
        }
    
        /** @brief Internal helper to safely decrement counts and potentially destroy the object. */
        void Release() noexcept
        {
            if (!m_Ptr)
                return;
    
            RefDeleter<T>::Execute(m_Ptr);
            m_Ptr = nullptr;
        }
    
        template <typename U>
        friend class Ref;
    
        template <typename U>
        friend class WeakRef;
    
        /** @brief The underlying raw pointer. */
        T* m_Ptr = nullptr;
    };
    
    // ============================================================
    // WeakRef
    // ============================================================
    
    /**
     * @brief Intrusive weak smart pointer. Observes an object without extending its logical lifetime.
     * @tparam T The type of the managed object.
     */
    template<typename T>
    class WeakRef
    {
    public:
        /** @brief Default constructor, initializes to nullptr. */
        constexpr WeakRef() noexcept = default;
    
        /** @brief Nullptr constructor. */
        constexpr WeakRef(std::nullptr_t) noexcept {}
    
        /**
         * @brief Constructs a weak reference from a strong reference.
         * @param strong The strong reference to observe.
         */
        WeakRef(const Ref<T>& strong) noexcept
            : m_Ptr(strong.Get())
        {
            IncWeakRef();
        }
    
        /**
         * @brief Copy constructor. Increments the weak reference count.
         * @param other The WeakRef to copy from.
         */
        WeakRef(const WeakRef& other) noexcept
            : m_Ptr(other.m_Ptr)
        {
            IncWeakRef();
        }
    
        /**
         * @brief Move constructor. Transfers the weak reference without altering counts.
         * @param other The WeakRef to move from.
         */
        WeakRef(WeakRef&& other) noexcept
            : m_Ptr(other.m_Ptr)
        {
            other.m_Ptr = nullptr;
        }
    
        /**
         * @brief Copy assignment operator. Updates weak counts accordingly.
         * @param other The WeakRef to assign from.
         * @return Reference to this object.
         */
        WeakRef& operator=(const WeakRef& other) noexcept
        {
            if (this == &other)
                return *this;
    
            Release();
            m_Ptr = other.m_Ptr;
    
            IncWeakRef();
    
            return *this;
        }
    
        /**
         * @brief Move assignment operator.
         * @param other The WeakRef to move from.
         * @return Reference to this object.
         */
        WeakRef& operator=(WeakRef&& other) noexcept
        {
            if (this == &other)
                return *this;
    
            Release();
            m_Ptr = other.m_Ptr;
            other.m_Ptr = nullptr;
            return *this;
        }
    
        /** @brief Destructor. Releases the weak reference and may trigger physical deallocation. */
        ~WeakRef() noexcept
        {
            Release();
        }
    
        /**
         * @brief Checks if the managed object has been logically destroyed.
         * @warning This is a snapshot. In multithreaded environments, the object could 
         * expire immediately after this returns true. Always use Lock() for safe access.
         * @return True if the object is dead or null, false otherwise.
         */
        [[nodiscard]] bool Expired() const noexcept
        {
            return !m_Ptr || static_cast<const RefCountedObject*>(m_Ptr)->StrongCount() == 0;
        }
    
        /**
         * @brief Releases the weak reference and resets the pointer to null.
         */
        void Reset() noexcept
        {
            Release();
            m_Ptr = nullptr;
        }
    
        /**
         * @brief Attempts to lock the weak reference, upgrading it to a strong reference.
         * @return A valid Ref<T> if the object is still alive, otherwise a null Ref.
         */
        [[nodiscard]] Ref<T> Lock() const noexcept
        {
            if (!m_Ptr)
                return nullptr;
    
            if (!static_cast<RefCountedObject*>(m_Ptr)->TryIncStrong())
                return nullptr;
    
            return Ref<T>(m_Ptr, DetailRef::AdoptTag{});
        }
    
    private:
        /** @brief Internal helper to safely increment the weak count. */
        void IncWeakRef() noexcept
        {
            if (m_Ptr)
                static_cast<const RefCountedObject*>(m_Ptr)->IncWeak();
        }
        
        /** @brief Internal helper to decrement the weak count and trigger memory deallocation if it hits zero. */
        void Release() noexcept
        {
            RefDeleter<T>::ExecuteWeak(m_Ptr);
            m_Ptr = nullptr;
        }
    
        /** @brief The underlying raw pointer. */
        T* m_Ptr = nullptr;
    };
    
    /**
     * @brief Global helper function to create a new RefCountedObject and return a Ref<T>.
     * Similar to std::make_shared.
     * @tparam T The type of the object to create.
     * @tparam Args Argument types to forward to the constructor.
     * @param args Arguments to pass to the object's constructor.
     * @return A valid Ref<T> managing the newly allocated object.
     */
    template <typename T, typename... Args>
    [[nodiscard]] Ref<T> MakeRef(Args&&... args)
    {
        return Ref<T>::Create(std::forward<Args>(args)...);
    }
}

/**
 * @brief std::hash specialization for Ref<T> to allow usage in unordered containers.
 */
template <typename T>
// NOLINTNEXTLINE(cert-dcl58-cpp)
struct std::hash<wio::runtime::Ref<T>>
{
    [[nodiscard]] std::size_t operator()(const wio::runtime::Ref<T>& ref) const noexcept
    {
        return std::hash<T*>{}(ref.Get());
    }
};

