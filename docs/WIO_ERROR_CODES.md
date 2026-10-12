# Wio Error and Status Code Reference

Generated from maintained implementation sources by `tools/Update-DiagnosticReference.ps1`.
This is the current development-source inventory, not a promise that every code exists in every installed release.

## Scope and interpretation

- Includes every literal WIR/LIR/WIRN/WCPP/WBC/WVM code found in compiler, app, runtime, VM, SDK, CLI, scripts and tools C++/Wio sources. Test expectations and historical documents are not definitions.
- Each entry gives source message templates and linked definition sites. `{dynamic context}` denotes runtime details; it is not literal output. One ID may have several context-dependent definitions.
- Dynamic-only messages are explicitly marked and given a reviewed category explanation. Follow the source expression and surrounding validation for their exact details. Reviewed overrides also distinguish conditional code/message branches.
- Verifier errors mean an IR/bytecode invariant was violated; valid source reaching them can indicate a compiler defect. Other codes describe ordinary runtime failures such as bounds, lifetime, cancellation or foreign-boundary errors.
- Native/module status enums, SDK exception categories and std error/domain enums are listed separately below. Pending/ready/success states are not errors.
- Legacy lexer/parser/analyzer/CLI messages without assigned stable IDs cannot have code definitions here. User/custom Result codes and native OS error numbers are open domains, not a finite Wio registry.
- This is a source inventory, not the future normative structured diagnostic registry, severity/fix-it catalog or `wio explain` implementation.

Regenerate: `pwsh -File tools/Update-DiagnosticReference.ps1`.
Check freshness: `pwsh -File tools/Update-DiagnosticReference.ps1 -Check`.

Distinct diagnostic IDs: **692**.

## Diagnostic families

| Family | IDs | Meaning |
| --- | ---: | --- |
| [WIR](#wir) | 221 | Typed WIR building/verification, specialization and canonical lowering. |
| [LIR](#lir) | 129 | Lowered WIR verification and canonical ownership/control-flow invariants. |
| [WIRN](#wirn) | 6 | Native ABI/thunk planning invariants. |
| [WCPP](#wcpp) | 25 | Lowered WIR C++ backend validation and emission. |
| [WBC](#wbc) | 93 | Bytecode compilation, format decoding and verification. |
| [WVM](#wvm) | 218 | VM loading/execution, intrinsics, native transport and lifetime failures. |

## WIR

### WIR1000

- Typed WIR module name cannot be empty.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:331](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L331).

### WIR1001

- Typed WIR type table contains an invalid type.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:338](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L338).

### WIR1002

- Typed WIR type references an unknown type id.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:349](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L349).

### WIR1003

- Named Typed WIR types require a stable logical name.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:352](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L352).

### WIR1004

- Unary constructed Typed WIR type requires exactly one type argument.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:361](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L361).

### WIR1005

- Dictionary Typed WIR type requires key and value arguments.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:364](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L364).

### WIR1006

- Only array Typed WIR types may carry a static extent.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:366](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L366).

### WIR1007

- Only named Typed WIR types may carry a nominal kind.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:368](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L368).

### WIR1008

- Native POD representation requires a named component, enum, or flagset type.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:376](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L376).

### WIR1009

- Only named Typed WIR types may carry layout or lifecycle metadata.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:387](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L387).

### WIR1010

- Typed WIR field layouts require unique names and valid field types.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:411](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L411).

### WIR1011

- Typed WIR nominal base layouts must reference named types.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:417](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L417).

### WIR1012

- Typed WIR method layouts require names, functions, valid signatures, and unique slots.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:428](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L428).

### WIR1013

- Typed WIR method layout references an unknown or non-method function.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:776](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L776).

### WIR1014

- Generic parameter Typed WIR types require a stable source name.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:356](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L356).

### WIR1015

- Only named Typed WIR types may carry a nominal value model.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:370](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L370).

### WIR1016

- Typed WIR ownership and cleanup metadata are inconsistent.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:405](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L405).

### WIR1017

- Native POD types require canonical C++ type binding metadata.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:380](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L380).

### WIR1018

- Only Native POD types may carry native type binding metadata.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:382](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L382).

### WIR1019

- Typed WIR enum/flagset layout requires an integer underlying type.
- Only enum/flagset Typed WIR types may carry case layout metadata.
- Typed WIR enum/flagset cases require unique non-empty names.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:393](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L393), [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:395](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L395), [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:399](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L399).

### WIR1020

- Typed WIR global id must be valid and unique.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:438](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L438).

### WIR1021

- Typed WIR global requires a name, type, and consistent mutability.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:440](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L440).

### WIR1022

- Typed WIR global initializer must be a known zero-argument function returning the global type.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:784](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L784).

### WIR1023

- Symbolic array extent must identify a const generic parameter.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:344](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L344).

### WIR1100

- Typed WIR function has an invalid id.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:448](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L448).

### WIR1101

- Typed WIR function id is duplicated.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:452](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L452).

### WIR1102

- Typed WIR function name cannot be empty.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:454](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L454).

### WIR1103

- Typed WIR function return type is invalid.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:456](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L456).

### WIR1104

- External Typed WIR functions cannot contain blocks.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:794](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L794).

### WIR1105

- Defined Typed WIR functions require at least one block.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:800](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L800).

### WIR1106

- Typed WIR methods require a named owner and leading self reference parameter.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:535](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L535).

### WIR1107

- Non-method Typed WIR functions cannot carry an owner type.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:541](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L541).

### WIR1108

- Typed WIR function requires a callable type ending in its return type.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:474](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L474).

### WIR1109

- Typed WIR generic parameter metadata must reference generic parameter types.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:504](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L504).

### WIR1110

- Typed WIR closure capture layout and hidden parameter count are inconsistent.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:511](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L511).

### WIR1111

- Typed WIR closure capture layout does not match its hidden parameter.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:524](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L524).

### WIR1112

- Typed WIR visible parameters must match the callable type after hidden receiver/capture parameters.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:490](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L490).

### WIR1113

- Native Typed WIR function binding has an invalid identity, ABI signature, or thunk contract.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:569](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L569).

### WIR1114

- Async Typed WIR function requires coroutine&lt;T&gt; return and matching coroutine layout.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:463](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L463).

### WIR1115

- Non-async Typed WIR function cannot carry coroutine layout.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:468](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L468).

### WIR1200

- Typed WIR value has an invalid id.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:818](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L818).

### WIR1201

- Typed WIR value has an invalid type.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:822](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L822).

### WIR1202

- Typed WIR value id is defined more than once.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:833](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L833).

### WIR1204

- Typed WIR parameter ownership does not match its type contract.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:824](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L824).

### WIR1205

- Borrowed Typed WIR values require lifetime metadata, and owned values cannot carry it.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:828](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L828).

### WIR1300

- Typed WIR block has an invalid id.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:845](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L845).

### WIR1301

- Typed WIR block id is duplicated.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:847](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L847).

### WIR1302

- Typed WIR block requires a terminator.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:960](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L960).

### WIR1303

Definition: Typed WIR terminator must be the final instruction in its block.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:970](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L970).

### WIR1304

Definition: Typed WIR block must end with a terminator.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:970](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L970).

### WIR1400

- Typed WIR instruction references an unknown value.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:980](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L980).

### WIR1401

- Value-producing Typed WIR instruction requires a typed result.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:1064](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L1064).

### WIR1402

- Typed WIR terminator cannot produce a value.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:1067](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L1067).

### WIR1403

- Typed WIR constant literal does not match its result type.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:1104](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L1104).

### WIR1404

- Typed WIR unary instruction requires one operand matching its result type.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:1111](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L1111).

### WIR1405

- Typed WIR binary operands must have the same type.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:1120](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L1120).

### WIR1406

- Typed WIR comparison result must be bool.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:1126](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L1126).

### WIR1407

- Typed WIR binary result must match its operand type.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:1131](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L1131).

### WIR1408

- Typed WIR select requires bool condition and matching result arms.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:1792](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L1792).

### WIR1409

- Typed WIR call references an unknown function.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:2046](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L2046).

### WIR1410

- Typed WIR call argument count does not match its callee.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:2065](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L2065).

### WIR1411

- Typed WIR call argument type does not match its parameter (operand={dynamic context}, signature={dynamic context}, parameter={dynamic context}).

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:2087](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L2087).

### WIR1412

- Typed WIR call result does not match its callee return type.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:2104](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L2104).

### WIR1413

- Typed WIR return value does not match its function return type.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:2153](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L2153).

### WIR1414

- Typed WIR branch requires exactly one target.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:2160](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L2160).

### WIR1415

- Typed WIR conditional branch requires a bool condition and two targets.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:2188](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L2188).

### WIR1416

- Typed WIR instruction references an unknown block.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:2209](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L2209).

### WIR1417

- Typed WIR branch argument count does not match its target block.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:2167](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L2167).

### WIR1418

- Typed WIR branch argument type does not match its target block parameter.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:2176](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L2176).

### WIR1419

- Typed WIR conditional branch targets cannot require block arguments.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:2198](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L2198).

### WIR1420

- Typed WIR value is used before its definition in the same block.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:993](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L993).

### WIR1421

- Typed WIR value definition does not dominate its use.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:1002](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L1002).

### WIR1422

- Typed WIR numeric conversion requires one numeric operand and a numeric result.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:1157](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L1157).

### WIR1423

- Typed WIR variant test requires one named value, a variant selector, and a bool result.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:1170](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L1170).

### WIR1424

- Typed WIR variant payload requires one named value and a variant selector.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:1191](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L1191).

### WIR1425

- Typed WIR array length requires one array value and a usize result.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:1205](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L1205).

### WIR1426

- Typed WIR array element projection must return its array element type.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:1217](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L1217).

### WIR1427

- Typed WIR array creation requires element operands matching its array type and extent.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:1235](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L1235).

### WIR1428

- Typed WIR array get requires an array, an integer index, and its element result type.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:1253](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L1253).

### WIR1429

- Typed WIR local place requires a named reference result and no operands.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:1497](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L1497).

### WIR1430

Definition: Typed WIR place initialization requires a reference place and a matching value.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:1541](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L1541).

### WIR1431

Definition: Typed WIR store requires a mutable reference place and a matching value.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:1541](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L1541).

### WIR1432

- Typed WIR load requires a reference place and its referred result type.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:1573](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L1573).

### WIR1433

- Typed WIR field place requires a named base, field selector, and non-strengthening reference result.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:1606](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L1606).

### WIR1434

- Typed WIR array place requires an array reference, integer index, and non-strengthening element reference.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:1654](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L1654).

### WIR1435

- Typed WIR borrow cannot change the referred type or strengthen mutability.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:1707](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L1707).

### WIR1436

- Typed WIR place initialization must target an uninitialized local place exactly once.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:1558](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L1558).

### WIR1437

- Typed WIR field place must match the nominal field layout and field mutability (owner={dynamic context}, selector='{dynamic context}', expected={dynamic context}, actual={dynamic context}, field-mutable={dynamic context}true{dynamic context}false{dynamic context}, place-mutable={dynamic context}true{dynamic context}false{dynamic context}, constructor={dynamic context}true{dynamic context}false{dynamic context}).

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:1621](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L1621).

### WIR1438

- Typed WIR construction requires a matching constructible component/object result and typed constructor signature.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:1729](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L1729).

### WIR1439

- Typed WIR drop requires a place containing a cleanup-bearing value.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:1781](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L1781).

### WIR1440

- Typed WIR method dispatch must match its owner slot, receiver, signature, and callee (selector='{dynamic context}', slot={dynamic context}, owner={dynamic context}, receiver={dynamic context}, callee=#{dynamic context}, operands={dynamic context}, signatures={dynamic context}, dispatch-valid={dynamic context}true{dynamic context}false{dynamic context}, callee-found={dynamic context}true{dynamic context}false{dynamic context}, method-found={dynamic context}true{dynamic context}false{dynamic context}, result={dynamic context}, method-result={dynamic context}, callee-result={dynamic context}, callee-generic={dynamic context}true{dynamic context}false{dynamic context}).

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:1968](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L1968).

### WIR1441

- Typed WIR object cast requires compatible object/interface source and target types.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:2009](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L2009).

### WIR1442

- Typed WIR type test requires an object/interface value, target type, and bool result.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:2021](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L2021).

### WIR1443

- Typed WIR identity equality requires two object/interface values and a bool result.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:2036](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L2036).

### WIR1444

- Typed WIR callable generic arguments must reference valid types.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:1072](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L1072).

### WIR1445

- Typed WIR function reference must pin a known callable declaration and callable type.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:1807](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L1807).

### WIR1446

- Typed WIR closure creation must match its closure body capture layout exactly.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:1836](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L1836).

### WIR1447

- Typed WIR indirect call must match its function value signature.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:1867](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L1867).

### WIR1448

- Typed WIR extension call must pin its implementation, receiver, signature, and specialization.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:1899](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L1899).

### WIR1455

- Typed WIR Option/Result variant test has an invalid variant selector.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:1180](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L1180).

### WIR1456

- Typed WIR dictionary creation requires alternating key/value operands matching its concrete dictionary type.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:1273](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L1273).

### WIR1457

- Typed WIR dictionary get requires matching dictionary, key, and mapped result types.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:1289](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L1289).

### WIR1458

- Typed WIR interpolation requires string/text identity, one more segment than values, and concrete value signatures.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:1312](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L1312).

### WIR1459

- Typed WIR enum constant requires a named enum/flagset result and stable member selector.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:1327](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L1327).

### WIR1460

Definition: Intrinsic call metadata is incomplete or inconsistent: family, selector, receiver, target type and concrete operand signature must agree. The diagnostic includes the actual types/counts.

Detailed message: dynamically supplied by the reporting path.
Message expression: `std::move(detail)`.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:1366](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L1366).

### WIR1461

- Typed WIR any box requires one operand and its materialized payload type.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:1377](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L1377).

### WIR1462

- Typed WIR any test/cast requires one any operand and a valid concrete target type.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:1393](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L1393).

### WIR1463

- Typed WIR nullable wrap requires one value matching the nullable payload type.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:1406](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L1406).

### WIR1464

- Typed WIR dictionary place requires a dictionary reference, matching key, and mapped-value reference.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:1677](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L1677).

### WIR1465

- Typed WIR load ownership must be borrowed for cleanup-bearing values in function '{dynamic context}' (expected={dynamic context}, actual={dynamic context}).

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:1582](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L1582).

### WIR1466

- Typed WIR copy must create one owned claim for a cleanup-bearing value.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:1742](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L1742).

### WIR1467

- Typed WIR move must transfer a cleanup-bearing place into one owned value.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:1756](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L1756).

### WIR1468

- Typed WIR release requires one cleanup-bearing owned value.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:1767](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L1767).

### WIR1469

- Cleanup-bearing place is initialized while already live.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:2257](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L2257).

### WIR1470

- Typed WIR native calls and ordinary calls must match the callee ABI binding.
- Cleanup-bearing place is moved or dropped more than once.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:2055](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L2055), [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:2265](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L2265).

### WIR1471

- Replace requires an initialized cleanup-bearing place.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:2272](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L2272).

### WIR1472

- Function exit leaves cleanup-bearing local places live.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:2276](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L2276).

### WIR1473

- Typed WIR async metadata is attached to an unsupported instruction.
- Control-flow merge disagrees on live cleanup-bearing places.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:1080](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L1080), [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:2290](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L2290).

### WIR1474

- Typed WIR await requires an async function, one coroutine&lt;T&gt;, and its exact payload result.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:2126](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L2126).

### WIR1475

