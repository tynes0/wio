# Wio 0.15.0 Release Notes

Wio 0.15 is the typed and behavioral attribute release.

## Highlights

- `[Attribute]` is the canonical declaration-leading spelling, with grouped or
  stacked lists, qualified names, named arguments, folded defaults, scoped
  activation, and user composition.
- Attribute policies now cover targets, retention, repetition, inheritance,
  requirements, conflicts, allow-lists, exclusivity, cardinality, and explicit
  ordering over one normalized built-in/user model.
- Compile-time `Validator<TTarget>` contracts reject incompatible targets and
  may publish stable constant diagnostics.
- Checked derive processors add bounded methods to concrete and generic
  component/object targets without layout or arbitrary AST mutation.
- Pre hooks support `any` and typed immutable receivers. Post hooks observe
  typed results, finally hooks observe success, and synchronous around hooks
  can map or replace results through a single non-escaping `proceed`.
- Async functions run pre/post/finally inside their coroutine. Async around is
  explicitly rejected in this release.
- Reflection exposes deterministic behavioral pipelines and normalized
  attribute records.
- Module ABI descriptor v9 adds retained attribute and processor descriptors
  for C++ hosts through `WIO_MODULE_CAP_ATTRIBUTE_METADATA_V1`.
- `wio migrate attributes [PATH] --check|--write` safely converts legacy `@`
  applications without touching strings or comments.

## Examples

```wio
[From(std::attribute::PreProcessor)]
object AliveGuard {
    public fn Before(receiver: view Alive) -> bool {
        return receiver.IsAlive();
    }
}

[attribute::Processor(AliveGuard)]
attribute CallbackMethod() for method retain runtime;

[CallbackMethod]
public fn Notify() {
    // skipped when the typed receiver guard returns false
}
```

```cpp
const WioModuleType* type = WioFindModuleType(api, "Controller");
const WioModuleMethod* method = WioFindModuleMethod(type, "Notify");
const auto* audited = WioFindModuleAttribute(
    method->attributes, method->attributeCount, "CallbackMethod");
```

## Compatibility boundary

Legacy `@Name(...)` remains accepted as migration input. Method-only derive and
the rejection of async around are intentional frozen boundaries. General
field/property generation and unrestricted call-context/AST mutation are not
part of Wio 0.15.

Product version: `0.15.0`  
Module ABI descriptor: `9`

