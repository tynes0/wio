#include "place_storage_internal.h"

#include "native_sequence_internal.h"

#include <cstdint>

namespace wio::vm
{
    PlaceStorage::~PlaceStorage()
    {
        detachProjection();
        retireProjections();
    }

    void PlaceStorage::bindProjection(PlaceStorage* root, PlaceProjectionDomain* projectionDomain) noexcept
    {
        domain = projectionDomain;
        if (domain)
        {
            nextDomainProjection = domain->first;
            if (nextDomainProjection)
                nextDomainProjection->previousDomainProjection = this;
            domain->first = this;
        }
        borrowedRoot = root;
        if (!root)
            return;
        nextProjection = root->firstProjection;
        if (nextProjection)
            nextProjection->previousProjection = this;
        root->firstProjection = this;
    }

    void PlaceStorage::detachProjection() noexcept
    {
        if (domain)
        {
            if (previousDomainProjection)
                previousDomainProjection->nextDomainProjection = nextDomainProjection;
            else
                domain->first = nextDomainProjection;
            if (nextDomainProjection)
                nextDomainProjection->previousDomainProjection = previousDomainProjection;
            domain = nullptr;
            previousDomainProjection = nullptr;
            nextDomainProjection = nullptr;
        }
        if (!borrowedRoot)
            return;
        if (previousProjection)
            previousProjection->nextProjection = nextProjection;
        else
            borrowedRoot->firstProjection = nextProjection;
        if (nextProjection)
            nextProjection->previousProjection = previousProjection;
        borrowedRoot = nullptr;
        previousProjection = nullptr;
        nextProjection = nullptr;
    }

    void PlaceStorage::retireProjections() noexcept
    {
        while (firstProjection)
        {
            auto* projected = firstProjection;
            projected->detachProjection();
            projected->initialized = false;
            projected->alias = nullptr;
        }
    }

    PlaceProjectionDomain::~PlaceProjectionDomain()
    {
        // Escaped owner-backed places may outlive the Machine. Their physical
        // storage remains owned, but the program's tracking list must not.
        while (first)
        {
            auto* projected = first;
            first = projected->nextDomainProjection;
            projected->domain = nullptr;
            projected->previousDomainProjection = nullptr;
            projected->nextDomainProjection = nullptr;
        }
    }

    void PlaceProjectionDomain::retireCells(const Value& root, bool includeRoot) noexcept
    {
        if (!includeRoot && root.kind() != Value::Kind::Array && root.kind() != Value::Kind::Dictionary &&
            root.kind() != Value::Kind::Component)
            return;
        for (auto* projected = first; projected;)
        {
            auto* next = projected->nextDomainProjection;
            if (projected->initialized && projected->alias && (includeRoot || projected->alias != &root) &&
                detail::containsNativeContainerCell(root, projected->alias))
            {
                projected->detachProjection();
                projected->initialized = false;
                projected->alias = nullptr;
            }
            projected = next;
        }
    }

    void PlaceProjectionDomain::remapCell(const Value* source, Value* destination, const Value& oldDictionary,
                                          const Value& newDictionary)
    {
        for (auto* projected = first; projected; projected = projected->nextDomainProjection)
            if (projected->initialized && projected->alias == source)
            {
                projected->alias = destination;
                if (projected->owner.kind() == Value::Kind::Dictionary && projected->owner.dictionaryCount() &&
                    projected->owner.dictionaryKey(0) == oldDictionary.dictionaryKey(0))
                    projected->owner = newDictionary;
            }
    }

    DictionaryPlaceMutation::DictionaryPlaceMutation(PlaceProjectionDomain& domain, Value& dictionary)
        : dictionary_(dictionary)
    {
        if (!domain.first || !dictionary.dictionaryCount())
            return;
        const auto begin = reinterpret_cast<std::uintptr_t>(dictionary.dictionaryKey(0));
        const auto cells = dictionary.dictionaryCount() * 2;
        for (auto* place = domain.first; place; place = place->nextDomainProjection)
        {
            if (!place->initialized || !place->alias)
                continue;
            const auto address = reinterpret_cast<std::uintptr_t>(place->alias);
            if (address < begin || (address - begin) % sizeof(Value))
                continue;
            const auto index = (address - begin) / sizeof(Value);
            if (index < cells)
                bindings_.push_back({place, *dictionary.dictionaryKey(index / 2), index % 2 == 0});
        }
    }

    DictionaryPlaceMutation::~DictionaryPlaceMutation()
    {
        remap();
    }

    void DictionaryPlaceMutation::remap() noexcept
    {
        for (auto& binding : bindings_)
        {
            auto* place = binding.place;
            if (!place->initialized)
                continue;
            Value* destination = nullptr;
            for (std::size_t i = 0; i < dictionary_.dictionaryCount(); ++i)
                if (*dictionary_.dictionaryKey(i) == binding.key)
                {
                    destination = binding.keyCell ? const_cast<Value*>(dictionary_.dictionaryKey(i))
                                                  : const_cast<Value*>(dictionary_.dictionaryValue(i));
                    break;
                }
            if (destination)
                place->alias = destination;
            else
            {
                place->detachProjection();
                place->initialized = false;
                place->alias = nullptr;
            }
        }
        bindings_.clear();
    }
} // namespace wio::vm