- Typed WIR executor switch requires an async function and an explicit destination executor.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:2138](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L2138).

### WIR1477

- Typed WIR iterator creation requires a canonical range/array/dictionary source.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:1440](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L1440).

### WIR1478

- Typed WIR iterator operation requires one iterator and a canonical result shape.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:1458](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L1458).

### WIR1479

- Typed WIR Result operation requires canonical Result&lt;T&gt; source and payload/propagation types.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:1486](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L1486).

### WIR1480

- Typed WIR global-place must match a declared global and its mutability.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:1509](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L1509).

### WIR1481

- Typed WIR pack expansion metadata must align with symbolic pack operands.
- Typed WIR nullable unwrap requires one nullable value and its payload result.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:1021](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L1021), [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:1421](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L1421).

### WIR1482

- Typed WIR range containment requires three equal numeric operands and a bool result.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:1143](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L1143).

### WIR1490

- Generic constant must identify a typed const parameter.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:1098](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L1098).

### WIR1491

- Default value requires a value type and no operands.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:1089](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L1089).

### WIR1500

- Typed WIR module contract requires a canonical stable identity.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:587](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L587).

### WIR1501

- Typed WIR SDK call table version, identity, and export cardinality must be canonical.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:593](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L593).

### WIR1502

- Typed WIR module imports require unique stable identities and logical names.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:598](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L598).

### WIR1503

- Typed WIR export descriptor has an invalid stable identity, slot, signature, or target.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:636](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L636).

### WIR1504

- Typed WIR reflection descriptors require unique identities and matching nominal types.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:647](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L647).

### WIR1505

- Typed WIR module lifecycle must reference known functions and pair save/restore state hooks.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:767](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L767).

### WIR1506

- Typed WIR reflected fields require unique identities, names, and valid types.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:653](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L653).

### WIR1507

- Typed WIR reflected methods require unique identities and valid callable signatures.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:657](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L657).

### WIR1508

- Typed WIR attribute applications require unique identities and valid targets.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:674](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L674).

### WIR1509

- Typed WIR attribute processors require a stable phase, identity, executable hook, and value type.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:688](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L688).

### WIR1510

- Typed WIR reflected type references an unknown attribute application.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:698](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L698).

### WIR1511

- Typed WIR reflected field references an unknown attribute application.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:701](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L701).

### WIR1512

- Typed WIR reflected method references an unknown attribute application.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:704](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L704).

### WIR1513

- Typed WIR systems require unique component types and valid lifecycle functions.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:719](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L719).

### WIR1514

- Typed WIR application requires a component type, entry/lifecycle functions, and declared systems.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:733](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L733).

### WIR1515

- Typed WIR application stages require canonical order, dependencies, and frequency.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:744](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L744).

### WIR1516

- Typed WIR application runs require resolved targets, functions, and resources.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:755](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L755).

### WIR1517

- Typed WIR reflected enum/flagset cases require unique identities and names.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:661](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L661).

### WIR1518

- Typed WIR reflected enum/flagset case references an unknown attribute application.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_verifier.cpp:707](../compiler/src/wio_compiler/wir/typed_ir_verifier.cpp#L707).

### WIR2000

- Cannot build Typed WIR from a null program.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:40](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L40).

### WIR2001

- Semantic type is missing while building Typed WIR.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:2201](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L2201).

### WIR2002

- Unsupported semantic primitive type '{dynamic context}' while mapping {dynamic context}an unknown source{dynamic context}.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:2231](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L2231).

### WIR2003

- Semantic type '{dynamic context}' has no backend-neutral WIR representation.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:2606](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L2606).

### WIR2100

- Function declaration is missing its resolved semantic function type.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:3064](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L3064).

### WIR2101

- Function parameter is missing its resolved semantic symbol or type.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:3138](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L3138).

### WIR2102

- Non-void function does not end with a return.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:3253](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L3253).

### WIR2103

- Async function must have a resolved coroutine&lt;T&gt; return type.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:3087](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L3087).

### WIR2201

- Statement kind '{dynamic context}' has no Typed WIR lowering.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:3459](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L3459).

### WIR2202

- Local variable declaration is missing its semantic symbol.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:3339](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L3339).

### WIR2203

- Default initialization for type '{dynamic context}' is not supported by Typed WIR.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:5903](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L5903).

### WIR2204

- Loop-carried local value is unavailable on a control-flow edge.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:6788](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L6788).

### WIR2205

Definition: Break statement has no active Typed WIR loop target.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:6801](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L6801).

### WIR2206

Definition: Continue statement has no active Typed WIR loop target.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:6801](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L6801).

### WIR2210

- For-in requires a range, array, or dictionary iterator source.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:6990](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L6990).

### WIR2211

- For-in binding is missing its semantic symbol.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:7080](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L7080).

### WIR2300

- Integer literal has no representable WIR integer type.
- Integer literal cannot be represented by the initial Typed WIR literal model.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:3789](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L3789), [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:3796](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L3796).

### WIR2301

- Identifier is not a value available in the current Typed WIR function.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:4182](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L4182).

### WIR2302

- Unary expression could not be lowered to Typed WIR.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:4511](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L4511).

### WIR2303

- Binary expression could not be lowered to Typed WIR.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:4653](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L4653).

### WIR2304

- Call target has no indexed WIR function identity.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:5137](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L5137).

### WIR2305

- Expression kind '{dynamic context}' has no Typed WIR lowering.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:5242](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L5242).

### WIR2306

- Await requires a resolved coroutine&lt;T&gt; operand.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:4423](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L4423).

### WIR2308

- Compound assignment operator is not supported by Typed WIR.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:4381](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L4381).

### WIR2309

- Float literal has no representable WIR floating-point type.
- Float literal cannot be represented by the Typed WIR literal model.
- Assignment target did not lower to a reference place.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:3810](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L3810), [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:3816](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L3816), [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:4368](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L4368).

### WIR2310

- Char literal must contain exactly one byte-sized character.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:3871](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L3871).

### WIR2311

- Byte literal is outside the u8 range.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:3884](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L3884).

### WIR2313

- Typed WIR fit lowering requires numeric or object/interface operands.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:4101](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L4101).

### WIR2314

- Duration literal cannot be represented by Typed WIR.
- Expression requires a conversion that is not allowed by Wio numeric promotion.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:3920](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L3920), [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:5834](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L5834).

### WIR2315

- Match expression requires at least one case.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:6071](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L6071).

### WIR2316

- Typed WIR does not recognize destructuring variant '{dynamic context}'.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:5959](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L5959).

### WIR2317

- Value-producing Typed WIR match cases require expression bodies.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:6085](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L6085).

### WIR2319

- Match binding is missing its semantic symbol.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:5983](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L5983).

### WIR2320

- Array literal requires a resolved WIR array element type.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:3930](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L3930).

### WIR2322

- Typed WIR index access requires an array, dictionary, string, or text value.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:4330](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L4330).

### WIR2323

- Addressable expression did not produce a reference place.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:932](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L932).

### WIR2324

- A mutable place was requested from a read-only view.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:937](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L937).

### WIR2325

- Initial Typed WIR member reads currently require a resolved data field.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:4280](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L4280).

### WIR2326

- Addressable identifier is missing its semantic symbol.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:5274](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L5274).

### WIR2327

- Addressable identifier is not available in the current Typed WIR function.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:5309](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L5309).

### WIR2328

- Only array and dictionary index expressions are addressable WIR places.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:5368](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L5368).

### WIR2329

- Addressable member must resolve to a data field.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:5392](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L5392).

### WIR2330

- Expression kind '{dynamic context}' is not an addressable Typed WIR place.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:5530](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L5530).

### WIR2331

- 'self' is unavailable outside a receiver-aware WIR function.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:4068](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L4068), [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:5256](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L5256).

### WIR2332

- 'super' is unavailable outside a receiver-aware WIR function.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:4075](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L4075).

### WIR2333

- Object/interface type test is missing a representable target type.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:4546](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L4546).

### WIR2340

- Lambda parameter is missing its resolved callable type.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:2867](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L2867).

### WIR2341

- Non-void lambda does not end with a return.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:2951](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L2951).

### WIR2342

- Lambda capture '{dynamic context}' is unavailable in the enclosing callable.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:4014](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L4014).

### WIR2343

- Lambda requested a self capture outside a receiver-aware callable.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:4040](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L4040).

### WIR2344

- Indirect call requires a resolved function value.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:5103](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L5103).

### WIR2345

- Dictionary literal requires resolved key and value types.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:3960](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L3960).

### WIR2350

- Result unwrap/propagation requires a canonical std::Result&lt;T&gt; value.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:3643](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L3643).

### WIR2351

- The backend-neutral 'in' operation currently requires a range.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:4563](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L4563).

### WIR2352

- Overloaded operator is missing its resolved callable identity.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:3571](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L3571).

### WIR2353

- Member operator is missing its nominal owner slot.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:3587](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L3587).

### WIR2357

- Increment/decrement operand did not lower to a mutable place.
- Derived extension is missing its processor method layout.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:4458](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L4458), [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:4867](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L4867).

### WIR2358

- Increment/decrement requires a representable numeric WIR type.
- Extension call is missing a materializable default argument.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:4469](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L4469), [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:4964](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L4964).

### WIR2359

- Constructor call is missing a materializable default argument.
- Derived extension call is missing a materializable default argument.
- Method call is missing a materializable default argument.
- Implicit self method call is missing a materializable default argument.
- Function call is missing a materializable default argument.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:4764](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L4764), [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:4911](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L4911), [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:5057](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L5057), [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:5182](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L5182), [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:5219](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L5219).

### WIR2360

- Implicit field '{dynamic context}' is not mutable.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:871](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L871).

### WIR2361

- Pack array materialization requires a runtime value pack.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:4236](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L4236).

### WIR2362

- Implicit self method call is missing its canonical owner slot.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:5155](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L5155).

### WIR2370

- Enum/flagset member requires a canonical integer constant value.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:1369](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L1369).

### WIR2401

- NativeCallback lifetime must be 'call' or 'retained'.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:676](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L676).

### WIR2402

- NativeCallback thread must be 'caller' or 'any'.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:680](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L680).

### WIR2403

- NativeConsume requires an owning value, not ref/view, opaque, callback or task storage.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:659](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L659).

### WIR2404

- NativeConsume is only supported on native function parameters.

Definitions: [compiler/src/wio_compiler/wir/typed_ir_builder.cpp:3196](../compiler/src/wio_compiler/wir/typed_ir_builder.cpp#L3196).

### WIR3001

- Typed select instruction has an invalid shape.

Definitions: [compiler/src/wio_compiler/wir/lowering_pipeline.cpp:532](../compiler/src/wio_compiler/wir/lowering_pipeline.cpp#L532).

### WIR3002

- Typed branch requires exactly one target before lowering.

Definitions: [compiler/src/wio_compiler/wir/lowering_pipeline.cpp:575](../compiler/src/wio_compiler/wir/lowering_pipeline.cpp#L575).

### WIR3003

- Typed conditional branch has an invalid shape before lowering.

Definitions: [compiler/src/wio_compiler/wir/lowering_pipeline.cpp:591](../compiler/src/wio_compiler/wir/lowering_pipeline.cpp#L591).

### WIR3004

- Suspension instruction appears outside an async function.

Definitions: [compiler/src/wio_compiler/wir/lowering_pipeline.cpp:468](../compiler/src/wio_compiler/wir/lowering_pipeline.cpp#L468).

### WIR3100

Definition: Generic function/type materialization failed. The detailed message identifies the concrete substitution, pack/constant/layout/capture or specialization invariant that failed; there is no single fixed payload.

Detailed message: dynamically supplied by the reporting path.
Message expression: `std::move(message)`.

Definitions: [compiler/src/wio_compiler/wir/generic_specializer.cpp:575](../compiler/src/wio_compiler/wir/generic_specializer.cpp#L575).

### WIR3101

- Generic materialization of '{dynamic context}' exceeded its body limit ({dynamic context}); possible expanding polymorphic recursion.

Definitions: [compiler/src/wio_compiler/wir/generic_specializer.cpp:1585](../compiler/src/wio_compiler/wir/generic_specializer.cpp#L1585).

### WIR3200

- lower-object-hierarchy

Definitions: [compiler/src/wio_compiler/wir/lowering_pipeline.cpp:674](../compiler/src/wio_compiler/wir/lowering_pipeline.cpp#L674).

## LIR

### LIR1000

- Lowered WIR module name cannot be empty.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:223](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L223).

### LIR1001

- Lowered WIR type table contains an invalid type.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:234](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L234).

### LIR1002

- Lowered WIR type references an unknown type id.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:238](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L238).

### LIR1003

- Only named Lowered WIR types may carry a nominal kind.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:241](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L241).

### LIR1004

- Native POD representation requires a named component, enum, or flagset type.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:249](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L249).

### LIR1005

- Only named Lowered WIR types may carry layout or lifecycle metadata.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:260](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L260).

### LIR1006

- Lowered WIR field layouts require unique names and valid field types.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:277](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L277).

### LIR1007

- Lowered WIR nominal base layouts must reference named types.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:283](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L283).

### LIR1008

- Lowered WIR method layouts require valid signatures and unique slots.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:293](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L293).

### LIR1009

- Only named Lowered WIR types may carry a nominal value model.
- Lowered WIR method layout references an unknown or non-method function.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:243](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L243), [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:610](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L610).

### LIR1010

- Native POD types require canonical C++ type binding metadata.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:253](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L253).

### LIR1011

- Only Native POD types may carry native type binding metadata.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:255](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L255).

### LIR1012

- Lowered WIR enum/flagset layout requires an integer underlying type.
- Only enum/flagset Lowered WIR types may carry case layout metadata.
- Lowered WIR enum/flagset cases require unique non-empty names.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:266](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L266), [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:268](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L268), [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:272](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L272).

### LIR1018

- Symbolic array extent must identify a const generic parameter.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:231](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L231).

### LIR1020

- Lowered WIR global id must be valid and unique.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:301](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L301).

### LIR1021

- Lowered WIR global requires a name, type, and consistent mutability.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:303](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L303).

### LIR1022

- Lowered WIR global initializer must be a known zero-argument function returning the global type.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:619](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L619).

### LIR1100

- Lowered WIR function has an invalid id.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:311](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L311).

### LIR1101

- Lowered WIR function id is duplicated.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:313](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L313).

### LIR1102

- Lowered WIR function name cannot be empty.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:315](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L315).

### LIR1103

- Lowered WIR function return type is invalid.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:317](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L317).

### LIR1104

- External Lowered WIR functions cannot contain blocks.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:651](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L651).

### LIR1105

- Defined Lowered WIR functions require at least one block.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:657](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L657).

### LIR1106

- Lowered WIR methods require a named owner and leading self parameter.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:364](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L364).

### LIR1110

- Lowered WIR function requires a callable type ending in its return type.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:332](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L332).

### LIR1111

- Lowered WIR closure capture metadata is inconsistent.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:355](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L355).

### LIR1112

- Lowered WIR visible parameters must match the callable type after hidden receiver/capture parameters.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:347](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L347).

### LIR1113

- Native Lowered WIR function binding has an invalid identity, ABI signature, or thunk contract.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:385](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L385).

### LIR1114

- Async Lowered WIR function requires coroutine&lt;T&gt; return and matching coroutine layout.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:323](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L323).

### LIR1115

- Non-async Lowered WIR function cannot carry coroutine layout.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:327](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L327).

### LIR1200

- Lowered WIR value has an invalid id.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:636](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L636).

### LIR1201

- Lowered WIR value has an invalid type.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:640](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L640).

### LIR1202

