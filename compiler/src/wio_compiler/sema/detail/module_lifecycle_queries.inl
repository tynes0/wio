// Internal compiler detail extracted from the owning translation unit.
// This file is included inside that translation unit's anonymous namespace.
        bool isModuleLifecycleAttribute(Attribute attribute)
        {
            return attribute == Attribute::ModuleApiVersion ||
                   attribute == Attribute::ModuleLoad ||
                   attribute == Attribute::ModuleUpdate ||
                   attribute == Attribute::ModuleUnload ||
                   attribute == Attribute::ModuleSaveState ||
                   attribute == Attribute::ModuleRestoreState;
        }

        std::vector<Attribute> getModuleLifecycleAttributes(const std::vector<NodePtr<AttributeStatement>>& attributes)
        {
            std::vector<Attribute> lifecycleAttributes;
            for (const auto& attr : attributes)
            {
                if (attr && isModuleLifecycleAttribute(attr->attribute))
                    lifecycleAttributes.push_back(attr->attribute);
            }

            return lifecycleAttributes;
        }

        const char* getModuleLifecycleAttributeName(Attribute attribute)
        {
            // NOLINTNEXTLINE(clang-diagnostic-switch-enum)
            switch (attribute)
            {
            case Attribute::ModuleApiVersion: return "@ModuleApiVersion";
            case Attribute::ModuleLoad: return "@ModuleLoad";
            case Attribute::ModuleUpdate: return "@ModuleUpdate";
            case Attribute::ModuleUnload: return "@ModuleUnload";
            case Attribute::ModuleSaveState: return "@ModuleSaveState";
            case Attribute::ModuleRestoreState: return "@ModuleRestoreState";
            default: return "@UnknownModuleLifecycle";
            }
        }
    }
