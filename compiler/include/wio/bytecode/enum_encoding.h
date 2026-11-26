#pragma once

#include "wio/wir/lowered_ir.h"

#include <cstdint>

namespace wio::bytecode
{
#define WIO_DECLARE_ENUM_ENCODER(type) [[nodiscard]] std::uint8_t encodeEnum(wio::wir::type value) noexcept
    WIO_DECLARE_ENUM_ENCODER(TypeKind);
    WIO_DECLARE_ENUM_ENCODER(NominalKind);
    WIO_DECLARE_ENUM_ENCODER(NominalRepresentation);
    WIO_DECLARE_ENUM_ENCODER(NominalValueModel);
    WIO_DECLARE_ENUM_ENCODER(IntrinsicFamily);
    WIO_DECLARE_ENUM_ENCODER(FieldVisibility);
    WIO_DECLARE_ENUM_ENCODER(CaptureKind);
    WIO_DECLARE_ENUM_ENCODER(AsyncExecutorKind);
    WIO_DECLARE_ENUM_ENCODER(AsyncOperation);
    WIO_DECLARE_ENUM_ENCODER(CoroutineFrameSlotKind);
    WIO_DECLARE_ENUM_ENCODER(OwnershipModel);
    WIO_DECLARE_ENUM_ENCODER(CleanupKind);
    WIO_DECLARE_ENUM_ENCODER(NativeSymbolLanguage);
    WIO_DECLARE_ENUM_ENCODER(NativeCallingConvention);
    WIO_DECLARE_ENUM_ENCODER(NativeExceptionBoundary);
    WIO_DECLARE_ENUM_ENCODER(NativePassingMode);
    WIO_DECLARE_ENUM_ENCODER(NativeMarshallingKind);
    WIO_DECLARE_ENUM_ENCODER(NativeCallbackLifetime);
    WIO_DECLARE_ENUM_ENCODER(NativeCallbackThread);
    WIO_DECLARE_ENUM_ENCODER(NativeThunkKind);
    WIO_DECLARE_ENUM_ENCODER(NativeReceiverKind);
    WIO_DECLARE_ENUM_ENCODER(ModuleKind);
    WIO_DECLARE_ENUM_ENCODER(ModuleImportKind);
    WIO_DECLARE_ENUM_ENCODER(ModuleExportKind);
    WIO_DECLARE_ENUM_ENCODER(ModuleExportRole);
    WIO_DECLARE_ENUM_ENCODER(MetadataTargetKind);
    WIO_DECLARE_ENUM_ENCODER(AttributeOriginKind);
    WIO_DECLARE_ENUM_ENCODER(AttributeProcessorPhase);
    WIO_DECLARE_ENUM_ENCODER(ApplicationStageKind);
    WIO_DECLARE_ENUM_ENCODER(ApplicationAffinity);
    WIO_DECLARE_ENUM_ENCODER(ResourceAccess);
#undef WIO_DECLARE_ENUM_ENCODER

#define WIO_DECLARE_TYPED_ENUM_ENCODER(type) [[nodiscard]] std::uint8_t encodeEnum(wio::wir::typed::type value) noexcept
    WIO_DECLARE_TYPED_ENUM_ENCODER(UnaryOperator);
    WIO_DECLARE_TYPED_ENUM_ENCODER(BinaryOperator);
    WIO_DECLARE_TYPED_ENUM_ENCODER(ConversionKind);
    WIO_DECLARE_TYPED_ENUM_ENCODER(ValueOwnership);
    WIO_DECLARE_TYPED_ENUM_ENCODER(BorrowLifetime);
#undef WIO_DECLARE_TYPED_ENUM_ENCODER

#define WIO_DECLARE_LOWERED_ENUM_ENCODER(type)                                                                         \
    [[nodiscard]] std::uint8_t encodeEnum(wio::wir::lowered::type value) noexcept
    WIO_DECLARE_LOWERED_ENUM_ENCODER(StorageClass);
    WIO_DECLARE_LOWERED_ENUM_ENCODER(EscapeClass);
    WIO_DECLARE_LOWERED_ENUM_ENCODER(BoundsCheckMode);
#undef WIO_DECLARE_LOWERED_ENUM_ENCODER
} // namespace wio::bytecode