- Lowered WIR value id is defined more than once.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:642](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L642).

### LIR1300

- Lowered WIR block has an invalid id.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:666](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L666).

### LIR1301

- Lowered WIR block id is duplicated.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:668](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L668).

### LIR1302

- Lowered WIR block requires a terminator.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:744](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L744).

### LIR1303

Definition: Lowered WIR terminator must be the final instruction in its block.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:754](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L754).

### LIR1304

Definition: Lowered WIR block must end with a terminator.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:754](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L754).

### LIR1400

- Lowered WIR instruction references an unknown value.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:763](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L763).

### LIR1401

- Value-producing Lowered WIR instruction requires a typed result.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:814](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L814).

### LIR1402

- Lowered WIR terminator cannot produce a value.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:817](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L817).

### LIR1403

- Lowered WIR unary operand must match its result type.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:937](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L937).

### LIR1404

- Lowered WIR binary operands must have the same type.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:944](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L944).

### LIR1405

- Lowered WIR comparison result must be bool.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:948](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L948).

### LIR1406

- Lowered WIR binary result must match its operand type.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:952](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L952).

### LIR1407

- Lowered WIR call references an unknown function.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:1773](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L1773).

### LIR1408

- Lowered WIR call argument count does not match its callee.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:1789](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L1789).

### LIR1409

- Lowered WIR call argument type does not match its parameter.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:1813](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L1813).

### LIR1410

- Lowered WIR call result does not match its callee.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:1825](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L1825).

### LIR1411

- Lowered WIR return value does not match its function.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:1921](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L1921).

### LIR1412

- Lowered WIR jump shape is invalid.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:1932](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L1932).

### LIR1413

- Lowered WIR conditional jump condition must be bool.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:1938](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L1938).

### LIR1414

- Lowered WIR jump references an unknown block.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:1949](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L1949).

### LIR1415

- Lowered WIR jump argument count does not match target block parameters.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:1956](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L1956).

### LIR1416

- Lowered WIR jump argument type does not match target block parameter.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:1964](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L1964).

### LIR1417

- Lowered WIR numeric conversion requires one numeric operand and a numeric result.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:977](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L977).

### LIR1418

- Lowered WIR variant test requires one named value, a variant selector, and a bool result.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:990](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L990).

### LIR1419

- Lowered WIR variant payload requires one named value and a variant selector.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:1003](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L1003).

### LIR1420

- Lowered WIR array length requires one array value and a usize result.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:1017](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L1017).

### LIR1421

- Lowered WIR array element projection must return its array element type.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:1029](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L1029).

### LIR1422

- Lowered WIR array creation requires element operands matching its array type and extent.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:1048](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L1048).

### LIR1423

- Lowered WIR array get requires an array, an integer index, and its element result type.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:1065](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L1065).

### LIR1424

- Lowered WIR local place requires a named reference result and no operands.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:1297](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L1297).

### LIR1425

Definition: Lowered WIR place initialization requires a reference place and a matching value.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:1324](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L1324).

### LIR1426

Definition: Lowered WIR store requires a mutable reference place and a matching value.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:1324](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L1324).

### LIR1427

- Lowered WIR load requires a reference place and its referred result type.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:1353](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L1353).

### LIR1428

- Lowered WIR field place requires a named base, field selector, and non-strengthening reference result.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:1373](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L1373).

### LIR1429

- Lowered WIR array place requires an array reference, integer index, and non-strengthening element reference.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:1423](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L1423).

### LIR1430

- Lowered WIR borrow cannot change the referred type or strengthen mutability.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:1476](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L1476).

### LIR1431

- Lowered WIR place initialization must target an uninitialized local place exactly once.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:1338](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L1338).

### LIR1432

- Lowered WIR field place must match the nominal field layout and field mutability.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:1398](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L1398).

### LIR1433

- Lowered WIR construction requires a matching constructible component/object result and typed constructor signature.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:1498](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L1498).

### LIR1434

- Lowered WIR release/drop-place must match the stored value cleanup protocol.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:1558](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L1558).

### LIR1435

- Lowered WIR method dispatch must match its owner slot, receiver, signature, and callee.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:1719](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L1719).

### LIR1436

- Lowered WIR object cast requires compatible object/interface source and target types.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:1741](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L1741).

### LIR1437

- Lowered WIR dictionary creation requires alternating key/value operands matching its concrete type.
- Lowered WIR type test requires an object/interface value, target type, and bool result.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:1086](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L1086), [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:1751](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L1751).

### LIR1438

- Lowered WIR dictionary get requires matching dictionary, key, and mapped result types.
- Lowered WIR identity equality requires object/interface operands and bool result.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:1101](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L1101), [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:1764](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L1764).

### LIR1439

- Lowered WIR interpolation requires string/text identity, segments, and concrete signatures.
- Lowered WIR function reference must pin a known callable declaration.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:1125](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L1125), [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:1570](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L1570).

### LIR1440

- Lowered WIR enum constant requires a named enum/flagset result and stable selector.
- Lowered WIR closure creation must match its capture layout.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:1141](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L1141), [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:1596](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L1596).

### LIR1441

- Lowered WIR intrinsic call requires a family, selector, receiver, target, and concrete signature.
- Lowered WIR indirect call must match its function value signature.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:1163](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L1163), [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:1624](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L1624).

### LIR1442

- Lowered WIR any box requires one operand and its materialized payload type.
- Lowered WIR extension call must pin its implementation and concrete signature.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:1177](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L1177), [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:1656](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L1656).

### LIR1443

- Lowered WIR any test/cast requires one any operand and a concrete target.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:1193](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L1193).

### LIR1444

- Lowered WIR nullable wrap requires one matching payload value.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:1206](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L1206).

### LIR1445

- Lowered WIR iterator creation requires canonical source metadata.
- Lowered WIR dictionary place requires a dictionary reference, matching key, and mapped reference.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:1241](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L1241), [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:1446](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L1446).

### LIR1446

- Lowered WIR iterator operation requires one iterator and a canonical result.
- Lowered WIR retain/copy-value must match the type cleanup protocol.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:1258](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L1258), [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:1514](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L1514).

### LIR1447

- Lowered WIR move-value must transfer one cleanup-bearing place.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:1528](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L1528).

### LIR1448

- Lowered WIR release/drop-value must match the value cleanup protocol.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:1541](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L1541).

### LIR1450

- Lowered native-invoke and ordinary call must match the callee ABI binding.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:1780](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L1780).

### LIR1451

- Coroutine frame slots require unique dense ids, values, and matching ownership metadata.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:710](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L710).

### LIR1452

- Coroutine states require dense ids and valid suspend/resume/value references.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:724](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L724).

### LIR1453

- Lowered WIR async metadata is attached to an unsupported instruction.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:826](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L826).

### LIR1454

- Cancellation check must immediately precede its coroutine suspension point.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:1837](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L1837).

### LIR1455

- Coroutine suspension must match one canonical state and resume target.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:1876](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L1876).

### LIR1456

- Coroutine resume must materialize the payload declared by its canonical state.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:1890](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L1890).

### LIR1457

- Coroutine completion value must match the async payload type.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:1905](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L1905).

### LIR1458

- Async Lowered WIR must use coroutine-complete instead of return.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:1912](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L1912).

### LIR1466

- Lowered WIR nullable unwrap requires one nullable value and its payload result.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:1220](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L1220).

### LIR1479

- Lowered WIR Result operation requires canonical Result&lt;T&gt; source and payload/propagation types.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:1286](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L1286).

### LIR1480

- Lowered WIR global-place must match a declared global and its mutability.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:1309](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L1309).

### LIR1481

- Lowered WIR pack expansion metadata must align with symbolic pack operands.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:781](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L781).

### LIR1482

- Lowered WIR range containment requires three equal numeric operands and a bool result.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:962](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L962).

### LIR1483

- Lowered WIR storage and escape metadata is incompatible with {dynamic context} (storage={dynamic context}, escape={dynamic context}).

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:853](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L853).

### LIR1484

- Lowered WIR bounds-check metadata must appear on every array access and nowhere else.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:866](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L866).

### LIR1485

- Lowered WIR eliminated bounds check requires a reproducible static or construction proof.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:910](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L910).

### LIR1490

- Generic constant must identify a typed const parameter.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:922](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L922).

### LIR1491

- Default value requires a value type and no operands.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:930](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L930).

### LIR1500

- Lowered WIR module contract requires a canonical stable identity.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:440](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L440).

### LIR1501

- Lowered WIR SDK call table version, identity, and export cardinality must be canonical.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:446](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L446).

### LIR1502

- Lowered WIR export descriptor has an invalid stable identity, slot, or target.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:474](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L474).

### LIR1503

- Lowered WIR module lifecycle must reference known functions and pair save/restore state hooks.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:482](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L482).

### LIR1504

- Lowered WIR module imports require unique stable identities and logical names.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:451](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L451).

### LIR1505

- Lowered WIR reflection descriptors require unique identities and matching nominal types.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:491](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L491).

### LIR1506

- Lowered WIR reflected fields require unique identities, names, and valid types.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:497](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L497).

### LIR1507

- Lowered WIR reflected methods require unique identities and valid callable signatures.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:502](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L502).

### LIR1508

- Lowered WIR attribute applications require unique identities and valid targets.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:518](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L518).

### LIR1509

- Lowered WIR attribute processors require a stable phase, identity, executable hook, and value type.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:532](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L532).

### LIR1510

- Lowered WIR reflected type references an unknown attribute application.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:542](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L542).

### LIR1511

- Lowered WIR reflected field references an unknown attribute application.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:545](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L545).

### LIR1512

- Lowered WIR reflected method references an unknown attribute application.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:548](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L548).

### LIR1513

- Lowered WIR systems require unique component types and valid lifecycle functions.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:562](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L562).

### LIR1514

- Lowered WIR application requires a component type, entry/lifecycle functions, and declared systems.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:576](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L576).

### LIR1515

- Lowered WIR application stages require canonical order, dependencies, and frequency.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:587](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L587).

### LIR1516

- Lowered WIR application runs require resolved targets, functions, and resources.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:598](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L598).

### LIR1517

- Lowered WIR reflected enum/flagset cases require unique identities and names.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:506](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L506).

### LIR1518

- Lowered WIR reflected enum/flagset case references an unknown attribute application.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:551](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L551).

### LIR1530

- Object hierarchy cast/dispatch/lifecycle table does not match canonical contracts.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:407](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L407), [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:416](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L416).

### LIR1531

- Object dispatch references an unknown implementation.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:420](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L420).

### LIR1532

- Field projection must identify its canonical declaring subobject and storage index.
- Field storage index does not identify the resolved declaring field.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:1388](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L1388), [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:432](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L432).

### LIR1533

- Coroutine object receiver must be retained by its canonical frame.

