#pragma once

#include "wio/vm/value.h"

#include <vector>

namespace wio::vm
{
    struct PlaceProjectionDomain final
    {
        PlaceStorage* first = nullptr;
        ~PlaceProjectionDomain();
        void retireCells(const Value& root, bool includeRoot = false) noexcept;
        void remapCell(const Value* source, Value* destination, const Value& oldDictionary, const Value& newDictionary);
    };

    class PlaceStorage final
    {
    public:
        Value value;
        Value owner;
        Value* alias = nullptr;
        PlaceStorage* borrowedRoot = nullptr;
        PlaceStorage* firstProjection = nullptr;
        PlaceStorage* previousProjection = nullptr;
        PlaceStorage* nextProjection = nullptr;
        PlaceProjectionDomain* domain = nullptr;
        PlaceStorage* previousDomainProjection = nullptr;
        PlaceStorage* nextDomainProjection = nullptr;
        bool initialized = false;
        bool mutableValue = true;
        // ABI callback cells live only until the current invocation returns.
        // A closure must not retain their raw PlaceStorage addresses.
        bool callbackBorrow = false;

        PlaceStorage() = default;
        PlaceStorage(const PlaceStorage&) = delete;
        PlaceStorage& operator=(const PlaceStorage&) = delete;
        ~PlaceStorage();

        void bindProjection(PlaceStorage* root, PlaceProjectionDomain* projectionDomain = nullptr) noexcept;
        void detachProjection() noexcept;
        void retireProjections() noexcept;

        [[nodiscard]] Value& storedValue() noexcept
        {
            return alias ? *alias : value;
        }

        [[nodiscard]] const Value& storedValue() const noexcept
        {
            return alias ? *alias : value;
        }

        void clear() noexcept
        {
            retireProjections();
            storedValue() = {};
            initialized = false;
        }
    };

    // Capture only live direct entry aliases, not a copy of the dictionary graph.
    // Value owns key snapshots, so relocation/reordering cannot change lookup
    // identity. Nested cells stay at their existing aggregate addresses.
    class DictionaryPlaceMutation final
    {
        struct Binding
        {
            PlaceStorage* place;
            Value key;
            bool keyCell;
        };
        Value& dictionary_;
        std::vector<Binding> bindings_;

    public:
        DictionaryPlaceMutation(PlaceProjectionDomain& domain, Value& dictionary);
        DictionaryPlaceMutation(const DictionaryPlaceMutation&) = delete;
        DictionaryPlaceMutation& operator=(const DictionaryPlaceMutation&) = delete;
        ~DictionaryPlaceMutation();
        void remap() noexcept;
    };
} // namespace wio::vm