Definitions: [compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp:696](../compiler/src/wio_compiler/wir/lowered_ir_verifier.cpp#L696).

## WIRN

### WIRN1001

- Native thunk plan references a function without native binding metadata.

Definitions: [compiler/src/wio_compiler/wir/native_abi_plan.cpp:53](../compiler/src/wio_compiler/wir/native_abi_plan.cpp#L53).

### WIRN1002

- NativeInvoke does not resolve to a native declaration.

Definitions: [compiler/src/wio_compiler/wir/native_abi_plan.cpp:104](../compiler/src/wio_compiler/wir/native_abi_plan.cpp#L104).

### WIRN1003

- Generic native invocation requires a concrete specialization key.

Definitions: [compiler/src/wio_compiler/wir/native_abi_plan.cpp:112](../compiler/src/wio_compiler/wir/native_abi_plan.cpp#L112).

### WIRN1004

- Concrete native thunk has an invalid identity or ABI signature.

Definitions: [compiler/src/wio_compiler/wir/native_abi_plan.cpp:143](../compiler/src/wio_compiler/wir/native_abi_plan.cpp#L143).

### WIRN1005

- Concrete native thunk symbol is not unique.

Definitions: [compiler/src/wio_compiler/wir/native_abi_plan.cpp:149](../compiler/src/wio_compiler/wir/native_abi_plan.cpp#L149).

### WIRN1006

- NativeInvoke specialization identity does not match its concrete native callee.

Definitions: [compiler/src/wio_compiler/wir/native_abi_plan.cpp:119](../compiler/src/wio_compiler/wir/native_abi_plan.cpp#L119).

## WCPP

### WCPP1000

- invalid Lowered WIR: {dynamic context}: {dynamic context}
- {dynamic context}invalid Lowered WIR: {dynamic context}: {dynamic context}

Definitions: [compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp:238](../compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp#L238), [compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp:3690](../compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp#L3690).

### WCPP1100

- unresolved type reached the C++ backend: {dynamic context}

Definitions: [compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp:450](../compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp#L450).

### WCPP1101

- const-value type has no canonical value

Definitions: [compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp:457](../compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp#L457).

### WCPP1102

- type {dynamic context} requires one argument

Definitions: [compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp:465](../compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp#L465).

### WCPP1103

- dictionary type requires key and value arguments

Definitions: [compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp:469](../compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp#L469).

### WCPP1104

- function type requires a return type

Definitions: [compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp:473](../compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp#L473).

### WCPP1105

- native POD type requires a C++ type binding

Definitions: [compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp:520](../compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp#L520).

### WCPP1106

- object/interface requires canonical hierarchy lowering

Definitions: [compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp:479](../compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp#L479).

### WCPP1107

- enum/flagset requires canonical underlying-type and case layout metadata

Definitions: [compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp:525](../compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp#L525).

### WCPP1108

- global value requires a concrete specialized type

Definitions: [compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp:535](../compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp#L535).

### WCPP1201

- opcode is not implemented by the WIR C++ backend: {dynamic context}

Definitions: [compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp:667](../compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp#L667).

### WCPP1202

- native invocation requires an adapter thunk not implemented by this backend

Definitions: [compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp:780](../compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp#L780).

### WCPP1203

- constructor identity does not match its canonical argument signature
- constructor '{dynamic context}' requires a callable constructor adapter; only canonical field-wise construction is enabled (operands={dynamic context}, fields={dynamic context})

Definitions: [compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp:804](../compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp#L804), [compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp:826](../compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp#L826).

### WCPP1205

- intrinsic family is not implemented by this C++ backend slice: {dynamic context}

Definitions: [compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp:737](../compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp#L737).

### WCPP1206

- enum/flagset intrinsic is not recognized: {dynamic context}

Definitions: [compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp:745](../compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp#L745).

### WCPP1207

- enum/flagset constant does not exist in canonical layout: {dynamic context}

Definitions: [compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp:752](../compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp#L752).

### WCPP1208

- payload variants currently require canonical Option/Result layout

Definitions: [compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp:768](../compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp#L768).

### WCPP1209

- generic call requires a concrete specialized function body

Definitions: [compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp:726](../compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp#L726).

### WCPP1210

- iterator source or operand count is not supported
- iterator projection is not supported: {dynamic context}

Definitions: [compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp:688](../compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp#L688), [compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp:716](../compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp#L716).

### WCPP1211

- cannot construct an object with abstract method contracts

Definitions: [compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp:792](../compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp#L792).

### WCPP1212

- object lifecycle requires a concrete receiver-only body

Definitions: [compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp:506](../compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp#L506), [compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp:515](../compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp#L515).

### WCPP1213

- wire ABI requires owned results and forbids ref/view parameters on asynchronous calls

Definitions: [compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp:609](../compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp#L609).

### WCPP1214

- application requires canonical exit-state fields
- worker application stages require a thread-transfer lowering contract

Definitions: [compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp:546](../compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp#L546), [compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp:549](../compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp#L549).

### WCPP1215

- behavioral attribute requires a concrete processor object and pinned hook function
- around processors are not valid on asynchronous functions

Definitions: [compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp:570](../compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp#L570), [compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp:577](../compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp#L577).

### WCPP1220

- {dynamic context}native-only bridge requires no main and native task sidecars

Definitions: [compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp:3682](../compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp#L3682).

## WBC

### WBC1001

- Module manifest references an invalid string id

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:101](../compiler/src/wio_compiler/bytecode/verifier.cpp#L101).

### WBC1002

- String constant references an invalid string id

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:106](../compiler/src/wio_compiler/bytecode/verifier.cpp#L106).

### WBC1003

- Constant has an unknown kind

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:108](../compiler/src/wio_compiler/bytecode/verifier.cpp#L108).

### WBC1004

- Type references an invalid name string

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:114](../compiler/src/wio_compiler/bytecode/verifier.cpp#L114).

### WBC1005

- Type argument references an invalid type id

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:118](../compiler/src/wio_compiler/bytecode/verifier.cpp#L118).

### WBC1006

- Type extent parameter references an invalid type id

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:121](../compiler/src/wio_compiler/bytecode/verifier.cpp#L121).

### WBC1007

- Global ids must be valid and unique

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:181](../compiler/src/wio_compiler/bytecode/verifier.cpp#L181).

### WBC1008

- Global references invalid string or type metadata

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:183](../compiler/src/wio_compiler/bytecode/verifier.cpp#L183).

### WBC1009

- Function ids must be valid and unique

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:191](../compiler/src/wio_compiler/bytecode/verifier.cpp#L191).

### WBC1010

- Function references invalid string or type metadata

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:194](../compiler/src/wio_compiler/bytecode/verifier.cpp#L194).

### WBC1011

- Block ids must be valid and unique within a function

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:274](../compiler/src/wio_compiler/bytecode/verifier.cpp#L274).

### WBC1012

- Instruction contains an unknown opcode

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:363](../compiler/src/wio_compiler/bytecode/verifier.cpp#L363).

### WBC1013

- Instruction references an invalid type id

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:366](../compiler/src/wio_compiler/bytecode/verifier.cpp#L366).

### WBC1014

- Instruction references an invalid constant id

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:369](../compiler/src/wio_compiler/bytecode/verifier.cpp#L369).

### WBC1015

- Instruction references an invalid string id

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:372](../compiler/src/wio_compiler/bytecode/verifier.cpp#L372).

### WBC1016

- Interpolation segment references an invalid string id

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:412](../compiler/src/wio_compiler/bytecode/verifier.cpp#L412).

### WBC1017

- Branch target references a block outside the function

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:418](../compiler/src/wio_compiler/bytecode/verifier.cpp#L418).

### WBC1018

- Nominal base references an invalid type id

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:125](../compiler/src/wio_compiler/bytecode/verifier.cpp#L125).

### WBC1019

- Field layout references invalid string or type metadata

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:130](../compiler/src/wio_compiler/bytecode/verifier.cpp#L130).

### WBC1020

- Method layout references invalid string, type, or function metadata

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:135](../compiler/src/wio_compiler/bytecode/verifier.cpp#L135).

### WBC1021

- Method parameter references an invalid type id

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:139](../compiler/src/wio_compiler/bytecode/verifier.cpp#L139).

### WBC1022

- Dispatch entry on type #{dynamic context} references invalid contract #{dynamic context} or implementation #{dynamic context}

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:146](../compiler/src/wio_compiler/bytecode/verifier.cpp#L146).

### WBC1023

- Type lifecycle or enum metadata contains an invalid id

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:165](../compiler/src/wio_compiler/bytecode/verifier.cpp#L165).

### WBC1024

- Enum case references an invalid string id

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:169](../compiler/src/wio_compiler/bytecode/verifier.cpp#L169).

### WBC1025

- Native type binding references an invalid string id

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:173](../compiler/src/wio_compiler/bytecode/verifier.cpp#L173).

### WBC1026

- Function capture count does not match its capture table

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:196](../compiler/src/wio_compiler/bytecode/verifier.cpp#L196).

### WBC1027

- Closure capture references invalid string or type metadata

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:200](../compiler/src/wio_compiler/bytecode/verifier.cpp#L200).

### WBC1028

- Native binding references invalid string or type metadata

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:207](../compiler/src/wio_compiler/bytecode/verifier.cpp#L207).

### WBC1029

- Native parameter references an invalid type id

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:211](../compiler/src/wio_compiler/bytecode/verifier.cpp#L211).

### WBC1030

- Native template argument references an invalid type id

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:265](../compiler/src/wio_compiler/bytecode/verifier.cpp#L265).

### WBC1031

- Coroutine layout does not match its async task return type

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:328](../compiler/src/wio_compiler/bytecode/verifier.cpp#L328).

### WBC1032

- Coroutine frame slot is not dense or does not match a live typed value

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:336](../compiler/src/wio_compiler/bytecode/verifier.cpp#L336).

### WBC1033

- Coroutine state references invalid block or type metadata

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:346](../compiler/src/wio_compiler/bytecode/verifier.cpp#L346).

### WBC1034

- External/template declarations must have no code and defined functions must have code

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:282](../compiler/src/wio_compiler/bytecode/verifier.cpp#L282).

### WBC1035

- Function value ids must be valid, typed, and unique

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:300](../compiler/src/wio_compiler/bytecode/verifier.cpp#L300).

### WBC1036

- Function parameter references an invalid name

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:302](../compiler/src/wio_compiler/bytecode/verifier.cpp#L302).

### WBC1037

- Value-producing instruction requires a unique typed result

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:316](../compiler/src/wio_compiler/bytecode/verifier.cpp#L316).

### WBC1038

- Defined bytecode block cannot be empty

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:355](../compiler/src/wio_compiler/bytecode/verifier.cpp#L355).

### WBC1039

- Every block must end in exactly one final terminator

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:375](../compiler/src/wio_compiler/bytecode/verifier.cpp#L375).

### WBC1040

- Instruction result shape does not match its opcode

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:385](../compiler/src/wio_compiler/bytecode/verifier.cpp#L385).

### WBC1041

- Non-value instruction cannot carry a result type

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:389](../compiler/src/wio_compiler/bytecode/verifier.cpp#L389).

### WBC1042

- Pack expansion metadata must align with instruction operands

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:393](../compiler/src/wio_compiler/bytecode/verifier.cpp#L393).

### WBC1043

- Instruction references an invalid callee

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:396](../compiler/src/wio_compiler/bytecode/verifier.cpp#L396).

### WBC1044

- Instruction references an invalid global

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:401](../compiler/src/wio_compiler/bytecode/verifier.cpp#L401).

### WBC1045

- Instruction references an undefined function value

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:406](../compiler/src/wio_compiler/bytecode/verifier.cpp#L406).

### WBC1046

- Branch argument count does not match target parameters

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:424](../compiler/src/wio_compiler/bytecode/verifier.cpp#L424).

### WBC1047

- Branch argument type does not match target parameter

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:433](../compiler/src/wio_compiler/bytecode/verifier.cpp#L433).

### WBC1048

- Control-flow instruction has an invalid operand or target shape

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:446](../compiler/src/wio_compiler/bytecode/verifier.cpp#L446).

### WBC1049

- Function reference does not match a known callable declaration

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:689](../compiler/src/wio_compiler/bytecode/verifier.cpp#L689).

### WBC1050

- Closure creation does not match its callable capture layout

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:718](../compiler/src/wio_compiler/bytecode/verifier.cpp#L718).

### WBC1051

- Indirect call does not match its function value signature

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:751](../compiler/src/wio_compiler/bytecode/verifier.cpp#L751).

### WBC1052

- Dispatch entry does not match an object/interface contract method slot

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:156](../compiler/src/wio_compiler/bytecode/verifier.cpp#L156).

### WBC1054

- Object cast table references a non-nominal or invalid type

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:161](../compiler/src/wio_compiler/bytecode/verifier.cpp#L161).

### WBC1055

- Dynamic call does not match its receiver, contract, slot, or declaration

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:789](../compiler/src/wio_compiler/bytecode/verifier.cpp#L789).

### WBC1056

- Object cast or type-test has invalid source or target metadata

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:808](../compiler/src/wio_compiler/bytecode/verifier.cpp#L808).

### WBC1057

- Object identity comparison has invalid operand or operator metadata

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:818](../compiler/src/wio_compiler/bytecode/verifier.cpp#L818).

### WBC1058

- Async bytecode function is missing its coroutine layout

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:350](../compiler/src/wio_compiler/bytecode/verifier.cpp#L350).

### WBC1059

- Cancellation check must precede its matching coroutine suspension

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:828](../compiler/src/wio_compiler/bytecode/verifier.cpp#L828).

### WBC1060

- Coroutine suspension does not match its canonical state and resume edge

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:860](../compiler/src/wio_compiler/bytecode/verifier.cpp#L860).

### WBC1061

- Coroutine resume does not materialize its canonical state payload

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:871](../compiler/src/wio_compiler/bytecode/verifier.cpp#L871).

### WBC1062

- Coroutine completion does not match its async payload type

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:884](../compiler/src/wio_compiler/bytecode/verifier.cpp#L884).

### WBC1063

- Executable bytecode cannot contain an unresolved generic constant

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:496](../compiler/src/wio_compiler/bytecode/verifier.cpp#L496).

### WBC1064

- Enum constant does not match a declared enum/flagset case

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:512](../compiler/src/wio_compiler/bytecode/verifier.cpp#L512).

### WBC1065

- Any box does not preserve its materialized payload type

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:524](../compiler/src/wio_compiler/bytecode/verifier.cpp#L524).

### WBC1066

- Any test/cast has invalid source, target, or result metadata

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:537](../compiler/src/wio_compiler/bytecode/verifier.cpp#L537).

### WBC1067

- Nullable wrap does not match its payload type

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:550](../compiler/src/wio_compiler/bytecode/verifier.cpp#L550).

### WBC1068

- Nullable unwrap does not match its payload result type

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:561](../compiler/src/wio_compiler/bytecode/verifier.cpp#L561).

### WBC1069

- Variant operation does not match its option/result layout

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:585](../compiler/src/wio_compiler/bytecode/verifier.cpp#L585).

### WBC1070

- Iterator creation has invalid source or signature metadata

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:626](../compiler/src/wio_compiler/bytecode/verifier.cpp#L626).

### WBC1071

- Iterator operation has invalid cursor or result metadata

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:647](../compiler/src/wio_compiler/bytecode/verifier.cpp#L647).

### WBC1072

- Result operation does not match its canonical result layout

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:676](../compiler/src/wio_compiler/bytecode/verifier.cpp#L676).

### WBC1073

- Native invocation does not match its external ABI declaration

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:491](../compiler/src/wio_compiler/bytecode/verifier.cpp#L491).

### WBC1074

- Native parameter has an invalid ABI mode or no stable type identity ({dynamic context}, type #{dynamic context}, marshalling {dynamic context})
- Native result has an invalid ABI mode or no stable type identity

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:242](../compiler/src/wio_compiler/bytecode/verifier.cpp#L242), [compiler/src/wio_compiler/bytecode/verifier.cpp:261](../compiler/src/wio_compiler/bytecode/verifier.cpp#L261).

### WBC1080

- Template declaration must have an open signature and no runtime binding or coroutine

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:291](../compiler/src/wio_compiler/bytecode/verifier.cpp#L291).

### WBC1100

- Module import contains invalid identity or string metadata

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:895](../compiler/src/wio_compiler/bytecode/verifier.cpp#L895).

### WBC1101

- Module import symbol references an invalid string id

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:899](../compiler/src/wio_compiler/bytecode/verifier.cpp#L899).

### WBC1102

- Module export contains invalid identity, symbol, or target metadata

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:908](../compiler/src/wio_compiler/bytecode/verifier.cpp#L908).

### WBC1103

- Module export parameter references an invalid type id

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:912](../compiler/src/wio_compiler/bytecode/verifier.cpp#L912).

### WBC1104

- Attribute application contains invalid identity or target metadata

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:920](../compiler/src/wio_compiler/bytecode/verifier.cpp#L920).

### WBC1105

- Attribute argument contains invalid string or type metadata

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:924](../compiler/src/wio_compiler/bytecode/verifier.cpp#L924).

### WBC1106

- Attribute processor contains invalid identity or hook metadata

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:932](../compiler/src/wio_compiler/bytecode/verifier.cpp#L932).

### WBC1107

- Reflection descriptor contains invalid identity or type metadata

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:938](../compiler/src/wio_compiler/bytecode/verifier.cpp#L938).

### WBC1108

- Reflection generic parameter references an invalid string id

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:942](../compiler/src/wio_compiler/bytecode/verifier.cpp#L942).

### WBC1109

- Reflected field contains invalid identity or type metadata

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:947](../compiler/src/wio_compiler/bytecode/verifier.cpp#L947).

### WBC1110

- Reflected method contains invalid identity or callable metadata

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:953](../compiler/src/wio_compiler/bytecode/verifier.cpp#L953).

### WBC1111

- Reflected case contains invalid identity or string metadata

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:958](../compiler/src/wio_compiler/bytecode/verifier.cpp#L958).

### WBC1112

- System descriptor contains invalid lifecycle metadata

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:965](../compiler/src/wio_compiler/bytecode/verifier.cpp#L965).

### WBC1113

- Application descriptor contains invalid lifecycle metadata

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:974](../compiler/src/wio_compiler/bytecode/verifier.cpp#L974).

### WBC1114

- Application stage contains invalid identity or string metadata

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:978](../compiler/src/wio_compiler/bytecode/verifier.cpp#L978).

### WBC1115

- Application stage run contains invalid target metadata

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:983](../compiler/src/wio_compiler/bytecode/verifier.cpp#L983).

### WBC1116

- Application resource contains invalid string or type metadata

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:987](../compiler/src/wio_compiler/bytecode/verifier.cpp#L987).

### WBC1117

- Module lifecycle references an invalid function id

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:996](../compiler/src/wio_compiler/bytecode/verifier.cpp#L996).

### WBC1118

- SDK call table identity or export cardinality is invalid

Definitions: [compiler/src/wio_compiler/bytecode/verifier.cpp:999](../compiler/src/wio_compiler/bytecode/verifier.cpp#L999).

## WVM

### WVM1000

- The virtual machine has no loaded module
- Timer scheduling requires a loaded VM program

Definitions: [vm/src/machine.cpp:1633](../vm/src/machine.cpp#L1633), [vm/src/machine.cpp:2811](../vm/src/machine.cpp#L2811), [vm/src/machine.cpp:3133](../vm/src/machine.cpp#L3133).

### WVM1001

- Bytecode verification failed ({dynamic context}): {dynamic context}

Definitions: [vm/src/machine.cpp:1538](../vm/src/machine.cpp#L1538).

### WVM1002

- Entry function id is outside the module

Definitions: [vm/src/machine.cpp:1637](../vm/src/machine.cpp#L1637), [vm/src/machine.cpp:3139](../vm/src/machine.cpp#L3139).

### WVM1003

- External function requires the Sprint 20 native bridge

Definitions: [vm/src/machine.cpp:3162](../vm/src/machine.cpp#L3162).

### WVM1004

- Call argument count does not match the function signature

Definitions: [vm/src/machine.cpp:3165](../vm/src/machine.cpp#L3165).

### WVM1005

- VM call depth limit exceeded

Definitions: [vm/src/machine.cpp:3167](../vm/src/machine.cpp#L3167).

### WVM1006

- Function register limit exceeded

Definitions: [vm/src/machine.cpp:3170](../vm/src/machine.cpp#L3170).

### WVM1007

- Instruction cursor escaped its basic block

Definitions: [vm/src/machine.cpp:3307](../vm/src/machine.cpp#L3307).

### WVM1008

- VM instruction limit exceeded

Definitions: [vm/src/machine.cpp:3309](../vm/src/machine.cpp#L3309).

### WVM1009

- Constant result register is invalid
- Default-value result register is invalid
- Array result register is invalid
- Dictionary result register is invalid
- Enum constant result register is invalid
- Enum intrinsic result register is invalid
- Intrinsic result register is invalid
- Any box result register is invalid
- Any type-test result register is invalid
- Any checked-cast result register is invalid
- Nullable wrap result register is invalid
- Nullable unwrap result register is invalid
- Variant-test result register is invalid
- Result error-test register is invalid
- Variant payload result register is invalid
- Range iterator result register is invalid
- Container iterator result register is invalid
- Iterator has-next result register is invalid
- Iterator index result register is invalid
- Iterator reference result register is invalid
- Iterator value result register is invalid
- Aggregate result register is invalid

Definitions: [vm/src/machine.cpp:3423](../vm/src/machine.cpp#L3423), [vm/src/machine.cpp:3449](../vm/src/machine.cpp#L3449), [vm/src/machine.cpp:3649](../vm/src/machine.cpp#L3649), [vm/src/machine.cpp:3690](../vm/src/machine.cpp#L3690), [vm/src/machine.cpp:3782](../vm/src/machine.cpp#L3782), [vm/src/machine.cpp:3868](../vm/src/machine.cpp#L3868), [vm/src/machine.cpp:3965](../vm/src/machine.cpp#L3965), [vm/src/machine.cpp:3983](../vm/src/machine.cpp#L3983), [vm/src/machine.cpp:4410](../vm/src/machine.cpp#L4410), [vm/src/machine.cpp:4427](../vm/src/machine.cpp#L4427), [vm/src/machine.cpp:4441](../vm/src/machine.cpp#L4441), [vm/src/machine.cpp:4494](../vm/src/machine.cpp#L4494), [vm/src/machine.cpp:4511](../vm/src/machine.cpp#L4511), [vm/src/machine.cpp:4522](../vm/src/machine.cpp#L4522), [vm/src/machine.cpp:4535](../vm/src/machine.cpp#L4535), [vm/src/machine.cpp:4556](../vm/src/machine.cpp#L4556), [vm/src/machine.cpp:4561](../vm/src/machine.cpp#L4561), [vm/src/machine.cpp:4573](../vm/src/machine.cpp#L4573), [vm/src/machine.cpp:4598](../vm/src/machine.cpp#L4598), [vm/src/machine.cpp:4619](../vm/src/machine.cpp#L4619), [vm/src/machine.cpp:4691](../vm/src/machine.cpp#L4691), [vm/src/machine.cpp:4718](../vm/src/machine.cpp#L4718), [vm/src/machine.cpp:4784](../vm/src/machine.cpp#L4784), [vm/src/machine.cpp:4787](../vm/src/machine.cpp#L4787), [vm/src/machine.cpp:5209](../vm/src/machine.cpp#L5209).

### WVM1010

- Default value is not implemented for this bytecode type

Definitions: [vm/src/machine.cpp:3447](../vm/src/machine.cpp#L3447).

### WVM1011

- Unary instruction reads an unavailable value

Definitions: [vm/src/machine.cpp:3457](../vm/src/machine.cpp#L3457).

### WVM1012

- Unary operator is invalid for the runtime value kind

Definitions: [vm/src/machine.cpp:3472](../vm/src/machine.cpp#L3472).

### WVM1013

- Binary instruction requires two available values of the same kind

Definitions: [vm/src/machine.cpp:3492](../vm/src/machine.cpp#L3492).

### WVM1014

- Integer division by zero

Definitions: [vm/src/machine.cpp:3512](../vm/src/machine.cpp#L3512), [vm/src/machine.cpp:3565](../vm/src/machine.cpp#L3565).

### WVM1015

- Signed integer division overflow

Definitions: [vm/src/machine.cpp:3514](../vm/src/machine.cpp#L3514).

### WVM1016

- Integer shift count is outside [0, 63]

Definitions: [vm/src/machine.cpp:3526](../vm/src/machine.cpp#L3526), [vm/src/machine.cpp:3577](../vm/src/machine.cpp#L3577).

### WVM1017

- Signed integer binary operator is not implemented

Definitions: [vm/src/machine.cpp:3538](../vm/src/machine.cpp#L3538).

### WVM1018

- Unsigned integer binary operator is not implemented

Definitions: [vm/src/machine.cpp:3581](../vm/src/machine.cpp#L3581).

### WVM1019

- Floating-point binary operator is not implemented

Definitions: [vm/src/machine.cpp:3600](../vm/src/machine.cpp#L3600).

### WVM1020

- String binary operator is not implemented

Definitions: [vm/src/machine.cpp:3613](../vm/src/machine.cpp#L3613).

### WVM1021

- Binary operator is invalid for the runtime value kind

Definitions: [vm/src/machine.cpp:3630](../vm/src/machine.cpp#L3630).

### WVM1022

- Range instruction has an invalid operand shape

Definitions: [vm/src/machine.cpp:4853](../vm/src/machine.cpp#L4853).

### WVM1023

- Range instruction reads incompatible values

Definitions: [vm/src/machine.cpp:4858](../vm/src/machine.cpp#L4858).

### WVM1024

- Range containment requires numeric runtime values

Definitions: [vm/src/machine.cpp:4874](../vm/src/machine.cpp#L4874).

### WVM1025

- Conversion reads an unavailable value

Definitions: [vm/src/machine.cpp:4883](../vm/src/machine.cpp#L4883).

### WVM1026

- Numeric conversion is out of range or invalid

Definitions: [vm/src/machine.cpp:4898](../vm/src/machine.cpp#L4898), [vm/src/machine.cpp:4912](../vm/src/machine.cpp#L4912), [vm/src/machine.cpp:4925](../vm/src/machine.cpp#L4925).

### WVM1027

- Conversion target is not a numeric bytecode type

Definitions: [vm/src/machine.cpp:4930](../vm/src/machine.cpp#L4930).

### WVM1028

- Call target is outside the module

Definitions: [vm/src/machine.cpp:6324](../vm/src/machine.cpp#L6324).

### WVM1029

- Call reads an unavailable argument value
- Dynamic call reads an unavailable argument value

Definitions: [vm/src/machine.cpp:6331](../vm/src/machine.cpp#L6331), [vm/src/machine.cpp:6379](../vm/src/machine.cpp#L6379).

### WVM1030

- Conditional jump requires an available bool value

Definitions: [vm/src/machine.cpp:6570](../vm/src/machine.cpp#L6570).

### WVM1031

- Branch target block does not exist

Definitions: [vm/src/machine.cpp:6577](../vm/src/machine.cpp#L6577).

### WVM1032

- Branch reads an unavailable argument value

Definitions: [vm/src/machine.cpp:6585](../vm/src/machine.cpp#L6585).

### WVM1033

- Return reads an unavailable value

Definitions: [vm/src/machine.cpp:6602](../vm/src/machine.cpp#L6602).

### WVM1034

- Call result register is invalid

Definitions: [vm/src/machine.cpp:3260](../vm/src/machine.cpp#L3260).

### WVM1035

- Execution reached an unreachable instruction

Definitions: [vm/src/machine.cpp:6610](../vm/src/machine.cpp#L6610).

### WVM1036

- Opcode is not implemented by this VM runtime slice: {dynamic context}

Definitions: [vm/src/machine.cpp:6612](../vm/src/machine.cpp#L6612).

### WVM1037

- VM exited without producing a result

Definitions: [vm/src/machine.cpp:6615](../vm/src/machine.cpp#L6615).

### WVM1038

- Local place result register is invalid

Definitions: [vm/src/machine.cpp:4942](../vm/src/machine.cpp#L4942).

### WVM1039

- Global place id or result register is invalid

Definitions: [vm/src/machine.cpp:4950](../vm/src/machine.cpp#L4950).

### WVM1040

- Place write requires an available place and source value

Definitions: [vm/src/machine.cpp:4961](../vm/src/machine.cpp#L4961).

### WVM1041

- Place can only be initialized once

Definitions: [vm/src/machine.cpp:4963](../vm/src/machine.cpp#L4963).

### WVM1042

- Store requires an initialized mutable place

Definitions: [vm/src/machine.cpp:4965](../vm/src/machine.cpp#L4965).

### WVM1043

- VM host cannot export a frame-local borrowed projection
- Load requires an initialized place
- Dynamic dispatch requires an initialized receiver place

Definitions: [vm/src/machine.cpp:3240](../vm/src/machine.cpp#L3240), [vm/src/machine.cpp:5039](../vm/src/machine.cpp#L5039), [vm/src/machine.cpp:6361](../vm/src/machine.cpp#L6361).

### WVM1044

- Borrow requires an available place or object value

Definitions: [vm/src/machine.cpp:5135](../vm/src/machine.cpp#L5135), [vm/src/machine.cpp:5141](../vm/src/machine.cpp#L5141).

### WVM1045

- Copy/retain reads an unavailable value

Definitions: [vm/src/machine.cpp:5329](../vm/src/machine.cpp#L5329).

### WVM1046

- Move requires an initialized place

Definitions: [vm/src/machine.cpp:5341](../vm/src/machine.cpp#L5341).

### WVM1047

- Value cleanup reads an unavailable value

Definitions: [vm/src/machine.cpp:5393](../vm/src/machine.cpp#L5393).

### WVM1048

- Place cleanup requires an initialized place

Definitions: [vm/src/machine.cpp:5411](../vm/src/machine.cpp#L5411).

### WVM1049

- Unicode text constant is not valid UTF-8

Definitions: [vm/src/machine.cpp:3421](../vm/src/machine.cpp#L3421).

### WVM1050

- Unicode text binary operator is not implemented

Definitions: [vm/src/machine.cpp:3627](../vm/src/machine.cpp#L3627).

### WVM1051

- Array creation result type is not an array

Definitions: [vm/src/machine.cpp:3638](../vm/src/machine.cpp#L3638).

### WVM1052

- Array creation reads an unavailable element

Definitions: [vm/src/machine.cpp:3645](../vm/src/machine.cpp#L3645).

### WVM1053

- Array length requires an available array

Definitions: [vm/src/machine.cpp:3657](../vm/src/machine.cpp#L3657).

### WVM1054

- Array index is invalid or outside the array bounds

Definitions: [vm/src/machine.cpp:3670](../vm/src/machine.cpp#L3670).

### WVM1055

- Interpolation segment shape is invalid

Definitions: [vm/src/machine.cpp:3738](../vm/src/machine.cpp#L3738).

### WVM1056

- Interpolation reads an unavailable value

Definitions: [vm/src/machine.cpp:3747](../vm/src/machine.cpp#L3747).

### WVM1057

- Unicode interpolation produced invalid UTF-8

Definitions: [vm/src/machine.cpp:3759](../vm/src/machine.cpp#L3759).

### WVM1058

- Intrinsic call has no available receiver

Definitions: [vm/src/machine.cpp:3879](../vm/src/machine.cpp#L3879).

### WVM1059

- Unicode text index is outside the scalar bounds

Definitions: [vm/src/text_intrinsics.cpp:178](../vm/src/text_intrinsics.cpp#L178).

### WVM1060

- Unicode text slice requires a non-negative start
- Unicode text slice requires a non-negative count

Definitions: [vm/src/text_intrinsics.cpp:166](../vm/src/text_intrinsics.cpp#L166), [vm/src/text_intrinsics.cpp:170](../vm/src/text_intrinsics.cpp#L170).

### WVM1061

- String intrinsic is not implemented: {dynamic context}
- Text intrinsic is not implemented: {dynamic context}
- Container or text intrinsic is not implemented: {dynamic context}

Definitions: [vm/src/machine.cpp:3960](../vm/src/machine.cpp#L3960), [vm/src/machine.cpp:3978](../vm/src/machine.cpp#L3978), [vm/src/machine.cpp:4402](../vm/src/machine.cpp#L4402).

### WVM1062

- Interpolation value has no VM string representation

Definitions: [vm/src/machine.cpp:3750](../vm/src/machine.cpp#L3750).

### WVM1063

- Field place reads an unavailable base

Definitions: [vm/src/machine.cpp:5063](../vm/src/machine.cpp#L5063).

### WVM1064

- Field place base is not initialized

Definitions: [vm/src/machine.cpp:5071](../vm/src/machine.cpp#L5071).

### WVM1065

- Object has no field layout
- Field owner is outside the concrete object hierarchy
- Field projection is outside the aggregate layout

Definitions: [vm/src/machine.cpp:5082](../vm/src/machine.cpp#L5082), [vm/src/machine.cpp:5086](../vm/src/machine.cpp#L5086), [vm/src/machine.cpp:5093](../vm/src/machine.cpp#L5093).

### WVM1066

- Array place requires an available base and non-negative index

Definitions: [vm/src/machine.cpp:5106](../vm/src/machine.cpp#L5106).

### WVM1067

- Array place base is not an initialized array

Definitions: [vm/src/machine.cpp:5114](../vm/src/machine.cpp#L5114).

### WVM1068

- Array place index is outside the array bounds

Definitions: [vm/src/machine.cpp:5124](../vm/src/machine.cpp#L5124).

### WVM1069

- Aggregate construction references an invalid type

Definitions: [vm/src/machine.cpp:5149](../vm/src/machine.cpp#L5149).

### WVM1070

- Aggregate construction kind does not match its result type

Definitions: [vm/src/machine.cpp:5154](../vm/src/machine.cpp#L5154).

### WVM1071

- Component aggregate has more arguments than declared fields
- Component aggregate reads an unavailable field argument
- Aggregate constructor reads an unavailable argument

Definitions: [vm/src/machine.cpp:5217](../vm/src/machine.cpp#L5217), [vm/src/machine.cpp:5222](../vm/src/machine.cpp#L5222), [vm/src/machine.cpp:5249](../vm/src/machine.cpp#L5249).

### WVM1072

- Reference release reads an unavailable value

Definitions: [vm/src/machine.cpp:5366](../vm/src/machine.cpp#L5366).

### WVM1073

- Reference release requires an owned object or callable handle

Definitions: [vm/src/machine.cpp:5371](../vm/src/machine.cpp#L5371).

### WVM1074

- Construction chain references an invalid function
- Construction continuation references an invalid function
- Component construction references an invalid lifecycle function

Definitions: [vm/src/machine.cpp:3276](../vm/src/machine.cpp#L3276), [vm/src/machine.cpp:3290](../vm/src/machine.cpp#L3290), [vm/src/machine.cpp:5257](../vm/src/machine.cpp#L5257).

### WVM1078

- Dictionary creation has an invalid result type or entry shape

Definitions: [vm/src/machine.cpp:3678](../vm/src/machine.cpp#L3678).

### WVM1079

- Dictionary creation reads an unavailable key or value

Definitions: [vm/src/machine.cpp:3685](../vm/src/machine.cpp#L3685).

### WVM1080

- Dictionary key was not found

Definitions: [vm/src/machine.cpp:3700](../vm/src/machine.cpp#L3700), [vm/src/machine.cpp:3728](../vm/src/machine.cpp#L3728), [vm/src/machine.cpp:4241](../vm/src/machine.cpp#L4241).

### WVM1081

- Dictionary place requires an available base and key

Definitions: [vm/src/machine.cpp:3710](../vm/src/machine.cpp#L3710).

### WVM1082

- Dictionary place base is not an initialized dictionary

Definitions: [vm/src/machine.cpp:3718](../vm/src/machine.cpp#L3718).

### WVM1083

- Array Contains reads an unavailable value

Definitions: [vm/src/machine.cpp:4000](../vm/src/machine.cpp#L4000).

### WVM1084

- Dictionary GetOr reads unavailable operands

Definitions: [vm/src/machine.cpp:4251](../vm/src/machine.cpp#L4251).

### WVM1085

- Dictionary TryGet requires a mutable initialized output place

Definitions: [vm/src/machine.cpp:4263](../vm/src/machine.cpp#L4263).

### WVM1086

- Dictionary Set requires a mutable receiver and available operands

Definitions: [vm/src/machine.cpp:4276](../vm/src/machine.cpp#L4276).

### WVM1087

- Dictionary GetOrAdd requires a mutable receiver and operands

Definitions: [vm/src/machine.cpp:4286](../vm/src/machine.cpp#L4286).

### WVM1088

- Dictionary Merge requires another dictionary

Definitions: [vm/src/machine.cpp:4314](../vm/src/machine.cpp#L4314).

### WVM1089

- Dictionary Extend requires a mutable receiver and dictionary

Definitions: [vm/src/machine.cpp:4326](../vm/src/machine.cpp#L4326).

### WVM1090

- Dictionary Clear requires a mutable receiver

Definitions: [vm/src/machine.cpp:4343](../vm/src/machine.cpp#L4343).

### WVM1091

- Dictionary Remove requires a mutable receiver and key

Definitions: [vm/src/machine.cpp:4363](../vm/src/machine.cpp#L4363).

### WVM1092

- Ordered dictionary endpoint requires a non-empty ordered dictionary

Definitions: [vm/src/machine.cpp:4384](../vm/src/machine.cpp#L4384).

### WVM1093

- Ordered dictionary bound lookup has invalid operands

Definitions: [vm/src/machine.cpp:4396](../vm/src/machine.cpp#L4396).

### WVM1094

- Array index search reads an unavailable value

Definitions: [vm/src/machine.cpp:4014](../vm/src/machine.cpp#L4014).

### WVM1095

- Array endpoint intrinsic requires a non-empty array

Definitions: [vm/src/machine.cpp:4044](../vm/src/machine.cpp#L4044).

### WVM1096

- Array intrinsic index is outside the array bounds

Definitions: [vm/src/machine.cpp:4053](../vm/src/machine.cpp#L4053).

### WVM1097

- Array GetOr reads an unavailable fallback

Definitions: [vm/src/machine.cpp:4062](../vm/src/machine.cpp#L4062).

### WVM1098

- Array Slice requires non-negative start and count operands

Definitions: [vm/src/machine.cpp:4077](../vm/src/machine.cpp#L4077).

### WVM1099

- Array Take/Skip requires a non-negative count

Definitions: [vm/src/machine.cpp:4085](../vm/src/machine.cpp#L4085).

### WVM1100

- Array Concat requires another array

Definitions: [vm/src/machine.cpp:4094](../vm/src/machine.cpp#L4094).

### WVM1101

- Array Join requires a string separator

Definitions: [vm/src/machine.cpp:4108](../vm/src/machine.cpp#L4108).

### WVM1102

- Array Join requires string elements

Definitions: [vm/src/machine.cpp:4114](../vm/src/machine.cpp#L4114).

### WVM1103

- Array Push requires a mutable receiver and available value

Definitions: [vm/src/machine.cpp:4126](../vm/src/machine.cpp#L4126).

### WVM1104

- Array Pop requires a mutable non-empty array

Definitions: [vm/src/machine.cpp:4133](../vm/src/machine.cpp#L4133).

### WVM1105

- Array Insert index is invalid or receiver is not mutable

Definitions: [vm/src/machine.cpp:4141](../vm/src/machine.cpp#L4141).

### WVM1106

- Array RemoveAt index is invalid or receiver is not mutable

Definitions: [vm/src/machine.cpp:4148](../vm/src/machine.cpp#L4148).

### WVM1107

- Array Remove requires a mutable receiver and available value

Definitions: [vm/src/machine.cpp:4155](../vm/src/machine.cpp#L4155).

### WVM1108

- Array Clear requires a mutable receiver

Definitions: [vm/src/machine.cpp:4162](../vm/src/machine.cpp#L4162).

### WVM1109

- Array Extend requires a mutable receiver and another array

Definitions: [vm/src/machine.cpp:4170](../vm/src/machine.cpp#L4170).

### WVM1110

- Array Reserve requires a mutable receiver and valid capacity

Definitions: [vm/src/machine.cpp:4177](../vm/src/machine.cpp#L4177).

### WVM1111

- Array ShrinkToFit requires a mutable receiver

Definitions: [vm/src/machine.cpp:4183](../vm/src/machine.cpp#L4183).

### WVM1112

- Array Fill requires a mutable receiver and available value

Definitions: [vm/src/machine.cpp:4191](../vm/src/machine.cpp#L4191).

### WVM1113

- Array Reverse requires a mutable receiver

Definitions: [vm/src/machine.cpp:4198](../vm/src/machine.cpp#L4198).

### WVM1114

- Array Sort requires a mutable array of orderable values

Definitions: [vm/src/machine.cpp:4205](../vm/src/machine.cpp#L4205).

### WVM1115

- Array Sorted requires orderable values

Definitions: [vm/src/machine.cpp:4212](../vm/src/machine.cpp#L4212).

### WVM1116

- String intrinsic receiver is not a string

Definitions: [vm/src/string_intrinsics.cpp:241](../vm/src/string_intrinsics.cpp#L241).

### WVM1117

- String Contains requires a string or byte-sized char

Definitions: [vm/src/string_intrinsics.cpp:255](../vm/src/string_intrinsics.cpp#L255).

### WVM1118

- String prefix/suffix query requires a string

Definitions: [vm/src/string_intrinsics.cpp:261](../vm/src/string_intrinsics.cpp#L261).

### WVM1119

- String index query requires a string or byte-sized char

Definitions: [vm/src/string_intrinsics.cpp:277](../vm/src/string_intrinsics.cpp#L277).

### WVM1120

- String endpoint intrinsic requires a non-empty string

Definitions: [vm/src/string_intrinsics.cpp:284](../vm/src/string_intrinsics.cpp#L284).

### WVM1121

- String indexing requires a non-negative index

Definitions: [vm/src/string_intrinsics.cpp:293](../vm/src/string_intrinsics.cpp#L293).

### WVM1122

- String index is outside the byte bounds

Definitions: [vm/src/string_intrinsics.cpp:299](../vm/src/string_intrinsics.cpp#L299).

### WVM1123

- String slicing requires a non-negative index or count
- String Slice requires a non-negative count

Definitions: [vm/src/string_intrinsics.cpp:308](../vm/src/string_intrinsics.cpp#L308), [vm/src/string_intrinsics.cpp:321](../vm/src/string_intrinsics.cpp#L321).

### WVM1124

- String replacement requires old and new string values

Definitions: [vm/src/string_intrinsics.cpp:343](../vm/src/string_intrinsics.cpp#L343), [vm/src/string_intrinsics.cpp:510](../vm/src/string_intrinsics.cpp#L510).

### WVM1125

- String Repeat count exceeds runtime limits

Definitions: [vm/src/string_intrinsics.cpp:352](../vm/src/string_intrinsics.cpp#L352).

### WVM1126

- String Split requires a separator

Definitions: [vm/src/string_intrinsics.cpp:366](../vm/src/string_intrinsics.cpp#L366).

### WVM1127

- String padding requires a width and optional byte-sized char

Definitions: [vm/src/string_intrinsics.cpp:411](../vm/src/string_intrinsics.cpp#L411).

### WVM1128

- String integer conversion has invalid input or base
- String integer conversion failed
- String unsigned conversion is out of range
- String signed conversion is out of range
- String integer conversion base is invalid

Definitions: [vm/src/string_intrinsics.cpp:195](../vm/src/string_intrinsics.cpp#L195), [vm/src/string_intrinsics.cpp:200](../vm/src/string_intrinsics.cpp#L200), [vm/src/string_intrinsics.cpp:211](../vm/src/string_intrinsics.cpp#L211), [vm/src/string_intrinsics.cpp:219](../vm/src/string_intrinsics.cpp#L219), [vm/src/string_intrinsics.cpp:429](../vm/src/string_intrinsics.cpp#L429).

### WVM1129

- String f32 conversion failed
- String floating-point conversion failed

Definitions: [vm/src/string_intrinsics.cpp:446](../vm/src/string_intrinsics.cpp#L446), [vm/src/string_intrinsics.cpp:454](../vm/src/string_intrinsics.cpp#L454).

### WVM1130

- String boolean conversion failed

Definitions: [vm/src/string_intrinsics.cpp:466](../vm/src/string_intrinsics.cpp#L466).

### WVM1131

- String Append requires a string suffix

Definitions: [vm/src/string_intrinsics.cpp:474](../vm/src/string_intrinsics.cpp#L474).

### WVM1132

- String Push requires a byte-sized char

Definitions: [vm/src/string_intrinsics.cpp:481](../vm/src/string_intrinsics.cpp#L481).

### WVM1133

- String Insert requires an index and fragment

Definitions: [vm/src/string_intrinsics.cpp:489](../vm/src/string_intrinsics.cpp#L489).

### WVM1134

- String Erase requires index and count operands

Definitions: [vm/src/string_intrinsics.cpp:497](../vm/src/string_intrinsics.cpp#L497).

### WVM1135

- String mutation requires a mutable receiver

Definitions: [vm/src/string_intrinsics.cpp:230](../vm/src/string_intrinsics.cpp#L230).

### WVM1136

- Text intrinsic receiver is not Unicode text

Definitions: [vm/src/text_intrinsics.cpp:152](../vm/src/text_intrinsics.cpp#L152).

### WVM1137

- Unicode text query requires another text value

Definitions: [vm/src/text_intrinsics.cpp:185](../vm/src/text_intrinsics.cpp#L185).

### WVM1138

- Grapheme slicing requires non-negative start and count values

Definitions: [vm/src/text_intrinsics.cpp:204](../vm/src/text_intrinsics.cpp#L204).

### WVM1139

- Function reference requires a known function and callable type

Definitions: [vm/src/machine.cpp:5431](../vm/src/machine.cpp#L5431).

### WVM1140

- Function reference result register is invalid

Definitions: [vm/src/machine.cpp:5433](../vm/src/machine.cpp#L5433).

### WVM1141

- Closure creation does not match a known callable capture layout

Definitions: [vm/src/machine.cpp:5442](../vm/src/machine.cpp#L5442).

### WVM1142

- Closure creation reads an unavailable capture value

Definitions: [vm/src/machine.cpp:5450](../vm/src/machine.cpp#L5450).

### WVM1143

- Reference capture requires a live borrowed place

Definitions: [vm/src/machine.cpp:5459](../vm/src/machine.cpp#L5459).

### WVM1144

- Retained-self capture requires a live component or object

Definitions: [vm/src/machine.cpp:5477](../vm/src/machine.cpp#L5477).

### WVM1145

- Closure creation contains an unknown capture kind

Definitions: [vm/src/machine.cpp:5482](../vm/src/machine.cpp#L5482).

### WVM1146

- Closure result register is invalid
- Native callable result register is invalid

Definitions: [vm/src/machine.cpp:5486](../vm/src/machine.cpp#L5486), [vm/src/machine.cpp:5513](../vm/src/machine.cpp#L5513).

### WVM1147

- Indirect call requires an available function value

Definitions: [vm/src/machine.cpp:5494](../vm/src/machine.cpp#L5494).

### WVM1148

- Indirect call target is outside the module

Definitions: [vm/src/machine.cpp:5519](../vm/src/machine.cpp#L5519).

### WVM1149

- Indirect call lost a closure capture

Definitions: [vm/src/machine.cpp:5527](../vm/src/machine.cpp#L5527).

### WVM1150

- Native indirect call reads an unavailable argument
- Indirect call reads an unavailable argument value

Definitions: [vm/src/machine.cpp:5505](../vm/src/machine.cpp#L5505), [vm/src/machine.cpp:5534](../vm/src/machine.cpp#L5534).

### WVM1151

- Object cast requires a live object value

Definitions: [vm/src/machine.cpp:5563](../vm/src/machine.cpp#L5563).

### WVM1152

Definition: Upcast target is incompatible with the concrete object type.

Definitions: [vm/src/machine.cpp:5565](../vm/src/machine.cpp#L5565).

### WVM1153

Definition: Checked object cast failed for the concrete object type.

Definitions: [vm/src/machine.cpp:5565](../vm/src/machine.cpp#L5565).

### WVM1154

- Object cast result register is invalid

Definitions: [vm/src/machine.cpp:5577](../vm/src/machine.cpp#L5577).

### WVM1155

- Object type-test result register is invalid

Definitions: [vm/src/machine.cpp:5587](../vm/src/machine.cpp#L5587).

### WVM1156

- Object identity comparison requires two operands

Definitions: [vm/src/machine.cpp:5594](../vm/src/machine.cpp#L5594).

### WVM1157

- Object identity comparison reads an unavailable value

Definitions: [vm/src/machine.cpp:5598](../vm/src/machine.cpp#L5598).

### WVM1158

- Object identity comparison requires object or null operands

Definitions: [vm/src/machine.cpp:5605](../vm/src/machine.cpp#L5605).

### WVM1159

- Object identity comparison has an invalid comparison operator

Definitions: [vm/src/machine.cpp:5612](../vm/src/machine.cpp#L5612).

### WVM1160

- Object identity comparison result register is invalid

Definitions: [vm/src/machine.cpp:5614](../vm/src/machine.cpp#L5614).

### WVM1161

- Dynamic dispatch requires a live object receiver

Definitions: [vm/src/machine.cpp:6365](../vm/src/machine.cpp#L6365).

### WVM1162

- Concrete object type has no implementation for the requested method slot

Definitions: [vm/src/machine.cpp:6369](../vm/src/machine.cpp#L6369).

### WVM1163

- Dynamic dispatch resolved outside the module function table

Definitions: [vm/src/machine.cpp:6371](../vm/src/machine.cpp#L6371).

### WVM1164

- Wait requires an async task value

Definitions: [vm/src/machine.cpp:2855](../vm/src/machine.cpp#L2855).

### WVM1165

- Async task was cancelled during machine shutdown
- Async task was cancelled
- Awaited async task was cancelled
- Async task was cancelled before its continuation resumed
- Async task was cancelled at a cooperative suspension point

Definitions: [vm/src/machine.cpp:1505](../vm/src/machine.cpp#L1505), [vm/src/machine.cpp:2446](../vm/src/machine.cpp#L2446), [vm/src/machine.cpp:2477](../vm/src/machine.cpp#L2477), [vm/src/machine.cpp:2869](../vm/src/machine.cpp#L2869), [vm/src/machine.cpp:2884](../vm/src/machine.cpp#L2884), [vm/src/machine.cpp:2913](../vm/src/machine.cpp#L2913), [vm/src/machine.cpp:2925](../vm/src/machine.cpp#L2925), [vm/src/machine.cpp:2945](../vm/src/machine.cpp#L2945), [vm/src/machine.cpp:3009](../vm/src/machine.cpp#L3009), [vm/src/machine.cpp:3071](../vm/src/machine.cpp#L3071), [vm/src/machine.cpp:6403](../vm/src/machine.cpp#L6403), [vm/src/machine.cpp:6439](../vm/src/machine.cpp#L6439).

### WVM1166

- Async task is already running or has an invalid state

Definitions: [vm/src/machine.cpp:2888](../vm/src/machine.cpp#L2888).

### WVM1167

- Async task lost a captured argument

Definitions: [vm/src/machine.cpp:2897](../vm/src/machine.cpp#L2897), [vm/src/machine.cpp:2899](../vm/src/machine.cpp#L2899).

### WVM1168

- Async indirect-call result register is invalid

Definitions: [vm/src/machine.cpp:5545](../vm/src/machine.cpp#L5545).

### WVM1169

- Async call result register is invalid

Definitions: [vm/src/machine.cpp:6338](../vm/src/machine.cpp#L6338).

### WVM1170

- Async dynamic-call result register is invalid

Definitions: [vm/src/machine.cpp:6386](../vm/src/machine.cpp#L6386).

### WVM1171

- Coroutine suspension does not match its function state table

Definitions: [vm/src/machine.cpp:6412](../vm/src/machine.cpp#L6412).

### WVM1172

- Coroutine suspension state does not match its resume edge

Definitions: [vm/src/machine.cpp:6417](../vm/src/machine.cpp#L6417).

### WVM1173

- Coroutine await requires an async task operand

Definitions: [vm/src/machine.cpp:6426](../vm/src/machine.cpp#L6426).

### WVM1174

- Coroutine suspension carries an unsupported async operation

Definitions: [vm/src/machine.cpp:6502](../vm/src/machine.cpp#L6502).

### WVM1175

- Coroutine resume block does not exist

Definitions: [vm/src/machine.cpp:3036](../vm/src/machine.cpp#L3036), [vm/src/machine.cpp:6506](../vm/src/machine.cpp#L6506).

### WVM1176

- Coroutine resume has no matching suspended payload

Definitions: [vm/src/machine.cpp:6530](../vm/src/machine.cpp#L6530).

### WVM1177

- Coroutine resume result register is invalid

Definitions: [vm/src/machine.cpp:6532](../vm/src/machine.cpp#L6532).

### WVM1178

- Enum constant does not match its canonical type metadata

Definitions: [vm/src/machine.cpp:3776](../vm/src/machine.cpp#L3776).

### WVM1179

- Enum intrinsic has no available receiver or target type
- Enum intrinsic target metadata is invalid
- Enum intrinsic receiver is not an integer value

Definitions: [vm/src/machine.cpp:3792](../vm/src/machine.cpp#L3792), [vm/src/machine.cpp:3797](../vm/src/machine.cpp#L3797), [vm/src/machine.cpp:3809](../vm/src/machine.cpp#L3809).

### WVM1180

- Flagset intrinsic requires an integer mask operand

Definitions: [vm/src/machine.cpp:3844](../vm/src/machine.cpp#L3844).

### WVM1181

- Enum or flagset intrinsic is not implemented: {dynamic context}

Definitions: [vm/src/machine.cpp:3860](../vm/src/machine.cpp#L3860).

### WVM1182

- Any boxing requires an available payload

Definitions: [vm/src/machine.cpp:4419](../vm/src/machine.cpp#L4419).

### WVM1183

- Any test or cast requires a live boxed value

Definitions: [vm/src/machine.cpp:4482](../vm/src/machine.cpp#L4482).

### WVM1184

- Checked any cast cannot recover a payload from null
- Checked any cast failed for the requested target type
- Checked any cast cannot borrow a non-object payload

Definitions: [vm/src/machine.cpp:4439](../vm/src/machine.cpp#L4439), [vm/src/machine.cpp:4499](../vm/src/machine.cpp#L4499), [vm/src/machine.cpp:4507](../vm/src/machine.cpp#L4507).

### WVM1185

- Nullable wrapping requires an available payload

Definitions: [vm/src/machine.cpp:4520](../vm/src/machine.cpp#L4520).

### WVM1186

- Nullable unwrap requires an available value

Definitions: [vm/src/machine.cpp:4531](../vm/src/machine.cpp#L4531).

### WVM1187

- Nullable value does not contain a payload

Definitions: [vm/src/machine.cpp:4533](../vm/src/machine.cpp#L4533).

### WVM1188

- Variant operation requires canonical option/result storage

Definitions: [vm/src/machine.cpp:4549](../vm/src/machine.cpp#L4549).

### WVM1189

- Result does not contain a success value

Definitions: [vm/src/machine.cpp:4568](../vm/src/machine.cpp#L4568).

### WVM1190

- Variant payload is outside its canonical field layout

Definitions: [vm/src/machine.cpp:4571](../vm/src/machine.cpp#L4571).

### WVM1191

- Iterator creation result type is not an iterator

Definitions: [vm/src/machine.cpp:4583](../vm/src/machine.cpp#L4583).

### WVM1192

- Range iterator requires three compatible integer values

Definitions: [vm/src/machine.cpp:4591](../vm/src/machine.cpp#L4591).

### WVM1193

- Range iterator step cannot be zero

Definitions: [vm/src/machine.cpp:4595](../vm/src/machine.cpp#L4595).

### WVM1194

- Container iterator source does not match its selector
- Container iterator has an invalid operand shape

Definitions: [vm/src/machine.cpp:4606](../vm/src/machine.cpp#L4606), [vm/src/machine.cpp:4617](../vm/src/machine.cpp#L4617).

### WVM1195

- Container iterator step must be a positive integer

Definitions: [vm/src/machine.cpp:4613](../vm/src/machine.cpp#L4613).

### WVM1196

- Iterator selector is not implemented: {dynamic context}

Definitions: [vm/src/machine.cpp:4622](../vm/src/machine.cpp#L4622).

### WVM1197

- Iterator operation requires a live iterator value

Definitions: [vm/src/machine.cpp:4634](../vm/src/machine.cpp#L4634).

### WVM1198

- Iterator state is incompatible with its source
- Range iterator state is incomplete
- Range iterator state is not integer-compatible
- Container iterator source is unavailable

Definitions: [vm/src/machine.cpp:4689](../vm/src/machine.cpp#L4689), [vm/src/machine.cpp:4803](../vm/src/machine.cpp#L4803), [vm/src/machine.cpp:4830](../vm/src/machine.cpp#L4830), [vm/src/machine.cpp:4836](../vm/src/machine.cpp#L4836).

### WVM1199

- Range iterator has no current value
- Container iterator has no current value
- Iterator current value is unavailable

Definitions: [vm/src/machine.cpp:4704](../vm/src/machine.cpp#L4704), [vm/src/machine.cpp:4713](../vm/src/machine.cpp#L4713), [vm/src/machine.cpp:4768](../vm/src/machine.cpp#L4768).

### WVM1200

- Array iterator field projection is invalid

Definitions: [vm/src/machine.cpp:4740](../vm/src/machine.cpp#L4740), [vm/src/machine.cpp:4745](../vm/src/machine.cpp#L4745).

### WVM1201

- Dictionary iterator projection is invalid

Definitions: [vm/src/machine.cpp:4764](../vm/src/machine.cpp#L4764).

### WVM1202

- Result propagation requires a canonical error payload and target type
- Result propagation target has no canonical result layout
- Result propagation target fields are unavailable

Definitions: [vm/src/machine.cpp:6546](../vm/src/machine.cpp#L6546), [vm/src/machine.cpp:6551](../vm/src/machine.cpp#L6551), [vm/src/machine.cpp:6556](../vm/src/machine.cpp#L6556).

### WVM1204

- Async task failure lost its terminal-state race
- Async task completion lost its terminal-state race

Definitions: [vm/src/machine.cpp:2928](../vm/src/machine.cpp#L2928), [vm/src/machine.cpp:2948](../vm/src/machine.cpp#L2948).

### WVM1205

- VM execution was aborted by the debug observer

Definitions: [vm/src/machine.cpp:3333](../vm/src/machine.cpp#L3333).

### WVM1206

- VM debug observer threw across the runtime boundary

Definitions: [vm/src/machine.cpp:3326](../vm/src/machine.cpp#L3326).

### WVM1207

- VM executor owner dispatch failed
- VM executor continuation threw across the runtime boundary

Definitions: [vm/src/machine.cpp:2984](../vm/src/machine.cpp#L2984), [vm/src/machine.cpp:3116](../vm/src/machine.cpp#L3116), [vm/src/machine.cpp:3121](../vm/src/machine.cpp#L3121).

### WVM1208

- Await continuation executor rejected the resumed VM frame
- Coroutine executor switch has no runnable target
- Coroutine executor queue rejected its continuation

Definitions: [vm/src/machine.cpp:3056](../vm/src/machine.cpp#L3056), [vm/src/machine.cpp:6515](../vm/src/machine.cpp#L6515), [vm/src/machine.cpp:6519](../vm/src/machine.cpp#L6519).

### WVM1209

- Await continuation lost its VM frame
- Coroutine await has no owning async task

Definitions: [vm/src/machine.cpp:3026](../vm/src/machine.cpp#L3026), [vm/src/machine.cpp:6454](../vm/src/machine.cpp#L6454).

### WVM1210

- {dynamic context}VM destructor threw while unwinding: {dynamic context}
- VM destructor threw a non-standard exception while unwinding
- VM await continuation dispatch failed

Definitions: [vm/src/machine.cpp:1414](../vm/src/machine.cpp#L1414), [vm/src/machine.cpp:1419](../vm/src/machine.cpp#L1419), [vm/src/machine.cpp:6484](../vm/src/machine.cpp#L6484).

### WVM1211

- Dictionary mutation cleanup failed while running a destructor
- Reference cleanup failed while running a destructor
- Value cleanup failed while running a destructor
- Place cleanup failed while running a destructor
- Native callback cleanup failed while running a destructor
- Native copy-back cleanup failed while running a destructor

Definitions: [vm/src/machine.cpp:4405](../vm/src/machine.cpp#L4405), [vm/src/machine.cpp:4996](../vm/src/machine.cpp#L4996), [vm/src/machine.cpp:5377](../vm/src/machine.cpp#L5377), [vm/src/machine.cpp:5398](../vm/src/machine.cpp#L5398), [vm/src/machine.cpp:5420](../vm/src/machine.cpp#L5420), [vm/src/machine.cpp:6109](../vm/src/machine.cpp#L6109), [vm/src/machine.cpp:6201](../vm/src/machine.cpp#L6201).

### WVM1212

- Async argument cleanup failed while running a destructor

Definitions: [vm/src/machine.cpp:2934](../vm/src/machine.cpp#L2934), [vm/src/machine.cpp:3105](../vm/src/machine.cpp#L3105).

### WVM1220

- Native invocation references an invalid function declaration
- Native invocation target has no native binding metadata
- Native function is not registered: {dynamic context}

Definitions: [vm/src/machine.cpp:5621](../vm/src/machine.cpp#L5621), [vm/src/machine.cpp:5624](../vm/src/machine.cpp#L5624), [vm/src/machine.cpp:5635](../vm/src/machine.cpp#L5635).

### WVM1221

- Native invocation argument count does not match its ABI plan

Definitions: [vm/src/machine.cpp:5626](../vm/src/machine.cpp#L5626).

### WVM1222

- Native callable has caller-thread affinity
- Native callable failed with ABI status {dynamic context}
- Native invocation '{dynamic context}' failed: {dynamic context}

Definitions: [vm/src/machine.cpp:1991](../vm/src/machine.cpp#L1991), [vm/src/machine.cpp:2029](../vm/src/machine.cpp#L2029), [vm/src/machine.cpp:6210](../vm/src/machine.cpp#L6210).

### WVM1223

- Native invocation reads an unavailable argument value
- Native ref/view argument does not reference a VM place
- Native ref/view argument references unavailable or immutable storage
- Native argument conversion failed: {dynamic context}
- Aliased native references disagree on their ABI value type
- Native value argument references unavailable storage
- Native callback argument has an invalid callable ABI shape
- Native container projection has an incompatible value type
- Native container object projection has no contract identity
- Native projection key conversion failed: {dynamic context}
- Native projection key has no contract identity
- Native sequence projection has an incompatible element type
- Native sequence object projection has no contract identity
- Overlapping native field reference has an incompatible producer layout: {dynamic context} (field type {dynamic context}, argument type {dynamic context}, identity {dynamic context})
- Native fixed-array reference requires a containing POD projection

Definitions: [vm/src/machine.cpp:1984](../vm/src/machine.cpp#L1984), [vm/src/machine.cpp:3442](../vm/src/machine.cpp#L3442), [vm/src/machine.cpp:5179](../vm/src/machine.cpp#L5179), [vm/src/machine.cpp:5187](../vm/src/machine.cpp#L5187), [vm/src/machine.cpp:5201](../vm/src/machine.cpp#L5201), [vm/src/machine.cpp:5738](../vm/src/machine.cpp#L5738), [vm/src/machine.cpp:5745](../vm/src/machine.cpp#L5745), [vm/src/machine.cpp:5748](../vm/src/machine.cpp#L5748), [vm/src/machine.cpp:5768](../vm/src/machine.cpp#L5768), [vm/src/machine.cpp:5783](../vm/src/machine.cpp#L5783), [vm/src/machine.cpp:5799](../vm/src/machine.cpp#L5799), [vm/src/machine.cpp:5805](../vm/src/machine.cpp#L5805), [vm/src/machine.cpp:5844](../vm/src/machine.cpp#L5844), [vm/src/machine.cpp:5903](../vm/src/machine.cpp#L5903), [vm/src/machine.cpp:5912](../vm/src/machine.cpp#L5912), [vm/src/machine.cpp:5933](../vm/src/machine.cpp#L5933), [vm/src/machine.cpp:5938](../vm/src/machine.cpp#L5938), [vm/src/machine.cpp:5962](../vm/src/machine.cpp#L5962), [vm/src/machine.cpp:5971](../vm/src/machine.cpp#L5971), [vm/src/machine.cpp:6052](../vm/src/machine.cpp#L6052), [vm/src/machine.cpp:6080](../vm/src/machine.cpp#L6080).

### WVM1224

- Native any payload refresh failed: {dynamic context}
- Native any payload is unsupported or absent from its descriptor closure
- Native any shared cell changed its payload identity
- Native mutable-reference copy-back failed: {dynamic context}
- Native borrowed result has an invalid reference contract
- Native producer did not advertise projected borrowed results
- Native borrowed result does not name a compatible caller reference
- Native borrowed result register is invalid
- Native async result register is invalid
- Native result conversion failed: {dynamic context}
- Native result register is invalid

Definitions: [vm/src/machine.cpp:4467](../vm/src/machine.cpp#L4467), [vm/src/machine.cpp:4469](../vm/src/machine.cpp#L4469), [vm/src/machine.cpp:4473](../vm/src/machine.cpp#L4473), [vm/src/machine.cpp:6214](../vm/src/machine.cpp#L6214), [vm/src/machine.cpp:6237](../vm/src/machine.cpp#L6237), [vm/src/machine.cpp:6242](../vm/src/machine.cpp#L6242), [vm/src/machine.cpp:6278](../vm/src/machine.cpp#L6278), [vm/src/machine.cpp:6281](../vm/src/machine.cpp#L6281), [vm/src/machine.cpp:6301](../vm/src/machine.cpp#L6301), [vm/src/machine.cpp:6313](../vm/src/machine.cpp#L6313), [vm/src/machine.cpp:6316](../vm/src/machine.cpp#L6316).

### WVM1225

- Native async result has no registered task bridge
- Invocation-scoped native callback borrows cannot be captured
- Native callback environment contains a borrowed capture
- Native producer does not advertise consumed argument ownership
- Native callbacks require scalar, string, text, qualified native POD, hosted object/interface, registered container or qualified value ref/view arguments; unsupported callback type #{dynamic context}
- Overlapping native container references require qualified projections (map keys, bool proxies or unsupported field path)
- Native producer does not support dictionary/container projections
- Native producer does not support sequence reference projections
- Native producer does not support sequence/POD field projections
- Native object/field aliasing requires synchronized projections
- Native producer does not support overlapping POD reference projections
- Native async result requires a registered task bridge and one payload
- Native async payload cannot be represented by the VM ABI bridge

Definitions: [vm/src/machine.cpp:1127](../vm/src/machine.cpp#L1127), [vm/src/machine.cpp:5462](../vm/src/machine.cpp#L5462), [vm/src/machine.cpp:5496](../vm/src/machine.cpp#L5496), [vm/src/machine.cpp:5740](../vm/src/machine.cpp#L5740), [vm/src/machine.cpp:5818](../vm/src/machine.cpp#L5818), [vm/src/machine.cpp:5829](../vm/src/machine.cpp#L5829), [vm/src/machine.cpp:5877](../vm/src/machine.cpp#L5877), [vm/src/machine.cpp:5901](../vm/src/machine.cpp#L5901), [vm/src/machine.cpp:5957](../vm/src/machine.cpp#L5957), [vm/src/machine.cpp:5959](../vm/src/machine.cpp#L5959), [vm/src/machine.cpp:6015](../vm/src/machine.cpp#L6015), [vm/src/machine.cpp:6046](../vm/src/machine.cpp#L6046), [vm/src/machine.cpp:6290](../vm/src/machine.cpp#L6290), [vm/src/machine.cpp:6294](../vm/src/machine.cpp#L6294).

### WVM1226

- Native task completion subscription failed with ABI status {dynamic context}
- Native task completion raised an internal bridge failure
- Native task completion failed with ABI status {dynamic context}
- Native task result failed: {dynamic context}
- Native task result conversion failed: {dynamic context}
- Native task cancellation failed with ABI status {dynamic context}

Definitions: [vm/src/machine.cpp:1155](../vm/src/machine.cpp#L1155), [vm/src/machine.cpp:1178](../vm/src/machine.cpp#L1178), [vm/src/machine.cpp:1213](../vm/src/machine.cpp#L1213), [vm/src/machine.cpp:1232](../vm/src/machine.cpp#L1232), [vm/src/machine.cpp:1245](../vm/src/machine.cpp#L1245), [vm/src/machine.cpp:1281](../vm/src/machine.cpp#L1281).

### WVM1240

- Generic declaration requires a concrete function specialization

Definitions: [vm/src/machine.cpp:1640](../vm/src/machine.cpp#L1640), [vm/src/machine.cpp:3159](../vm/src/machine.cpp#L3159).

### WVM1241

Definition: VM object-layout indexing failed while loading the verified bytecode module. The detailed layout message is supplied by ObjectLayouts::build.

Detailed message: dynamically supplied by the reporting path.
Message expression: `std::move(layoutError)`.

Definitions: [vm/src/machine.cpp:1547](../vm/src/machine.cpp#L1547).

## Native, SDK and std status/error enums

Values below are current implementation values. Only explicitly ABI-versioned enums are wire contracts; do not rely on incidental SDK exception ordinals as a stable ABI. OS native codes retain their platform meaning and user Result codes retain their author-defined domain.

`WioResultError` carries domain, portable code, native OS code and message separately. The same integer in two domains is not the same error. `none`/`Ok` denotes success/no error; `Pending`/`NotReady`/`TimedOut` is task state or an observation outcome and must not be conflated with a faulted payload. ConsoleErrorDomain and WioResultDomain classify provenance, not failure reasons.

### ErrorCode — sdk/include/wio_sdk.h

Source: [sdk/include/wio_sdk.h](../sdk/include/wio_sdk.h#L87).

| Name | Value | Meaning / category |
| --- | ---: | --- |
| `InvalidArgument` | 0 | The caller supplied an invalid argument to an SDK operation. |
| `LibraryOpenFailed` | 1 | The platform loader could not open the requested module library. |
| `SymbolLookupFailed` | 2 | A required exported symbol could not be resolved in the library. |
| `ApiUnavailable` | 3 | The requested module API or optional sidecar capability is unavailable. |
| `InvalidApiDescriptor` | 4 | Module/sidecar descriptor version, size or required pointers are invalid. |
| `ExportNotFound` | 5 | The requested exported entry is absent. |
| `TypeNotFound` | 6 | The requested exported/reflected type is absent. |
| `CommandNotFound` | 7 | The requested exported command is absent. |
| `EventNotFound` | 8 | The requested exported event is absent. |
| `EventHookNotFound` | 9 | The requested event hook is absent. |
| `FieldNotFound` | 10 | The requested field descriptor is absent. |
| `MethodNotFound` | 11 | The requested callable method descriptor is absent. |
| `FieldNotWritable` | 12 | The requested field is readonly or has no writable bridge. |
| `SignatureMismatch` | 13 | Requested type/signature/shape differs from the advertised bridge contract. |
| `InvokeFailed` | 14 | The exported/native invocation failed or returned an invalid result. |
| `LifecycleFailed` | 15 | A module/application lifecycle operation failed. |
| `StaleBinding` | 16 | A binding belongs to a retired/unloaded generation and cannot be used. |
| `ReloadFailed` | 17 | Staging or replacing the module during reload failed. |
| `IoFailure` | 18 | An SDK file/I/O operation failed. |

### WioNativeAbiStatus — sdk/include/wio_native_abi.h

Source: [sdk/include/wio_native_abi.h](../sdk/include/wio_native_abi.h#L12).

| Name | Value | Meaning / category |
| --- | ---: | --- |
| `WIO_NATIVE_ABI_OK` | 0 | ok |
| `WIO_NATIVE_ABI_INVALID_ARGUMENT` | 1 | invalid argument |
| `WIO_NATIVE_ABI_TYPE_MISMATCH` | 2 | type mismatch |
| `WIO_NATIVE_ABI_EXCEPTION` | 3 | exception |
| `WIO_NATIVE_ABI_PANIC` | 4 | panic |
| `WIO_NATIVE_ABI_WRONG_THREAD` | 5 | wrong thread |
| `WIO_NATIVE_ABI_STALE_HANDLE` | 6 | stale handle |
| `WIO_NATIVE_ABI_NOT_READY` | 7 | not ready |
| `WIO_NATIVE_ABI_CANCELLED` | 8 | cancelled |

### WioInvokeStatus — sdk/include/module_api.h

Source: [sdk/include/module_api.h](../sdk/include/module_api.h#L179).

| Name | Value | Meaning / category |
| --- | ---: | --- |
| `WIO_INVOKE_OK` | 0 | ok |
| `WIO_INVOKE_EXPORT_NOT_FOUND` | 1 | export not found |
| `WIO_INVOKE_BAD_ARGUMENTS` | 2 | bad arguments |
| `WIO_INVOKE_TYPE_MISMATCH` | 3 | type mismatch |
| `WIO_INVOKE_RESULT_REQUIRED` | 4 | result required |
| `WIO_INVOKE_NOT_CALLABLE` | 5 | not callable |

### WioCallbackStatus — sdk/include/module_api.h

Source: [sdk/include/module_api.h](../sdk/include/module_api.h#L473).

| Name | Value | Meaning / category |
| --- | ---: | --- |
| `WIO_CALLBACK_OK` | 0 | ok |
| `WIO_CALLBACK_BAD_ARGUMENTS` | 1 | bad arguments |
| `WIO_CALLBACK_TYPE_MISMATCH` | 2 | type mismatch |
| `WIO_CALLBACK_RESULT_REQUIRED` | 3 | result required |
| `WIO_CALLBACK_FAULTED` | 4 | faulted |

### WioApplicationStatus — sdk/include/module_api.h

Source: [sdk/include/module_api.h](../sdk/include/module_api.h#L536).

| Name | Value | Meaning / category |
| --- | ---: | --- |
| `WIO_APPLICATION_OK` | 0 | ok |
| `WIO_APPLICATION_EXIT_REQUESTED` | 1 | exit requested |
| `WIO_APPLICATION_ALREADY_CLOSED` | 2 | already closed |
| `WIO_APPLICATION_INVALID_STATE` | 3 | invalid state |
| `WIO_APPLICATION_FAULTED` | 4 | faulted |
| `WIO_APPLICATION_WRONG_THREAD` | 5 | wrong thread |

### WioAsyncTaskStatus — sdk/include/module_api.h

Source: [sdk/include/module_api.h](../sdk/include/module_api.h#L608).

| Name | Value | Meaning / category |
| --- | ---: | --- |
| `WIO_ASYNC_TASK_PENDING` | 0 | pending |
| `WIO_ASYNC_TASK_READY` | 1 | ready |
| `WIO_ASYNC_TASK_CANCELLED` | 2 | cancelled |
| `WIO_ASYNC_TASK_FAULTED` | 3 | faulted |

### WioAsyncOperationStatus — sdk/include/module_api.h

Source: [sdk/include/module_api.h](../sdk/include/module_api.h#L616).

| Name | Value | Meaning / category |
| --- | ---: | --- |
| `WIO_ASYNC_OK` | 0 | ok |
| `WIO_ASYNC_TIMED_OUT` | 1 | timed out |
| `WIO_ASYNC_NOT_READY` | 2 | not ready |
| `WIO_ASYNC_CANCELLED` | 3 | cancelled |
| `WIO_ASYNC_FAULTED` | 4 | faulted |
| `WIO_ASYNC_BAD_ARGUMENTS` | 5 | bad arguments |
| `WIO_ASYNC_TYPE_MISMATCH` | 6 | type mismatch |

### WioResultDomain — sdk/include/wio_values.h

Source: [sdk/include/wio_values.h](../sdk/include/wio_values.h#L301).

| Name | Value | Meaning / category |
| --- | ---: | --- |
| `None` | 0 | none |
| `Console` | 1 | console |
| `Io` | 2 | io |
| `FileSystem` | 3 | file system |
| `Runtime` | 4 | runtime |
| `Custom` | 5 | custom |

### WriteError — runtime/include/io_errors.h

Source: [runtime/include/io_errors.h](../runtime/include/io_errors.h#L6).

| Name | Value | Meaning / category |
| --- | ---: | --- |
| `none` | 0 | none |
| `null_file` | 1 | null file |
| `null_data` | 2 | null data |
| `size_overflow` | 3 | size overflow |
| `io_error` | 4 | io error |
| `partial_write` | 5 | partial write |

### ReadError — runtime/include/io_errors.h

Source: [runtime/include/io_errors.h](../runtime/include/io_errors.h#L16).

| Name | Value | Meaning / category |
| --- | ---: | --- |
| `none` | 0 | none |
| `null_file` | 1 | null file |
| `invalid_argument` | 2 | invalid argument |
| `eof` | 3 | eof |
| `io_error` | 4 | io error |
| `partial_read` | 5 | partial read |
| `platform_error` | 6 | platform error |

### ParseError — runtime/include/std_convert.h

Source: [runtime/include/std_convert.h](../runtime/include/std_convert.h#L10).

| Name | Value | Meaning / category |
| --- | ---: | --- |
| `none` | 0 | none |
| `empty` | 1 | empty |
| `invalid_format` | 2 | invalid format |
| `trailing_characters` | 3 | trailing characters |
| `out_of_range` | 4 | out of range |
| `invalid_base` | 5 | invalid base |

### ErrorCode — runtime/include/std_fs.h

Source: [runtime/include/std_fs.h](../runtime/include/std_fs.h#L12).

| Name | Value | Meaning / category |
| --- | ---: | --- |
| `None` | 0 | none |
| `InvalidPath` | 1 | invalid path |
| `NotFound` | 2 | not found |
| `AlreadyExists` | 3 | already exists |
| `NotDirectory` | 4 | not directory |
| `IsDirectory` | 5 | is directory |
| `PermissionDenied` | 6 | permission denied |
| `Io` | 7 | io |
| `Unsupported` | 8 | unsupported |
| `Unknown` | 9 | unknown |

### FileError — runtime/include/std_io.h

Source: [runtime/include/std_io.h](../runtime/include/std_io.h#L128).

| Name | Value | Meaning / category |
| --- | ---: | --- |
| `none` | 0 | none |
| `null_path` | 1 | null path |
| `empty_path` | 2 | empty path |
| `invalid_path` | 3 | invalid path |
| `invalid_utf8` | 4 | invalid utf8 |
| `allocation_failed` | 5 | allocation failed |
| `invalid_argument` | 6 | invalid argument |
| `invalid_mode` | 7 | invalid mode |
| `unsupported_mode` | 8 | unsupported mode |
| `already_open` | 9 | already open |
| `not_open` | 10 | not open |
| `open_failed` | 11 | open failed |
| `close_failed` | 12 | close failed |
| `sync_failed` | 13 | sync failed |
| `seek_failed` | 14 | seek failed |
| `tell_failed` | 15 | tell failed |
| `size_failed` | 16 | size failed |

### ProcessError — runtime/include/std_process.h

Source: [runtime/include/std_process.h](../runtime/include/std_process.h#L10).

| Name | Value | Meaning / category |
| --- | ---: | --- |
| `none` | 0 | none |
| `empty_program` | 1 | empty program |
| `invalid_working_directory` | 2 | invalid working directory |
| `launch_failed` | 3 | launch failed |
| `pipe_failed` | 4 | pipe failed |
| `process_closed` | 5 | process closed |
| `wait_failed` | 6 | wait failed |
| `terminate_failed` | 7 | terminate failed |
| `cancelled` | 8 | cancelled |

### RegexError — runtime/include/std_regex.h

Source: [runtime/include/std_regex.h](../runtime/include/std_regex.h#L10).

| Name | Value | Meaning / category |
| --- | ---: | --- |
| `none` | 0 | none |
| `invalid_pattern` | 1 | invalid pattern |
| `runtime_error` | 2 | runtime error |
| `limit_exceeded` | 3 | limit exceeded |

### ConsoleStatus — runtime/include/std_console.h

Source: [runtime/include/std_console.h](../runtime/include/std_console.h#L28).

| Name | Value | Meaning / category |
| --- | ---: | --- |
| `Ok` | 0 | ok |
| `Unsupported` | 1 | unsupported |
| `Unavailable` | 2 | unavailable |
| `InvalidArgument` | 3 | invalid argument |
| `InvalidRange` | 4 | invalid range |
| `NullArgument` | 5 | null argument |
| `InputRedirected` | 6 | input redirected |
| `OutputRedirected` | 7 | output redirected |
| `ErrorRedirected` | 8 | error redirected |
| `IoError` | 9 | io error |
| `PartialIo` | 10 | partial io |
| `EndOfFile` | 11 | end of file |
| `EncodingError` | 12 | encoding error |
| `FormatError` | 13 | format error |
| `PermissionDenied` | 14 | permission denied |
| `PlatformError` | 15 | platform error |
| `UnknownError` | 16 | unknown error |

### ConsoleErrorDomain — runtime/include/std_console.h

Source: [runtime/include/std_console.h](../runtime/include/std_console.h#L49).

| Name | Value | Meaning / category |
| --- | ---: | --- |
| `None` | 0 | none |
| `Generic` | 1 | generic |
| `Stdio` | 2 | stdio |
| `Win32` | 3 | win32 |
| `Posix` | 4 | posix |
| `Terminal` | 5 | terminal |
| `Formatting` | 6 | formatting |
